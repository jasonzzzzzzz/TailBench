// $Id$

/***********************************************************************
Moses - factored phrase-based language decoder
Copyright (C) 2006 University of Edinburgh

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
***********************************************************************/

#include <limits>
#include <iostream>
#include <memory>
#include <sstream>

#include "moses/FFState.h"
#include "Implementation.h"
#include "moses/TypeDef.h"
#include "moses/Util.h"
#include "moses/Manager.h"
#include "moses/FactorCollection.h"
#include "moses/Phrase.h"
#include "moses/StaticData.h"
#include "moses/ChartManager.h"
#include "moses/ChartHypothesis.h"
#include "util/check.hh"

using namespace std;

namespace Moses
{

void LanguageModelImplementation::ShiftOrPush(std::vector<const Word*> &contextFactor, const Word &word) const
{
  if (contextFactor.size() < GetNGramOrder()) {
    contextFactor.push_back(&word);
  } else {
    // shift
    for (size_t currNGramOrder = 0 ; currNGramOrder < GetNGramOrder() - 1 ; currNGramOrder++) {
      contextFactor[currNGramOrder] = contextFactor[currNGramOrder + 1];
    }
    contextFactor[GetNGramOrder() - 1] = &word;
  }
}

LMResult LanguageModelImplementation::GetValueGivenState(
  const std::vector<const Word*> &contextFactor,
  FFState &state) const
{
  return GetValueForgotState(contextFactor, state);
}

void LanguageModelImplementation::GetState(
  const std::vector<const Word*> &contextFactor,
  FFState &state) const
{
  GetValueForgotState(contextFactor, state);
}

// Calculate score of a phrase.  
void LanguageModelImplementation::CalcScore(const Phrase &phrase, float &fullScore, float &ngramScore, size_t &oovCount) const {
  fullScore  = 0;
  ngramScore = 0;

  oovCount = 0;

  size_t phraseSize = phrase.GetSize();
  if (!phraseSize) return;

  vector<const Word*> contextFactor;
  contextFactor.reserve(GetNGramOrder());
  std::auto_ptr<FFState> state(NewState((phrase.GetWord(0) == GetSentenceStartArray()) ?
                               GetBeginSentenceState() : GetNullContextState()));
  size_t currPos = 0;
  while (currPos < phraseSize) {
    const Word &word = phrase.GetWord(currPos);

    if (word.IsNonTerminal()) {
      // do nothing. reset ngram. needed to score target phrases during pt loading in chart decoding
      if (!contextFactor.empty()) {
        // TODO: state operator= ?
        state.reset(NewState(GetNullContextState()));
        contextFactor.clear();
      }
    } else {
      ShiftOrPush(contextFactor, word);
      CHECK(contextFactor.size() <= GetNGramOrder());

      if (word == GetSentenceStartArray()) {
        // do nothing, don't include prob for <s> unigram
        if (currPos != 0) {
          std::cerr << "Either your data contains <s> in a position other than the first word or your language model is missing <s>.  Did you build your ARPA using IRSTLM and forget to run add-start-end.sh?" << std::endl;
          abort();
        }
      } else {
        LMResult result = GetValueGivenState(contextFactor, *state);
        fullScore += result.score;
        if (contextFactor.size() == GetNGramOrder())
          ngramScore += result.score;
        if (result.unknown) ++oovCount;  
      }
    }

    currPos++;
  }
}

FFState *LanguageModelImplementation::Evaluate(const Hypothesis &hypo, const FFState *ps, ScoreComponentCollection *out, const LanguageModel *feature) const {
  // In this function, we only compute the LM scores of n-grams that overlap a
  // phrase boundary. Phrase-internal scores are taken directly from the
  // translation option.

  // In the case of unigram language models, there is no overlap, so we don't
  // need to do anything.
  if(GetNGramOrder() <= 1)
    return NULL;

  clock_t t = 0;
  IFVERBOSE(2) {
    t = clock();  // track time
  }

  // Empty phrase added? nothing to be done
  if (hypo.GetCurrTargetLength() == 0)
    return ps ? NewState(ps) : NULL;

  const size_t currEndPos = hypo.GetCurrTargetWordsRange().GetEndPos();
  const size_t startPos = hypo.GetCurrTargetWordsRange().GetStartPos();

  // 1st n-gram
  vector<const Word*> contextFactor(GetNGramOrder());
  size_t index = 0;
  for (int currPos = (int) startPos - (int) GetNGramOrder() + 1 ; currPos <= (int) startPos ; currPos++) {
    if (currPos >= 0)
      contextFactor[index++] = &hypo.GetWord(currPos);
    else {
      contextFactor[index++] = &GetSentenceStartArray();
    }
  }
  FFState *res = NewState(ps);
  float lmScore = ps ? GetValueGivenState(contextFactor, *res).score : GetValueForgotState(contextFactor, *res).score;

  // main loop
  size_t endPos = std::min(startPos + GetNGramOrder() - 2
                           , currEndPos);
  for (size_t currPos = startPos + 1 ; currPos <= endPos ; currPos++) {
    // shift all args down 1 place
    for (size_t i = 0 ; i < GetNGramOrder() - 1 ; i++)
      contextFactor[i] = contextFactor[i + 1];

    // add last factor
    contextFactor.back() = &hypo.GetWord(currPos);

    lmScore += GetValueGivenState(contextFactor, *res).score;
  }

  // end of sentence
  if (hypo.IsSourceCompleted()) {
    const size_t size = hypo.GetSize();
    contextFactor.back() = &GetSentenceEndArray();

    for (size_t i = 0 ; i < GetNGramOrder() - 1 ; i ++) {
      int currPos = (int)(size - GetNGramOrder() + i + 1);
      if (currPos < 0)
        contextFactor[i] = &GetSentenceStartArray();
      else
        contextFactor[i] = &hypo.GetWord((size_t)currPos);
    }
    lmScore += GetValueForgotState(contextFactor, *res).score;
  }
  else
  {
    if (endPos < currEndPos) {
      //need to get the LM state (otherwise the last LM state is fine)
      for (size_t currPos = endPos+1; currPos <= currEndPos; currPos++) {
        for (size_t i = 0 ; i < GetNGramOrder() - 1 ; i++)
          contextFactor[i] = contextFactor[i + 1];
        contextFactor.back() = &hypo.GetWord(currPos);
      }
      GetState(contextFactor, *res);
    }
  }
  if (feature->OOVFeatureEnabled()) {
    vector<float> scores(2);
    scores[0] = lmScore;
    scores[1] = 0;
    out->PlusEquals(feature, scores);
  } else {
    out->PlusEquals(feature, lmScore);
  }


  IFVERBOSE(2) {
    hypo.GetManager().GetSentenceStats().AddTimeCalcLM( clock()-t );
  }
  return res;
}

namespace {

// This is the FFState used by LanguageModelImplementation::EvaluateChart.  
// Though svn blame goes back to heafield, don't blame me.  I just moved this from LanguageModelChartState.cpp and ChartHypothesis.cpp.  
class LanguageModelChartState : public FFState
{
private:
  float m_prefixScore;
  FFState* m_lmRightContext;

