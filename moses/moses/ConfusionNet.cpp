// $Id$

#include "ConfusionNet.h"
#include <sstream>

#include "FactorCollection.h"
#include "Util.h"
#include "moses/TranslationModel/PhraseDictionaryTreeAdaptor.h"
#include "TranslationOptionCollectionConfusionNet.h"
#include "StaticData.h"
#include "Sentence.h"
#include "UserMessage.h"

namespace Moses
{
struct CNStats {
  size_t created,destr,read,colls,words;

  CNStats() : created(0),destr(0),read(0),colls(0),words(0) {}
  ~CNStats() {
    print(std::cerr);
  }

  void createOne() {
    ++created;
  }
  void destroyOne() {
    ++destr;
  }

  void collect(const ConfusionNet& cn) {
    ++read;
    colls+=cn.GetSize();
    for(size_t i=0; i<cn.GetSize(); ++i)
      words+=cn[i].size();
  }
  void print(std::ostream& out) const {
    if(created>0) {
      out<<"confusion net statistics:\n"
         " created:\t"<<created<<"\n"
         " destroyed:\t"<<destr<<"\n"
         " succ. read:\t"<<read<<"\n"
         " columns:\t"<<colls<<"\n"
         " words:\t"<<words<<"\n"
         " avg. word/column:\t"<<words/(1.0*colls)<<"\n"
         " avg. cols/sent:\t"<<colls/(1.0*read)<<"\n"
         "\n\n";
    }
  }

};

CNStats stats;

size_t ConfusionNet::GetColumnIncrement(size_t i, size_t j) const
{
  (void) i;
  (void) j;
  return 1;
}

ConfusionNet::ConfusionNet()
  : InputType()
{
  stats.createOne();
}
ConfusionNet::~ConfusionNet()
{
  stats.destroyOne();
}

ConfusionNet::ConfusionNet(Sentence const& s)
{
  data.resize(s.GetSize());
  for(size_t i=0; i<s.GetSize(); ++i)
    data[i].push_back(std::make_pair(s.GetWord(i),0.0));
}

bool ConfusionNet::ReadF(std::istream& in,
                         const std::vector<FactorType>& factorOrder,
                         int format)
{
  VERBOSE(1, "read confusion net with format "<<format<<"\n");
  switch(format) {
  case 0:
    return ReadFormat0(in,factorOrder);
  case 1:
    return ReadFormat1(in,factorOrder);
  default:
    std::stringstream strme;
    strme << "ERROR: unknown format '"<<format
          <<"' in ConfusionNet::Read";
    UserMessage::Add(strme.str());
  }
  return false;
}

int ConfusionNet::Read(std::istream& in,
                       const std::vector<FactorType>& factorOrder)
{
  int rv=ReadF(in,factorOrder,0);
  if(rv) stats.collect(*this);
  return rv;
}


void ConfusionNet::String2Word(const std::string& s,Word& w,
                               const std::vector<FactorType>& factorOrder)
{
  std::vector<std::string> factorStrVector = Tokenize(s, "|");
  for(size_t i=0; i<factorOrder.size(); ++i)
    w.SetFactor(factorOrder[i],
                FactorCollection::Instance().AddFactor(Input,factorOrder[i],
                    factorStrVector[i]));
}

bool ConfusionNet::ReadFormat0(std::istream& in,
                               const std::vector<FactorType>& factorOrder)
{
  Clear();
  std::string line;
  size_t numLinkParams = StaticData::Instance().GetNumLinkParams();
  size_t numLinkWeights = StaticData::Instance().GetNumInputScores();
  bool addRealWordCount = ((numLinkParams + 1) == numLinkWeights);

  while(getline(in,line)) {
    std::istringstream is(line);
    std::string word;

    Column col;
    while(is>>word) {
      Word w;
      String2Word(word,w,factorOrder);
      std::vector<float> probs(numLinkWeights,0.0);
      for(size_t i=0; i<numLinkParams; i++) {
        double prob;
        if (!(is>>prob)) {
          TRACE_ERR("ERROR: unable to parse CN input - bad link probability, or wrong number of scores\n");
          return false;
        }
        if(prob<0.0) {
          VERBOSE(1, "WARN: negative prob: "<<prob<<" ->set to 0.0\n");
          prob=0.0;
        } else if (prob>1.0) {
          VERBOSE(1, "WARN: prob > 1.0 : "<<prob<<" -> set to 1.0\n");
          prob=1.0;
        }
        probs[i] = (std::max(static_cast<float>(log(prob)),LOWEST_SCORE));

      }
      //store 'real' word count in last feature if we have one more weight than we do arc scores and not epsilon
      if (addRealWordCount && word!=EPSILON && word!="")
        probs[numLinkParams] = -1.0;
      col.push_back(std::make_pair(w,probs));
    }
    if(col.size()) {
      data.push_back(col);
      ShrinkToFit(data.back());
    } else break;
  }
  return !data.empty();
}
bool ConfusionNet::ReadFormat1(std::istream& in,
                               const std::vector<FactorType>& factorOrder)
{
  Clear();
  std::string line;
  if(!getline(in,line)) return 0;
  size_t s;
  if(getline(in,line)) s=atoi(line.c_str());
  else return 0;
  data.resize(s);
  for(size_t i=0; i<data.size(); ++i) {
    if(!getline(in,line)) return 0;
    std::istringstream is(line);
    if(!(is>>s)) return 0;
    std::string word;
    double prob;
    data[i].resize(s);
    for(size_t j=0; j<s; ++j)
      if(is>>word>>prob) {
        //TODO: we are only reading one prob from this input format, should read many... but this function is unused anyway. -JS
        data[i][j].second = std::vector<float> (1);
        data[i][j].second.push_back((float) log(prob));
        if(data[i][j].second[0]<0) {
          VERBOSE(1, "WARN: neg costs: "<<data[i][j].second[0]<<" -> set to 0\n");
          data[i][j].second[0]=0.0;
        }
        String2Word(word,data[i][j].first,factorOrder);
      } else return 0;
  }
  return !data.empty();
}

void ConfusionNet::Print(std::ostream& out) const
{
  out<<"conf net: "<<data.size()<<"\n";
  for(size_t i=0; i<data.size(); ++i) {
    out<<i<<" -- ";
    for(size_t j=0; j<data[i].size(); ++j) {
      out<<"("<<data[i][j].first.ToString()<<", ";
      for(std::vector<float>::const_iterator scoreIterator = data[i][j].second.begin(); scoreIterator<data[i][j].second.end(); scoreIterator++) {
        out<<", "<<*scoreIterator;
      }
      out<<") ";
    }
    out<<"\n";
  }
  out<<"\n\n";
}

#ifdef _WIN32
#pragma warning(disable:4716)
#endif
Phrase ConfusionNet::GetSubString(const WordsRange&) const
{
  TRACE_ERR("ERROR: call to ConfusionNet::GetSubString\n");
  abort();
  //return Phrase(Input);
}

std::string ConfusionNet::GetStringRep(const std::vector<FactorType> /* factorsToPrint */) const  //not well defined yet
{
  TRACE_ERR("ERROR: call to ConfusionNet::GeStringRep\n");
  return "";
}
#ifdef _WIN32
#pragma warning(disable:4716)
#endif
const Word& ConfusionNet::GetWord(size_t) const
{
  TRACE_ERR("ERROR: call to ConfusionNet::GetFactorArray\n");
  abort();
}
#ifdef _WIN32
#pragma warning(default:4716)
#endif
std::ostream& operator<<(std::ostream& out,const ConfusionNet& cn)
{
  cn.Print(out);
  return out;
}

TranslationOptionCollection*
ConfusionNet::CreateTranslationOptionCollection(const TranslationSystem* system) const
{
  size_t maxNoTransOptPerCoverage = StaticData::Instance().GetMaxNoTransOptPerCoverage();
  float translationOptionThreshold = StaticData::Instance().GetTranslationOptionThreshold();
  TranslationOptionCollection *rv= new TranslationOptionCollectionConfusionNet(system, *this, maxNoTransOptPerCoverage, translationOptionThreshold);
  CHECK(rv);
  return rv;
}

}


