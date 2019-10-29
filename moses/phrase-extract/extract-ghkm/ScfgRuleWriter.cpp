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

#include "ScfgRuleWriter.h"

#include "Alignment.h"
#include "Options.h"
#include "ScfgRule.h"

#include <cassert>
#include <cmath>
#include <ostream>
#include <map>
#include <sstream>
#include <vector>

namespace Moses {
namespace GHKM {

void ScfgRuleWriter::Write(const ScfgRule &rule)
{
  std::ostringstream sourceSS;
  std::ostringstream targetSS;

  if (m_options.unpairedExtractFormat) {
    WriteUnpairedFormat(rule, sourceSS, targetSS);
  } else {
    WriteStandardFormat(rule, sourceSS, targetSS);
  }

  // Write the rule to the forward and inverse extract files.
  m_fwd << sourceSS.str() << " ||| " << targetSS.str() << " |||";
  m_inv << targetSS.str() << " ||| " << sourceSS.str() << " |||";

  const Alignment &alignment = rule.GetAlignment();
  for (Alignment::const_iterator p = alignment.begin();
       p != alignment.end(); ++p) {
    m_fwd << " " << p->first << "-" << p->second;
    m_inv << " " << p->second << "-" << p->first;
  }

  // Write a count of 1 and an empty NT length column to the forward extract
  // file.
  // TODO Add option to write NT length?
  m_fwd << " ||| 1 ||| |||";
  if (m_options.pcfg) {
    // Write the PCFG score.
    m_fwd << " " << std::exp(rule.GetPcfgScore());
  }
  m_fwd << std::endl;

  // Write a count of 1 to the inverse extract file.
  m_inv << " ||| 1" << std::endl;
}

void ScfgRuleWriter::WriteStandardFormat(const ScfgRule &rule,
                                         std::ostream &sourceSS,
                                         std::ostream &targetSS)
{
  const std::vector<Symbol> &sourceRHS = rule.GetSourceRHS();
  const std::vector<Symbol> &targetRHS = rule.GetTargetRHS();

  std::map<int, int> sourceToTargetNTMap;
  std::map<int, int> targetToSourceNTMap;

  const Alignment &alignment = rule.GetAlignment();

  for (Alignment::const_iterator p(alignment.begin());
       p != alignment.end(); ++p) {
    if (sourceRHS[p->first].GetType() == NonTerminal) {
      assert(targetRHS[p->second].GetType() == NonTerminal);
      sourceToTargetNTMap[p->first] = p->second;
      targetToSourceNTMap[p->second] = p->first;
    }
  }

  // Write the source side of the rule to sourceSS.
  int i = 0;
  for (std::vector<Symbol>::const_iterator p(sourceRHS.begin());
       p != sourceRHS.end(); ++p, ++i) {
    WriteSymbol(*p, sourceSS);
    if (p->GetType() == NonTerminal) {
      int targetIndex = sourceToTargetNTMap[i];
      WriteSymbol(targetRHS[targetIndex], sourceSS);
    }
    sourceSS << " ";
  }
  if (m_options.conditionOnTargetLhs) {
    WriteSymbol(rule.GetTargetLHS(), sourceSS);
  } else {
    WriteSymbol(rule.GetSourceLHS(), sourceSS);
  }

  // Write the target side of the rule to targetSS.
  i = 0;
  for (std::vector<Symbol>::const_iterator p(targetRHS.begin());
       p != targetRHS.end(); ++p, ++i) {
    if (p->GetType() == NonTerminal) {
      int sourceIndex = targetToSourceNTMap[i];
      WriteSymbol(sourceRHS[sourceIndex], targetSS);
    }
    WriteSymbol(*p, targetSS);
    targetSS << " ";
  }
  WriteSymbol(rule.GetTargetLHS(), targetSS);
}

void ScfgRuleWriter::WriteUnpairedFormat(const ScfgRule &rule,
                                         std::ostream &sourceSS,
                                         std::ostream &targetSS)
{
  const std::vector<Symbol> &sourceRHS = rule.GetSourceRHS();
  const std::vector<Symbol> &targetRHS = rule.GetTargetRHS();

  // Write the source side of the rule to sourceSS.
  int i = 0;
  for (std::vector<Symbol>::const_iterator p(sourceRHS.begin());
       p != sourceRHS.end(); ++p, ++i) {
    WriteSymbol(*p, sourceSS);
    sourceSS << " ";
  }
  if (m_options.conditionOnTargetLhs) {
    WriteSymbol(rule.GetTargetLHS(), sourceSS);
  } else {
    WriteSymbol(rule.GetSourceLHS(), sourceSS);
  }

  // Write the target side of the rule to targetSS.
  i = 0;
  for (std::vector<Symbol>::const_iterator p(targetRHS.begin());
       p != targetRHS.end(); ++p, ++i) {
    WriteSymbol(*p, targetSS);
    targetSS << " ";
  }
  WriteSymbol(rule.GetTargetLHS(), targetSS);
}

void ScfgRuleWriter::WriteSymbol(const Symbol &symbol, std::ostream &out)
{
  if (symbol.GetType() == NonTerminal) {
    out << "[" << symbol.GetValue() << "]";
  } else {
    out << symbol.GetValue();
  }
}

}  // namespace GHKM
}  // namespace Moses