  Phrase m_contextPrefix, m_contextSuffix;

  size_t m_numTargetTerminals; // This isn't really correct except for the surviving hypothesis

  const ChartHypothesis &m_hypo;

  /** Construct the prefix string of up to specified size 
   * \param ret prefix string
   * \param size maximum size (typically max lm context window)
   */
  size_t CalcPrefix(const ChartHypothesis &hypo, int featureID, Phrase &ret, size_t size) const
  {
    const TargetPhrase &target = hypo.GetCurrTargetPhrase();
    const AlignmentInfo::NonTermIndexMap &nonTermIndexMap =
          target.GetAlignNonTerm().GetNonTermIndexMap();
    
    // loop over the rule that is being applied
    for (size_t pos = 0; pos < target.GetSize(); ++pos) {
      const Word &word = target.GetWord(pos);

      // for non-terminals, retrieve it from underlying hypothesis
      if (word.IsNonTerminal()) {
        size_t nonTermInd = nonTermIndexMap[pos];
        const ChartHypothesis *prevHypo = hypo.GetPrevHypo(nonTermInd);
        size = static_cast<const LanguageModelChartState*>(prevHypo->GetFFState(featureID))->CalcPrefix(*prevHypo, featureID, ret, size);
      }
      // for words, add word
      else {
        ret.AddWord(target.GetWord(pos));
        size--;
      }

      // finish when maximum length reached
      if (size==0)
        break;
    }

    return size;
  }

