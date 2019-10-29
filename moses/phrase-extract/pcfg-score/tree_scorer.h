/***********************************************************************
 Moses - statistical machine translation system
 Copyright (C) 2006-2012 University of Edinburgh
 
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
#ifndef PCFG_SCORE_TREE_SCORER_H_
#define PCFG_SCORE_TREE_SCORER_H_

#include "pcfg-common/pcfg.h"
#include "pcfg-common/pcfg_tree.h"
#include "pcfg-common/typedef.h"

namespace Moses {
namespace PCFG {

class TreeScorer {
 public:
  TreeScorer(const Pcfg &, const Vocabulary &);

  // Score tree according to PCFG.  Returns false if unsuccessful (due to
  // missing rule).
  bool Score(PcfgTree &) const;

 private:
  const Pcfg &pcfg_;
  const Vocabulary &non_term_vocab_;
};

}  // namespace PCFG
}  // namespace Moses

#endif
