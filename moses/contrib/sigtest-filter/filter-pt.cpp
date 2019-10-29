
#include <cstring> 
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

#include "_SuffixArraySearchApplicationBase.h"

#include <vector>
#include <iostream>
#include <set>

#ifdef WIN32
#include "WIN32_functions.h"
#else
#include <unistd.h>
#endif

typedef std::vector<TextLenType> SentIdSet;
typedef std::pair<SentIdSet, clock_t> ClockedSentIdSet;
typedef std::map<std::string, ClockedSentIdSet> PhraseSetMap;

#undef min

// constants
const size_t MINIMUM_SIZE_TO_KEEP = 10000;     // increase this to improve memory usage,
// reduce for speed
const std::string SEPARATOR       = " ||| ";

const double ALPHA_PLUS_EPS  = -1000.0;        // dummy value
const double ALPHA_MINUS_EPS = -2000.0;        // dummy value

// configuration params
int pfe_filter_limit = 0;               // 0 = don't filter anything based on P(f|e)
bool print_cooc_counts = false;         // add cooc counts to phrase table?
bool print_neglog_significance = false; // add -log(p) to phrase table?
double sig_filter_limit = 0;            // keep phrase pairs with -log(sig) > sig_filter_limit
//    higher = filter-more
bool pef_filter_only = false;           // only filter based on pef
bool hierarchical = false;
int max_cache = 0;

// globals
PhraseSetMap esets;
PhraseSetMap fsets;
double p_111 = 0.0;                     // alpha
size_t nremoved_sigfilter = 0;
size_t nremoved_pfefilter = 0;

C_SuffixArraySearchApplicationBase e_sa;
C_SuffixArraySearchApplicationBase f_sa;
int num_lines;

void usage()
{
  std::cerr << "\nFilter phrase table using significance testing as described\n"
            << "in H. Johnson, et al. (2007) Improving Translation Quality\n"
            << "by Discarding Most of the Phrasetable. EMNLP 2007.\n"
            << "\nUsage:\n"
            << "\n  filter-pt -e english.suf-arr -f french.suf-arr\n"
            << "      [-c] [-p] [-l threshold] [-n num] < PHRASE-TABLE > FILTERED-PHRASE-TABLE\n\n"
            << "   [-l threshold] >0.0, a+e, or a-e: keep values that have a -log significance > this\n"
            << "   [-n num      ] 0, 1...: 0=no filtering, >0 sort by P(e|f) and keep the top num elements\n"
            << "   [-c          ] add the cooccurence counts to the phrase table\n"
            << "   [-p          ] add -log(significance) to the phrasetable\n"
            << "   [-h          ] filter hierarchical rule table\n"
            << "   [-m num      ] limit cache to num most recent phrases\n";
  exit(1);
}

struct PTEntry {
  PTEntry(const std::string& str, int index);
  std::string f_phrase;
  std::string e_phrase;
  std::string extra;
  std::string scores;
  float pfe;
  int cf;
  int ce;
  int cfe;
  float nlog_pte;
  void set_cooc_stats(int _cef, int _cf, int _ce, float nlp) {
    cfe = _cef;
    cf = _cf;
    ce = _ce;
    nlog_pte = nlp;
  }

};