  /** Construct the suffix phrase of up to specified size 
   * will always be called after the construction of prefix phrase
   * \param ret suffix phrase
   * \param size maximum size of suffix
   */
  size_t CalcSuffix(const ChartHypothesis &hypo, int featureID, Phrase &ret, size_t size) const
  {
    CHECK(m_contextPrefix.GetSize() <= m_numTargetTerminals);

    // special handling for small hypotheses
    // does the prefix match the entire hypothesis string? -> just copy prefix
    if (m_contextPrefix.GetSize() == m_numTargetTerminals) {
      size_t maxCount = std::min(m_contextPrefix.GetSize(), size);
      size_t pos= m_contextPrefix.GetSize() - 1;

      for (size_t ind = 0; ind < maxCount; ++ind) {
        const Word &word = m_contextPrefix.GetWord(pos);
        ret.PrependWord(word);
        --pos;
      }

      size -= maxCount;
      return size;
    }
    // construct suffix analogous to prefix
    else {
      const TargetPhrase& target = hypo.GetCurrTargetPhrase();
      const AlignmentInfo::NonTermIndexMap &nonTermIndexMap =
            target.GetAlignNonTerm().GetNonTermIndexMap();
      for (int pos = (int) target.GetSize() - 1; pos >= 0 ; --pos) {
        const Word &word = target.GetWord(pos);

        if (word.IsNonTerminal()) {
          size_t nonTermInd = nonTermIndexMap[pos];
          const ChartHypothesis *prevHypo = hypo.GetPrevHypo(nonTermInd);
          size = static_cast<const LanguageModelChartState*>(prevHypo->GetFFState(featureID))->CalcSuffix(*prevHypo, featureID, ret, size);
        }
        else {
          ret.PrependWord(hypo.GetCurrTargetPhrase().GetWord(pos));
          size--;
        }

        if (size==0)
          break;
      }

      return size;
    }
  }


public:
  LanguageModelChartState(const ChartHypothesis &hypo, int featureID, size_t order)
      :m_lmRightContext(NULL)
      ,m_contextPrefix(order - 1)
      ,m_contextSuffix( order - 1)
      ,m_hypo(hypo)
  {
    m_numTargetTerminals = hypo.GetCurrTargetPhrase().GetNumTerminals();

    for (std::vector<const ChartHypothesis*>::const_iterator i = hypo.GetPrevHypos().begin(); i != hypo.GetPrevHypos().end(); ++i) {
      // keep count of words (= length of generated string)
      m_numTargetTerminals += static_cast<const LanguageModelChartState*>((*i)->GetFFState(featureID))->GetNumTargetTerminals();
    }

    CalcPrefix(hypo, featureID, m_contextPrefix, order - 1);
    CalcSuffix(hypo, featureID, m_contextSuffix, order - 1);
  }

  ~LanguageModelChartState() {
    delete m_lmRightContext;
  }

  void Set(float prefixScore, FFState *rightState) {
    m_prefixScore = prefixScore;
    m_lmRightContext = rightState;
  }

  float GetPrefixScore() const { return m_prefixScore; }
  FFState* GetRightContext() const { return m_lmRightContext; }

  size_t GetNumTargetTerminals() const {
    return m_numTargetTerminals;
  }

  const Phrase &GetPrefix() const {
    return m_contextPrefix;
  }
  const Phrase &GetSuffix() const {
    return m_contextSuffix;
  }

