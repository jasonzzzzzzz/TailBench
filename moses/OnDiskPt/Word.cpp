// $Id$
/***********************************************************************
 Moses - factored phrase-based, hierarchical and syntactic language decoder
 Copyright (C) 2009 Hieu Hoang

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

#include "moses/FactorCollection.h"
#include "moses/Util.h"
#include "moses/Word.h"
#include "Word.h"

#include "util/tokenize_piece.hh"
#include "util/exception.hh"

using namespace std;

namespace OnDiskPt
{

Word::Word(const Word &copy)
  :m_isNonTerminal(copy.m_isNonTerminal)
  ,m_vocabId(copy.m_vocabId)
{}

Word::~Word()
{}

void Word::CreateFromString(const std::string &inString, Vocab &vocab)
{
  if (inString.substr(0, 1) == "[" && inString.substr(inString.size() - 1, 1) == "]") {
    // non-term
    m_isNonTerminal = true;
    string str = inString.substr(1, inString.size() - 2);
    m_vocabId = vocab.AddVocabId(str);
  } else {
    m_isNonTerminal = false;
    m_vocabId = vocab.AddVocabId(inString);
  }

}

size_t Word::WriteToMemory(char *mem) const
{
  UINT64 *vocabMem = (UINT64*) mem;
  vocabMem[0] = m_vocabId;

  size_t size = sizeof(UINT64);

  // is non-term
  char bNonTerm = (char) m_isNonTerminal;
  mem[size] = bNonTerm;
  ++size;

  return size;
}

size_t Word::ReadFromMemory(const char *mem)
{
  UINT64 *vocabMem = (UINT64*) mem;
  m_vocabId = vocabMem[0];

  size_t memUsed = sizeof(UINT64);

  // is non-term
  char bNonTerm;
  bNonTerm = mem[memUsed];
  m_isNonTerminal = (bool) bNonTerm;
  ++memUsed;

  return memUsed;
}

size_t Word::ReadFromFile(std::fstream &file)
{
  const size_t memAlloc = sizeof(UINT64) + sizeof(char);
  char mem[sizeof(UINT64) + sizeof(char)];
  file.read(mem, memAlloc);

  size_t memUsed = ReadFromMemory(mem);
  CHECK(memAlloc == memUsed);

  return memAlloc;
}

void Word::ConvertToMoses(
    const std::vector<Moses::FactorType> &outputFactorsVec, 
    const Vocab &vocab,
    Moses::Word &overwrite) const {
  Moses::FactorCollection &factorColl = Moses::FactorCollection::Instance();
  overwrite = Moses::Word(m_isNonTerminal);

  // TODO: this conversion should have been done at load time.  
  util::TokenIter<util::SingleCharacter> tok(vocab.GetString(m_vocabId), '|');

  for (std::vector<Moses::FactorType>::const_iterator t = outputFactorsVec.begin(); t != outputFactorsVec.end(); ++t, ++tok) {
    UTIL_THROW_IF(!tok, util::Exception, "Too few factors in \"" << vocab.GetString(m_vocabId) << "\"; was expecting " << outputFactorsVec.size());
    overwrite.SetFactor(*t, factorColl.AddFactor(*tok));
  }
  UTIL_THROW_IF(tok, util::Exception, "Too many factors in \"" << vocab.GetString(m_vocabId) << "\"; was expecting " << outputFactorsVec.size());
}

int Word::Compare(const Word &compare) const
{
  int ret;

  if (m_isNonTerminal != compare.m_isNonTerminal)
    return m_isNonTerminal ?-1 : 1;

  if (m_vocabId < compare.m_vocabId)
    ret = -1;
  else if (m_vocabId > compare.m_vocabId)
    ret = 1;
  else
    ret = 0;

  return ret;
}

bool Word::operator<(const Word &compare) const
{
  int ret = Compare(compare);
  return ret < 0;
}

bool Word::operator==(const Word &compare) const
{
  int ret = Compare(compare);
  return ret == 0;
}

void Word::DebugPrint(ostream &out, const Vocab &vocab) const
{
 	const string &str = vocab.GetString(m_vocabId);
  out << str;
}

std::ostream& operator<<(std::ostream &out, const Word &word)
{
  out << "(";
 	out << word.m_vocabId;

  out << (word.m_isNonTerminal ? "n" : "t");
  out << ")";

  return out;
}
}