PTEntry::PTEntry(const std::string& str, int index) :
  cf(0), ce(0), cfe(0), nlog_pte(0.0)
{
  size_t pos = 0;
  std::string::size_type nextPos = str.find(SEPARATOR, pos);
  this->f_phrase = str.substr(pos,nextPos);

  pos = nextPos + SEPARATOR.size();
  nextPos = str.find(SEPARATOR, pos);
  this->e_phrase = str.substr(pos,nextPos-pos);

  pos = nextPos + SEPARATOR.size();
  nextPos = str.find(SEPARATOR, pos);
  if (nextPos < str.size()) {
    this->scores = str.substr(pos,nextPos-pos);

    pos = nextPos + SEPARATOR.size();
    this->extra = str.substr(pos);
  }
  else {
    this->scores = str.substr(pos,str.size()-pos);
  }

  int c = 0;
  std::string::iterator i=scores.begin();
  if (index > 0) {
    for (; i != scores.end(); ++i) {
      if ((*i) == ' ') {
        c++;
        if (c == index) break;
      }
    }
  }
  if (i != scores.end()) {
    ++i;
  }
  char f[24];
  char *fp=f;
  while (i != scores.end() && *i != ' ') {
    *fp++=*i++;
  }
  *fp++=0;

  this->pfe = atof(f);

  // std::cerr << "L: " << f_phrase << " ::: " << e_phrase << " ::: " << scores << " ::: " << pfe << std::endl;
  // std::cerr << "X: " << extra << "\n";
}

struct PfeComparer {
  bool operator()(const PTEntry* a, const PTEntry* b) const {
    return a->pfe > b->pfe;
  }
};

struct NlogSigThresholder {
  NlogSigThresholder(float threshold) : t(threshold) {}
  float t;
  bool operator()(const PTEntry* a) const {
    if (a->nlog_pte < t) {
      delete a;
      return true;
    } else return false;
  }
};

std::ostream& operator << (std::ostream& os, const PTEntry& pp)
{
  os << pp.f_phrase << " ||| " << pp.e_phrase;
  os << " ||| " << pp.scores;
  if (pp.extra.size()>0) os << " ||| " << pp.extra;
  if (print_cooc_counts) os << " ||| " << pp.cfe << " " << pp.cf << " " << pp.ce;
  if (print_neglog_significance) os << " ||| " << pp.nlog_pte;
  return os;
}

void print(int a, int b, int c, int d, float p)
{
  std::cerr << a << "\t" << b << "\t P=" << p << "\n"
            << c << "\t" << d << "\t xf=" << (double)(b)*(double)(c)/(double)(a+1)/(double)(d+1) << "\n\n";
}

// 2x2 (one-sided) Fisher's exact test
// see B. Moore. (2004) On Log Likelihood and the Significance of Rare Events
double fisher_exact(int cfe, int ce, int cf)
{
  assert(cfe <= ce);
  assert(cfe <= cf);

  int a = cfe;
  int b = (cf - cfe);
  int c = (ce - cfe);
  int d = (num_lines - ce - cf + cfe);
  int n = a + b + c + d;

  double cp = exp(lgamma(1+a+c) + lgamma(1+b+d) + lgamma(1+a+b) + lgamma(1+c+d) - lgamma(1+n) - lgamma(1+a) - lgamma(1+b) - lgamma(1+c) - lgamma(1+d));
  double total_p = 0.0;
  int tc = std::min(b,c);
  for (int i=0; i<=tc; i++) {
    total_p += cp;
//      double lg = lgamma(1+a+c) + lgamma(1+b+d) + lgamma(1+a+b) + lgamma(1+c+d) - lgamma(1+n) - lgamma(1+a) - lgamma(1+b) - lgamma(1+c) - lgamma(1+d); double cp = exp(lg);
//      print(a,b,c,d,cp);
    double coef = (double)(b)*(double)(c)/(double)(a+1)/(double)(d+1);
    cp *= coef;
    ++a;
    --c;
    ++d;
    --b;
  }
  return total_p;
}

template <class setType>
setType ordered_set_intersect(setType & set_1, setType & set_2)
{
    setType set_out;
    std::set_intersection(set_1.begin(), set_1.end(), set_2.begin(), set_2.end(), inserter(set_out,set_out.begin()) );
    return set_out;
}