  int Compare(const FFState& o) const {
    const LanguageModelChartState &other =
      dynamic_cast<const LanguageModelChartState &>( o );

    // prefix
    if (m_hypo.GetCurrSourceRange().GetStartPos() > 0) // not for "<s> ..."
    {
      int ret = GetPrefix().Compare(other.GetPrefix());
      if (ret != 0)
        return ret;
    }

    // suffix
    size_t inputSize = m_hypo.GetManager().GetSource().GetSize();
    if (m_hypo.GetCurrSourceRange().GetEndPos() < inputSize - 1)// not for "... </s>"
    {
      int ret = other.GetRightContext()->Compare(*m_lmRightContext);
      if (ret != 0)
        return ret;
    }
    return 0;
  }
};

} // namespace

FFState* LanguageModelImplementation::EvaluateChart(const ChartHypothesis& hypo, int featureID, ScoreComponentCollection* out, const LanguageModel *scorer) const {
  LanguageModelChartState *ret = new LanguageModelChartState(hypo, featureID, GetNGramOrder());
  // data structure for factored context phrase (history and predicted word)
  vector<const Word*> contextFactor;
  contextFactor.reserve(GetNGramOrder());

  // initialize language model context state
  FFState *lmState = NewState( GetNullContextState() );

  // initial language model scores
  float prefixScore = 0.0;    // not yet final for initial words (lack context)
  float finalizedScore = 0.0; // finalized, has sufficient context

  // get index map for underlying hypotheses
  const TargetPhrase &target = hypo.GetCurrTargetPhrase();
  const AlignmentInfo::NonTermIndexMap &nonTermIndexMap =
      hypo.GetCurrTargetPhrase().GetAlignNonTerm().GetNonTermIndexMap();

  // loop over rule
  for (size_t phrasePos = 0, wordPos = 0;
       phrasePos < hypo.GetCurrTargetPhrase().GetSize();
       phrasePos++)
  {
    // consult rule for either word or non-terminal
    const Word &word = hypo.GetCurrTargetPhrase().GetWord(phrasePos);

    // regular word
    if (!word.IsNonTerminal())
    {
      ShiftOrPush(contextFactor, word);

      // beginning of sentence symbol <s>? -> just update state
      if (word == GetSentenceStartArray())
      {        
        CHECK(phrasePos == 0);
        delete lmState;
        lmState = NewState( GetBeginSentenceState() );
      }
      // score a regular word added by the rule
      else
      {
        updateChartScore( &prefixScore, &finalizedScore, UntransformLMScore(GetValueGivenState(contextFactor, *lmState).score), ++wordPos );
      }
    }

    // non-terminal, add phrase from underlying hypothesis
    else
    {
      // look up underlying hypothesis
      size_t nonTermIndex = nonTermIndexMap[phrasePos];
      const ChartHypothesis *prevHypo = hypo.GetPrevHypo(nonTermIndex);

      const LanguageModelChartState* prevState =
        static_cast<const LanguageModelChartState*>(prevHypo->GetFFState(featureID));

      size_t subPhraseLength = prevState->GetNumTargetTerminals();

      // special case: rule starts with non-terminal -> copy everything
      if (phrasePos == 0) {

        // get prefixScore and finalizedScore
        prefixScore = prevState->GetPrefixScore();
        finalizedScore = prevHypo->GetScoreBreakdown().GetScoresForProducer(scorer)[0] - prefixScore;

        // get language model state
        delete lmState;
        lmState = NewState( prevState->GetRightContext() );

        // push suffix
        int suffixPos = prevState->GetSuffix().GetSize() - (GetNGramOrder()-1);
        if (suffixPos < 0) suffixPos = 0; // push all words if less than order
        for(;(size_t)suffixPos < prevState->GetSuffix().GetSize(); suffixPos++)
        {
          const Word &word = prevState->GetSuffix().GetWord(suffixPos);
          ShiftOrPush(contextFactor, word);
          wordPos++;
        }
      }

      // internal non-terminal
      else
      {
        // score its prefix
        for(size_t prefixPos = 0;
            prefixPos < GetNGramOrder()-1 // up to LM order window
              && prefixPos < subPhraseLength; // up to length
            prefixPos++)
        {
          const Word &word = prevState->GetPrefix().GetWord(prefixPos);
          ShiftOrPush(contextFactor, word);
          updateChartScore( &prefixScore, &finalizedScore, UntransformLMScore(GetValueGivenState(contextFactor, *lmState).score), ++wordPos );
        }

        // check if we are dealing with a large sub-phrase
        if (subPhraseLength > GetNGramOrder() - 1)
        {
          // add its finalized language model score
          finalizedScore +=
            prevHypo->GetScoreBreakdown().GetScoresForProducer(scorer)[0] // full score
            - prevState->GetPrefixScore();                              // - prefix score

          // copy language model state
          delete lmState;
          lmState = NewState( prevState->GetRightContext() );

          // push its suffix
          size_t remainingWords = subPhraseLength - (GetNGramOrder()-1);
          if (remainingWords > GetNGramOrder()-1) {
            // only what is needed for the history window
            remainingWords = GetNGramOrder()-1;
          }
          for(size_t suffixPos = prevState->GetSuffix().GetSize() - remainingWords;
              suffixPos < prevState->GetSuffix().GetSize();
              suffixPos++) {
            const Word &word = prevState->GetSuffix().GetWord(suffixPos);
            ShiftOrPush(contextFactor, word);
          }
          wordPos += subPhraseLength;
        }
      }
    }
  }

  // assign combined score to score breakdown
  out->Assign(scorer, prefixScore + finalizedScore);

  ret->Set(prefixScore, lmState);
  return ret;
}

void LanguageModelImplementation::updateChartScore(float *prefixScore, float *finalizedScore, float score, size_t wordPos) const {
  if (wordPos < GetNGramOrder()) {
    *prefixScore += score;
  }
  else {
    *finalizedScore += score;
  }
}

}
