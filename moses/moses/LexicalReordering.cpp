#include <sstream>

#include "FFState.h"
#include "LexicalReordering.h"
#include "LexicalReorderingState.h"
#include "StaticData.h"

namespace Moses
{

LexicalReordering::LexicalReordering(std::vector<FactorType>& f_factors,
                                     std::vector<FactorType>& e_factors,
                                     const LexicalReorderingConfiguration& configuration,
                                     const std::string &filePath,
                                     const std::vector<float>& weights)
  : StatefulFeatureFunction("LexicalReordering_" + configuration.GetModelString(),
                            configuration.GetNumScoreComponents()),
    m_configuration(configuration)
{
  m_configuration.SetScoreProducer(this);
  std::cerr << "Creating lexical reordering...\n";
  std::cerr << "weights: ";
  for(size_t w = 0; w < weights.size(); ++w) {
    std::cerr << weights[w] << " ";
  }
  std::cerr << "\n";

  m_modelTypeString = m_configuration.GetModelString();

  switch(m_configuration.GetCondition()) {
  case LexicalReorderingConfiguration::FE:
  case LexicalReorderingConfiguration::E:
    m_factorsE = e_factors;
    if(m_factorsE.empty()) {
      UserMessage::Add("TL factor mask for lexical reordering is unexpectedly empty");
      exit(1);
    }
    if(m_configuration.GetCondition() == LexicalReorderingConfiguration::E)
      break; // else fall through
  case LexicalReorderingConfiguration::F:
    m_factorsF = f_factors;
    if(m_factorsF.empty()) {
      UserMessage::Add("SL factor mask for lexical reordering is unexpectedly empty");
      exit(1);
    }
    break;
  default:
    UserMessage::Add("Unknown conditioning option!");
    exit(1);
  }

  size_t numberOfScoreComponents = m_configuration.GetNumScoreComponents();
  if (weights.size() > numberOfScoreComponents) {
    m_configuration.SetAdditionalScoreComponents(weights.size() - numberOfScoreComponents);
  } else if(weights.size() < numberOfScoreComponents) {
    std::ostringstream os;
    os << "Lexical reordering model (type " << m_modelTypeString << "): expected " << numberOfScoreComponents << " weights, got " << weights.size() << std::endl;
    UserMessage::Add(os.str());
    exit(1);
  }

  const_cast<StaticData&>(StaticData::Instance()).SetWeights(this, weights);

  m_table = LexicalReorderingTable::LoadAvailable(filePath, m_factorsF, m_factorsE, std::vector<FactorType>());
}

LexicalReordering::~LexicalReordering()
{
  if(m_table)
    delete m_table;
}

Scores LexicalReordering::GetProb(const Phrase& f, const Phrase& e) const
{
  return m_table->GetScore(f, e, Phrase(ARRAY_SIZE_INCR));
}

FFState* LexicalReordering::Evaluate(const Hypothesis& hypo,
                                     const FFState* prev_state,
                                     ScoreComponentCollection* out) const
{
  Scores score(GetNumScoreComponents(), 0);
  const LexicalReorderingState *prev = dynamic_cast<const LexicalReorderingState *>(prev_state);
  LexicalReorderingState *next_state = prev->Expand(hypo.GetTranslationOption(), score);

  out->PlusEquals(this, score);

  return next_state;
}

const FFState* LexicalReordering::EmptyHypothesisState(const InputType &input) const
{
  return m_configuration.CreateLexicalReorderingState(input);
}

}