SentIdSet lookup_phrase(const std::string & phrase, C_SuffixArraySearchApplicationBase & my_sa)
{
    SentIdSet occur_set;
    vector<S_SimplePhraseLocationElement> locations;

    locations = my_sa.locateExactPhraseInCorpus(phrase.c_str());
    if(locations.size()==0) {
        cerr<<"No occurrences found!!\n";
    }
    for (vector<S_SimplePhraseLocationElement>::iterator i=locations.begin(); i != locations.end(); ++i) {
        occur_set.push_back(i->sentIdInCorpus);
    }
    
    std::sort(occur_set.begin(), occur_set.end());
    SentIdSet::iterator it = std::unique(occur_set.begin(), occur_set.end());
    occur_set.resize(it - occur_set.begin());
    
    return occur_set;
}


// slight simplicifaction: we consider all sentences in which "a" and "b" occur to be instances of the rule "a [X][X] b".
SentIdSet lookup_multiple_phrases(vector<std::string> & phrases, C_SuffixArraySearchApplicationBase & my_sa, const std::string & rule, PhraseSetMap & cache) 
{

    if (phrases.size() == 1) {
        return lookup_phrase(phrases.front(), my_sa);
    }

    else {
        SentIdSet main_set;
        ClockedSentIdSet & clocked_first_set = cache[phrases.front()];
        SentIdSet & first_set = clocked_first_set.first;
        clocked_first_set.second = clock();
        
        bool first = true;
        if (first_set.empty()) {
            first_set = lookup_phrase(phrases.front(), my_sa);
        }
        for (vector<std::string>::iterator phrase=phrases.begin()+1; phrase != phrases.end(); ++phrase) {
            ClockedSentIdSet & clocked_temp_set = cache[*phrase];
            SentIdSet & temp_set = clocked_temp_set.first;
            clocked_temp_set.second = clock();
            
            if (temp_set.empty()) {
                temp_set = lookup_phrase(*phrase, my_sa);
            }
            if (first) {
                main_set = ordered_set_intersect(first_set,temp_set);
                first = false;
            }
            else {
                main_set = ordered_set_intersect(main_set,temp_set);
            }
            if (temp_set.size() < MINIMUM_SIZE_TO_KEEP) {
                cache.erase(*phrase);
            }
        }

        if (first_set.size() < MINIMUM_SIZE_TO_KEEP) {
            cache.erase(phrases.front());
        }

        return main_set;
    }
}


SentIdSet find_occurrences(const std::string& rule, C_SuffixArraySearchApplicationBase & my_sa, PhraseSetMap & cache)
{
    SentIdSet sa_set;

    // we search for hierarchical rules by stripping away NT and looking for terminals sequences
    // if a rule contains multiple sequences of terminals, we intersect their occurrences.
    if (hierarchical) {
        //   std::cerr << "splitting up phrase: " << phrase << "\n";
        int pos = 0;
        int endPos = 0;
        vector<std::string> phrases;

        while (rule.find("[X][X] ", pos) < rule.size()) {
            endPos = rule.find("[X][X] ",pos) - 1; // -1 to cut space before NT
            if (endPos < pos) { // no space: NT at start of rule (or two consecutive NTs)
                pos += 7; 
                continue;
            }
            phrases.push_back(rule.substr(pos,endPos-pos));
            pos = endPos + 8;
        }

        // cut LHS of rule
        endPos = rule.size()-4;
        if (endPos > pos) {
            phrases.push_back(rule.substr(pos,endPos-pos));
        }
        sa_set = lookup_multiple_phrases(phrases, my_sa, rule, cache);
    }
    else {
        sa_set = lookup_phrase(rule, my_sa);
    }
  return sa_set;
}


