/***********************************************************************
  Moses - factored phrase-based language decoder
  Copyright (C) 2011 University of Edinburgh

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

#include "ChartRuleLookupManagerMemoryPerSentence.h"
#include "DotChartInMemory.h"

#include "moses/TranslationModel/RuleTable/PhraseDictionaryFuzzyMatch.h"
#include "moses/InputType.h"
#include "moses/ChartParserCallback.h"
#include "moses/StaticData.h"
#include "moses/NonTerminal.h"
#include "moses/ChartCellCollection.h"

namespace Moses
{

ChartRuleLookupManagerMemoryPerSentence::ChartRuleLookupManagerMemoryPerSentence(
  const InputType &src,
  const ChartCellCollectionBase &cellColl,
  const PhraseDictionaryFuzzyMatch &ruleTable)
  : ChartRuleLookupManagerCYKPlus(src, cellColl)
  , m_ruleTable(ruleTable)
{
  CHECK(m_dottedRuleColls.size() == 0);
  size_t sourceSize = src.GetSize();
  m_dottedRuleColls.resize(sourceSize);

  const PhraseDictionaryNodeSCFG &rootNode = m_ruleTable.GetRootNode(src);

  for (size_t ind = 0; ind < m_dottedRuleColls.size(); ++ind) {
#ifdef USE_BOOST_POOL
    DottedRuleInMemory *initDottedRule = m_dottedRulePool.malloc();
    new (initDottedRule) DottedRuleInMemory(rootNode);
#else
    DottedRuleInMemory *initDottedRule = new DottedRuleInMemory(rootNode);
#endif

    DottedRuleColl *dottedRuleColl = new DottedRuleColl(sourceSize - ind + 1);
    dottedRuleColl->Add(0, initDottedRule); // init rule. stores the top node in tree

    m_dottedRuleColls[ind] = dottedRuleColl;
  }
}

ChartRuleLookupManagerMemoryPerSentence::~ChartRuleLookupManagerMemoryPerSentence()
{
  RemoveAllInColl(m_dottedRuleColls);
}

void ChartRuleLookupManagerMemoryPerSentence::GetChartRuleCollection(
  const WordsRange &range,
  ChartParserCallback &outColl)
{
  size_t relEndPos = range.GetEndPos() - range.GetStartPos();
  size_t absEndPos = range.GetEndPos();

  // MAIN LOOP. create list of nodes of target phrases

  // get list of all rules that apply to spans at same starting position
  DottedRuleColl &dottedRuleCol = *m_dottedRuleColls[range.GetStartPos()];
  const DottedRuleList &expandableDottedRuleList = dottedRuleCol.GetExpandableDottedRuleList();
  
  // loop through the rules
  // (note that expandableDottedRuleList can be expanded as the loop runs 
  //  through calls to ExtendPartialRuleApplication())
  for (size_t ind = 0; ind < expandableDottedRuleList.size(); ++ind) {
    // rule we are about to extend
    const DottedRuleInMemory &prevDottedRule = *expandableDottedRuleList[ind];
    // we will now try to extend it, starting after where it ended
    size_t startPos = prevDottedRule.IsRoot()
                    ? range.GetStartPos()
                    : prevDottedRule.GetWordsRange().GetEndPos() + 1;

    // search for terminal symbol
    // (if only one more word position needs to be covered)
    if (startPos == absEndPos) {

      // look up in rule dictionary, if the current rule can be extended
      // with the source word in the last position
      const ChartCellLabel &sourceWordLabel = GetSourceAt(absEndPos);
      const Word &sourceWord = sourceWordLabel.GetLabel();
      const PhraseDictionaryNodeSCFG *node = prevDottedRule.GetLastNode().GetChild(sourceWord);

      // if we found a new rule -> create it and add it to the list
      if (node != NULL) {
				// create the rule
#ifdef USE_BOOST_POOL
        DottedRuleInMemory *dottedRule = m_dottedRulePool.malloc();
        new (dottedRule) DottedRuleInMemory(*node, sourceWordLabel,
                                            prevDottedRule);
#else
        DottedRuleInMemory *dottedRule = new DottedRuleInMemory(*node,
                                                                sourceWordLabel,
                                                                prevDottedRule);
#endif
        dottedRuleCol.Add(relEndPos+1, dottedRule);
      }
    }

    // search for non-terminals
    size_t endPos, stackInd;

    // span is already complete covered? nothing can be done
    if (startPos > absEndPos)
      continue;

    else if (startPos == range.GetStartPos() && range.GetEndPos() > range.GetStartPos()) {
      // We're at the root of the prefix tree so won't try to cover the full
      // span (i.e. we don't allow non-lexical unary rules).  However, we need
      // to match non-unary rules that begin with a non-terminal child, so we
      // do that in two steps: during this iteration we search for non-terminals
      // that cover all but the last source word in the span (there won't
      // already be running nodes for these because that would have required a
      // non-lexical unary rule match for an earlier span).  Any matches will
      // result in running nodes being appended to the list and on subsequent
      // iterations (for this same span), we'll extend them to cover the final
      // word.
      endPos = absEndPos - 1;
      stackInd = relEndPos;
    }
    else 
    {
      endPos = absEndPos;
      stackInd = relEndPos + 1;
    }


    ExtendPartialRuleApplication(prevDottedRule, startPos, endPos, stackInd,
                                 dottedRuleCol);
  }

  // list of rules that that cover the entire span
  DottedRuleList &rules = dottedRuleCol.Get(relEndPos + 1);

  // look up target sides for the rules
  DottedRuleList::const_iterator iterRule;
  for (iterRule = rules.begin(); iterRule != rules.end(); ++iterRule) {
    const DottedRuleInMemory &dottedRule = **iterRule;
    const PhraseDictionaryNodeSCFG &node = dottedRule.GetLastNode();

    // look up target sides
    const TargetPhraseCollection *tpc = node.GetTargetPhraseCollection();

    // add the fully expanded rule (with lexical target side)
    if (tpc != NULL) {
      AddCompletedRule(dottedRule, *tpc, range, outColl);
    }
  }

  dottedRuleCol.Clear(relEndPos+1);
}

// Given a partial rule application ending at startPos-1 and given the sets of
// source and target non-terminals covering the span [startPos, endPos],
// determines the full or partial rule applications that can be produced through
// extending the current rule application by a single non-terminal.
void ChartRuleLookupManagerMemoryPerSentence::ExtendPartialRuleApplication(
  const DottedRuleInMemory &prevDottedRule,
  size_t startPos,
  size_t endPos,
  size_t stackInd,
  DottedRuleColl & dottedRuleColl)
{
  // source non-terminal labels for the remainder
  const NonTerminalSet &sourceNonTerms =
    GetSentence().GetLabelSet(startPos, endPos);

  // target non-terminal labels for the remainder
  const ChartCellLabelSet &targetNonTerms = GetTargetLabelSet(startPos, endPos);

  // note where it was found in the prefix tree of the rule dictionary
  const PhraseDictionaryNodeSCFG &node = prevDottedRule.GetLastNode();

  const PhraseDictionaryNodeSCFG::NonTerminalMap & nonTermMap =
    node.GetNonTerminalMap();

  const size_t numChildren = nonTermMap.size();
  if (numChildren == 0) {
    return;
  }
  const size_t numSourceNonTerms = sourceNonTerms.size();
  const size_t numTargetNonTerms = targetNonTerms.GetSize();
  const size_t numCombinations = numSourceNonTerms * numTargetNonTerms;

  // We can search by either:
  //   1. Enumerating all possible source-target NT pairs that are valid for
  //      the span and then searching for matching children in the node,
  // or
  //   2. Iterating over all the NT children in the node, searching
  //      for each source and target NT in the span's sets.
  // We'll do whichever minimises the number of lookups:
  if (numCombinations <= numChildren*2) {

		// loop over possible source non-terminal labels (as found in input tree)
    NonTerminalSet::const_iterator p = sourceNonTerms.begin();
    NonTerminalSet::const_iterator sEnd = sourceNonTerms.end();
    for (; p != sEnd; ++p) {
      const Word & sourceNonTerm = *p;

      // loop over possible target non-terminal labels (as found in chart)
      ChartCellLabelSet::const_iterator q = targetNonTerms.begin();
      ChartCellLabelSet::const_iterator tEnd = targetNonTerms.end();
      for (; q != tEnd; ++q) {
        const ChartCellLabel &cellLabel = q->second;

        // try to match both source and target non-terminal
        const PhraseDictionaryNodeSCFG * child =
          node.GetChild(sourceNonTerm, cellLabel.GetLabel());

        // nothing found? then we are done
        if (child == NULL) {
          continue;
        }

        // create new rule
#ifdef USE_BOOST_POOL
        DottedRuleInMemory *rule = m_dottedRulePool.malloc();
        new (rule) DottedRuleInMemory(*child, cellLabel, prevDottedRule);
#else
        DottedRuleInMemory *rule = new DottedRuleInMemory(*child, cellLabel,
                                                          prevDottedRule);
#endif
        dottedRuleColl.Add(stackInd, rule);
      }
    }
  } 
  else 
  {
    // loop over possible expansions of the rule
    PhraseDictionaryNodeSCFG::NonTerminalMap::const_iterator p;
    PhraseDictionaryNodeSCFG::NonTerminalMap::const_iterator end =
      nonTermMap.end();
    for (p = nonTermMap.begin(); p != end; ++p) {
      // does it match possible source and target non-terminals?
      const PhraseDictionaryNodeSCFG::NonTerminalMapKey &key = p->first;
      const Word &sourceNonTerm = key.first;
      if (sourceNonTerms.find(sourceNonTerm) == sourceNonTerms.end()) {
        continue;
      }
      const Word &targetNonTerm = key.second;
      const ChartCellLabel *cellLabel = targetNonTerms.Find(targetNonTerm);
      if (!cellLabel) {
        continue;
      }

      // create new rule
      const PhraseDictionaryNodeSCFG &child = p->second;
#ifdef USE_BOOST_POOL
      DottedRuleInMemory *rule = m_dottedRulePool.malloc();
      new (rule) DottedRuleInMemory(child, *cellLabel, prevDottedRule);
#else
      DottedRuleInMemory *rule = new DottedRuleInMemory(child, *cellLabel,
                                                        prevDottedRule);
#endif
      dottedRuleColl.Add(stackInd, rule);
    }
  }
}

}  // namespace Moses
