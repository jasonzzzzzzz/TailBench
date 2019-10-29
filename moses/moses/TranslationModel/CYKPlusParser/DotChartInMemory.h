/***********************************************************************
 Moses - statistical machine translation system
 Copyright (C) 2006-2011 University of Edinburgh

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

#pragma once

#include "DotChart.h"
#include "moses/TranslationModel/RuleTable/PhraseDictionaryNodeSCFG.h"

#include "util/check.hh"
#include <vector>

namespace Moses
{

/** @todo what is this?
 */
class DottedRuleInMemory : public DottedRule
{
 public:
  // used only to init dot stack.
  explicit DottedRuleInMemory(const PhraseDictionaryNodeSCFG &node)
      : DottedRule()
      , m_node(node) {}

  DottedRuleInMemory(const PhraseDictionaryNodeSCFG &node,
                     const ChartCellLabel &cellLabel,
                     const DottedRuleInMemory &prev)
      : DottedRule(cellLabel, prev)
      , m_node(node) {}
             
  const PhraseDictionaryNodeSCFG &GetLastNode() const { return m_node; }

 private:
  const PhraseDictionaryNodeSCFG &m_node;
};

typedef std::vector<const DottedRuleInMemory*> DottedRuleList;

// Collection of all in-memory DottedRules that share a common start point,
// grouped by end point.  Additionally, maintains a list of all
// DottedRules that could be expanded further, i.e. for which the
// corresponding PhraseDictionaryNodeSCFG is not a leaf.
class DottedRuleColl
{
protected:
  typedef std::vector<DottedRuleList> CollType;
  CollType m_coll;
  DottedRuleList m_expandableDottedRuleList;

public:
  typedef CollType::iterator iterator;
  typedef CollType::const_iterator const_iterator;

  const_iterator begin() const {
    return m_coll.begin();
  }
  const_iterator end() const {
    return m_coll.end();
  }
  iterator begin() {
    return m_coll.begin();
  }
  iterator end() {
    return m_coll.end();
  }

  DottedRuleColl(size_t size)
    : m_coll(size)
  {}

  ~DottedRuleColl();

  const DottedRuleList &Get(size_t pos) const {
    return m_coll[pos];
  }
  DottedRuleList &Get(size_t pos) {
    return m_coll[pos];
  }

  void Add(size_t pos, const DottedRuleInMemory *dottedRule) {
    CHECK(dottedRule);
    m_coll[pos].push_back(dottedRule);
    if (!dottedRule->GetLastNode().IsLeaf()) {
      m_expandableDottedRuleList.push_back(dottedRule);
    }
  }

  void Clear(size_t pos) {
#ifdef USE_BOOST_POOL
    m_coll[pos].clear();
#endif
  }

  const DottedRuleList &GetExpandableDottedRuleList() const {
    return m_expandableDottedRuleList;
  }

};

}