// input: unordered list of translation options for a single source phrase
void compute_cooc_stats_and_filter(std::vector<PTEntry*>& options)
{
  if (pfe_filter_limit>0 && options.size() > pfe_filter_limit) {
    nremoved_pfefilter += (options.size() - pfe_filter_limit);
    std::nth_element(options.begin(), options.begin()+pfe_filter_limit, options.end(), PfeComparer());
    for (std::vector<PTEntry*>::iterator i=options.begin()+pfe_filter_limit; i != options.end(); ++i)
      delete *i;
    options.erase(options.begin()+pfe_filter_limit,options.end());
  }
  if (pef_filter_only) return;
//   std::cerr << "f phrase: " << options.front()->f_phrase << "\n";
  SentIdSet fset;
  fset = find_occurrences(options.front()->f_phrase, f_sa, fsets);
  size_t cf = fset.size();
  for (std::vector<PTEntry*>::iterator i=options.begin(); i != options.end(); ++i) {
    const std::string& e_phrase = (*i)->e_phrase;
    size_t cef=0;
    ClockedSentIdSet& clocked_eset = esets[e_phrase];
    SentIdSet & eset = clocked_eset.first;
    clocked_eset.second = clock();
    if (eset.empty()) {
        eset = find_occurrences(e_phrase, e_sa, esets);
        //std::cerr << "Looking up e-phrase: " << e_phrase << "\n";
    }
    size_t ce=eset.size();
    if (ce < cf) {
      for (SentIdSet::iterator i=eset.begin(); i != eset.end(); ++i) {
        if (std::binary_search(fset.begin(), fset.end(), *i)) cef++;
      }
    } else {
      for (SentIdSet::iterator i=fset.begin(); i != fset.end(); ++i) {
        if (std::binary_search(eset.begin(), eset.end(), *i)) cef++;
      }
    }
    double nlp = -log(fisher_exact(cef, cf, ce));
    (*i)->set_cooc_stats(cef, cf, ce, nlp);
    if (ce < MINIMUM_SIZE_TO_KEEP) {
      esets.erase(e_phrase);
    }

  }
  std::vector<PTEntry*>::iterator new_end =
    std::remove_if(options.begin(), options.end(), NlogSigThresholder(sig_filter_limit));
  nremoved_sigfilter += (options.end() - new_end);
  options.erase(new_end,options.end());
}

void prune_cache(PhraseSetMap & psm) {
  if(max_cache && psm.size() > max_cache) {
    std::vector<clock_t> clocks;
    for(PhraseSetMap::iterator it = psm.begin(); it != psm.end(); it++) 
      clocks.push_back(it->second.second);
    
    std::sort(clocks.begin(), clocks.end());
    clock_t out = clocks[psm.size()-max_cache];
    
    for(PhraseSetMap::iterator it = psm.begin(); it != psm.end(); it++)
      if(it->second.second < out)
        psm.erase(it);
  }
}

