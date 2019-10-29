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
#include <algorithm>
#include <set>
#include "util/check.hh"
#include "AlignmentInfo.h"
#include "TypeDef.h"
#include "StaticData.h"

namespace Moses
{
AlignmentInfo::AlignmentInfo(const std::set<std::pair<size_t,size_t> > &pairs)
  : m_collection(pairs)
{
  BuildNonTermIndexMap();
}

void AlignmentInfo::BuildNonTermIndexMap()
{
  if (m_collection.empty()) {
    return;
  }
  const_iterator p = begin();
  size_t maxIndex = p->second;
  for (++p;  p != end(); ++p) {
    if (p->second > maxIndex) {
      maxIndex = p->second;
    }
  }
  m_nonTermIndexMap.resize(maxIndex+1, NOT_FOUND);
  size_t i = 0;
  for (p = begin(); p != end(); ++p) {
  	if (m_nonTermIndexMap[p->second] != NOT_FOUND) {
  		// 1-to-many. Definitely a set of terminals. Don't bother storing 1-to-1 index map
  		m_nonTermIndexMap.clear();
  		return;
  	}
    m_nonTermIndexMap[p->second] = i++;
  }
            
}

bool compare_target(const std::pair<size_t,size_t> *a, const std::pair<size_t,size_t> *b) {
  if(a->second < b->second)  return true;
  if(a->second == b->second) return (a->first < b->first);
  return false;
}


std::vector< const std::pair<size_t,size_t>* > AlignmentInfo::GetSortedAlignments() const
{
  std::vector< const std::pair<size_t,size_t>* > ret;
  
  CollType::const_iterator iter;
  for (iter = m_collection.begin(); iter != m_collection.end(); ++iter)
  {
    const std::pair<size_t,size_t> &alignPair = *iter;
    ret.push_back(&alignPair);
  }
  
  const StaticData &staticData = StaticData::Instance();
  WordAlignmentSort wordAlignmentSort = staticData.GetWordAlignmentSort();
  
  switch (wordAlignmentSort)
  {
    case NoSort:
      break;
      
    case TargetOrder:
      std::sort(ret.begin(), ret.end(), compare_target);
      break;
      
    default:
      CHECK(false);
  }
  
  return ret;
  
}

std::vector<size_t> AlignmentInfo::GetSourceIndex2PosMap() const
{
  std::set<size_t> sourcePoses;

  CollType::const_iterator iter;
  for (iter = m_collection.begin(); iter != m_collection.end(); ++iter) {
    size_t sourcePos = iter->first;
    sourcePoses.insert(sourcePos);
  }
  std::vector<size_t> ret(sourcePoses.begin(), sourcePoses.end());
  return ret;
}

std::ostream& operator<<(std::ostream &out, const AlignmentInfo &alignmentInfo)
{
  AlignmentInfo::const_iterator iter;
  for (iter = alignmentInfo.begin(); iter != alignmentInfo.end(); ++iter) {
    out << iter->first << "-" << iter->second << " ";
  }
  return out;
}

}