int main(int argc, char * argv[])
{
  int c;
  const char* efile=0;
  const char* ffile=0;
  int pfe_index = 2;
  while ((c = getopt(argc, argv, "cpf:e:i:n:l:m:h")) != -1) {
    switch (c) {
    case 'e':
      efile = optarg;
      break;
    case 'f':
      ffile = optarg;
      break;
    case 'i':  // index of pfe in phrase table
      pfe_index = atoi(optarg);
      break;
    case 'n':  // keep only the top n entries in phrase table sorted by p(f|e) (0=all)
      pfe_filter_limit = atoi(optarg);
      std::cerr << "P(f|e) filter limit: " << pfe_filter_limit << std::endl;
      break;
    case 'c':
      print_cooc_counts = true;
      break;
    case 'p':
      print_neglog_significance = true;
      break;
    case 'h':
      hierarchical = true;
      break;
    case 'm':
      max_cache = atoi(optarg);
      break;
    case 'l':
      std::cerr << "-l = " << optarg << "\n";
      if (strcmp(optarg,"a+e") == 0) {
        sig_filter_limit = ALPHA_PLUS_EPS;
      } else if (strcmp(optarg,"a-e") == 0) {
        sig_filter_limit = ALPHA_MINUS_EPS;
      } else {
        char *x;
        sig_filter_limit = strtod(optarg, &x);
        if (sig_filter_limit < 0.0) {
          std::cerr << "Filter limit (-l) must be either 'a+e', 'a-e' or a real number >= 0.0\n";
          usage();
        }
      }
      break;
    default:
      usage();
    }
  }
  if (sig_filter_limit == 0.0) pef_filter_only = true;
  //-----------------------------------------------------------------------------
  if (optind != argc || ((!efile || !ffile) && !pef_filter_only)) {
    usage();
  }

  //load the indexed corpus with vocabulary(noVoc=false) and with offset(noOffset=false)
  if (!pef_filter_only) {
    e_sa.loadData_forSearch(efile, false, false);
    f_sa.loadData_forSearch(ffile, false, false);
    size_t elines = e_sa.returnTotalSentNumber();
    size_t flines = f_sa.returnTotalSentNumber();
    if (elines != flines) {
      std::cerr << "Number of lines in e-corpus != number of lines in f-corpus!\n";
      usage();
    } else {
      std::cerr << "Training corpus: " << elines << " lines\n";
      num_lines = elines;
    }
    p_111 = -log(fisher_exact(1,1,1));
    std::cerr << "\\alpha = " << p_111 << "\n";
    if (sig_filter_limit == ALPHA_MINUS_EPS) {
      sig_filter_limit = p_111 - 0.001;
    } else if (sig_filter_limit == ALPHA_PLUS_EPS) {
      sig_filter_limit = p_111 + 0.001;
    }
    std::cerr << "Sig filter threshold is = " << sig_filter_limit << "\n";
  } else {
    std::cerr << "Filtering using P(e|f) only. n=" << pfe_filter_limit << std::endl;
  }

  char tmpString[10000];
  std::string prev = "";
  std::vector<PTEntry*> options;
  size_t pt_lines = 0;
  while(!cin.eof()) {
    cin.getline(tmpString,10000,'\n');
    if(++pt_lines%10000==0) { 
      std::cerr << ".";
      
      prune_cache(esets);
      prune_cache(fsets);
      
      if(pt_lines%500000==0) 
        std::cerr << "[n:"<<pt_lines<<"]\n";
    }

    if(strlen(tmpString)>0) {
      PTEntry* pp = new PTEntry(tmpString, pfe_index);
      if (prev != pp->f_phrase) {
        prev = pp->f_phrase;

        if (!options.empty()) {  // always true after first line
          compute_cooc_stats_and_filter(options);
        }
        for (std::vector<PTEntry*>::iterator i=options.begin(); i != options.end(); ++i) {
          std::cout << **i << std::endl;
          delete *i;
        }
        options.clear();
        options.push_back(pp);

      } else {
        options.push_back(pp);
      }
      //			  for(int i=0;i<locations.size(); i++){
      //				  cout<<"SentId="<<locations[i].sentIdInCorpus<<" Pos="<<(int)locations[i].posInSentInCorpus<<endl;
      //			  }
    }
  }
  compute_cooc_stats_and_filter(options);
  for (std::vector<PTEntry*>::iterator i=options.begin(); i != options.end(); ++i) {
    std::cout << **i << std::endl;
    delete *i;
  }
  float pfefper = (100.0*(float)nremoved_pfefilter)/(float)pt_lines;
  float sigfper = (100.0*(float)nremoved_sigfilter)/(float)pt_lines;
  std::cerr << "\n\n------------------------------------------------------\n"
            << "  unfiltered phrases pairs: " << pt_lines << "\n"
            << "\n"
            << "     P(f|e) filter [first]: " << nremoved_pfefilter << "   (" << pfefper << "%)\n"
            << "       significance filter: " << nremoved_sigfilter << "   (" << sigfper << "%)\n"
            << "            TOTAL FILTERED: " << (nremoved_pfefilter + nremoved_sigfilter) << "   (" << (sigfper + pfefper) << "%)\n"
            << "\n"
            << "     FILTERED phrase pairs: " << (pt_lines - nremoved_pfefilter - nremoved_sigfilter) << "   (" << (100.0-sigfper - pfefper) << "%)\n"
            << "------------------------------------------------------\n";

  return 0;
}
