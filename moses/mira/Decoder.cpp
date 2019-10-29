/***********************************************************************
Moses - factored phrase-based language decoder
Copyright (C) 2009 University of Edinburgh

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

#include "Decoder.h"
#include "moses/Manager.h"
#include "moses/ChartManager.h"
#include "moses/Sentence.h"
#include "moses/InputType.h"
#include "moses/TranslationSystem.h"
#include "moses/Phrase.h"
#include "moses/TrellisPathList.h"
#include "moses/ChartTrellisPathList.h"
#include "moses/ChartTrellisPath.h"

using namespace std;
using namespace Moses;


namespace Mira {

  /**
    * Allocates a char* and copies string into it.
  **/
  static char* strToChar(const string& s) {
    char* c = new char[s.size()+1];
    strcpy(c,s.c_str());
    return c;
  }

  MosesDecoder::MosesDecoder(const string& inifile, int debuglevel, int argc, vector<string> decoder_params)
	: m_manager(NULL) {
  	static int BASE_ARGC = 8;
	  Parameter* params = new Parameter();
	  char ** mosesargv = new char*[BASE_ARGC + argc];
	  mosesargv[0] = strToChar("-f");
	  mosesargv[1] = strToChar(inifile);
	  mosesargv[2] = strToChar("-v");
	  stringstream dbgin;
	  dbgin << debuglevel;
	  mosesargv[3] = strToChar(dbgin.str());
	  mosesargv[4] = strToChar("-use-persistent-cache"); 
	  mosesargv[5] = strToChar("0"); 
	  mosesargv[6] = strToChar("-persistent-cache-size");
          mosesargv[7] = strToChar("0");

	  for (int i = 0; i < argc; ++i) {
		  char *cstr = &(decoder_params[i])[0];
		  mosesargv[BASE_ARGC + i] = cstr;
	  }

	  if (!params->LoadParam(BASE_ARGC + argc,mosesargv)) {
		  cerr << "Loading static data failed, exit." << endl;
		  exit(1);
	  }
	  StaticData::LoadDataStatic(params, "mira");
	  for (int i = 0; i < BASE_ARGC; ++i) {
		  delete[] mosesargv[i];
	  }
	  delete[] mosesargv;

	  const StaticData &staticData = StaticData::Instance();
	  m_bleuScoreFeature = staticData.GetBleuScoreFeature();
  }
  
  void MosesDecoder::cleanup(bool chartDecoding) {
	  delete m_manager;
	  if (chartDecoding)
	  	delete m_chartManager;
	  else
	  	delete m_sentence;
  }

  vector< vector<const Word*> > MosesDecoder::getNBest(const std::string& source,
                              size_t sentenceid,
                              size_t nBestSize,
                              float bleuObjectiveWeight, 
                              float bleuScoreWeight,
                              vector< ScoreComponentCollection>& featureValues,
                              vector< float>& bleuScores,
                              vector< float>& modelScores,
                              size_t numReturnedTranslations,
                              bool realBleu,
                              bool distinct,
                              bool avgRefLength,
                              size_t rank,
                              size_t epoch,
                              string filename)
  {
  	StaticData &staticData = StaticData::InstanceNonConst();
  	bool chartDecoding = (staticData.GetSearchAlgorithm() == ChartDecoding);
  	initialize(staticData, source, sentenceid, bleuObjectiveWeight, bleuScoreWeight, avgRefLength, chartDecoding);
    const TranslationSystem& system = staticData.GetTranslationSystem(TranslationSystem::DEFAULT);

    // run the decoder
    if (chartDecoding) {
    	return runChartDecoder(source, sentenceid, nBestSize, bleuObjectiveWeight, bleuScoreWeight,
    			featureValues, bleuScores, modelScores, numReturnedTranslations, realBleu, distinct, rank, epoch,
    			system);
    }
    else {
    	SearchAlgorithm search = staticData.GetSearchAlgorithm();
    	return runDecoder(source, sentenceid, nBestSize, bleuObjectiveWeight, bleuScoreWeight,
    			featureValues, bleuScores, modelScores, numReturnedTranslations, realBleu, distinct, rank, epoch,
    			search, system, filename);
    }
  }

  vector< vector<const Word*> > MosesDecoder::runDecoder(const std::string& source,
  														size_t sentenceid,
  														size_t nBestSize,
  														float bleuObjectiveWeight,
  														float bleuScoreWeight,
  														vector< ScoreComponentCollection>& featureValues,
  														vector< float>& bleuScores,
  														vector< float>& modelScores,
  														size_t numReturnedTranslations,
  														bool realBleu,
  														bool distinct,
  														size_t rank,
  														size_t epoch,
  														SearchAlgorithm& search,
  														const TranslationSystem& system,
  														string filename) {
  	// run the decoder
    m_manager = new Moses::Manager(0,*m_sentence, search, &system);
    m_manager->ProcessSentence();
    TrellisPathList nBestList;
    m_manager->CalcNBest(nBestSize, nBestList, distinct);
    
    // optionally print nbest to file (to extract scores and features.. currently just for sentence bleu scoring)
    if (filename != "") {
    	ofstream out(filename.c_str());
    	if (!out) {
    		ostringstream msg;
    		msg << "Unable to open " << filename;
    		throw runtime_error(msg.str());
    	}
    	// TODO: handle sentence id (for now always 0)
    	//OutputNBest(out, nBestList, StaticData::Instance().GetOutputFactorOrder(),m_manager->GetTranslationSystem(), 0, false);
    	out.close();
    }

    // read off the feature values and bleu scores for each sentence in the nbest list
    Moses::TrellisPathList::const_iterator iter;
    for (iter = nBestList.begin() ; iter != nBestList.end() ; ++iter) {
    	const Moses::TrellisPath &path = **iter;
    	featureValues.push_back(path.GetScoreBreakdown());
    	float bleuScore, dynBleuScore, realBleuScore;
	if (realBleu) realBleuScore = m_bleuScoreFeature->CalculateBleu(path.GetTargetPhrase());
	else dynBleuScore = getBleuScore(featureValues.back());  
    	bleuScore = realBleu ? realBleuScore : dynBleuScore; 
    	bleuScores.push_back(bleuScore);	

    	//std::cout << "Score breakdown: " << path.GetScoreBreakdown() << endl;
    	float scoreWithoutBleu = path.GetTotalScore() - (bleuObjectiveWeight * bleuScoreWeight * bleuScore);
    	modelScores.push_back(scoreWithoutBleu);
    	
    	if (iter != nBestList.begin())
    		cerr << endl;  	
    	cerr << "Rank " << rank << ", epoch " << epoch << ", \"" << path.GetTargetPhrase() << "\", score: " 
    		 << scoreWithoutBleu << ", Bleu: " << bleuScore << ", total: " << path.GetTotalScore();
	if (m_bleuScoreFeature->Enabled() && realBleu)
	  cerr << " (d-bleu: " << dynBleuScore << ", r-bleu: " << realBleuScore << ") ";

    	// set bleu score to zero in the feature vector since we do not want to optimise its weight
    	setBleuScore(featureValues.back(), 0);
    }

    // prepare translations to return
    vector< vector<const Word*> > translations;
    for (size_t i=0; i < numReturnedTranslations && i < nBestList.GetSize(); ++i) {
        const TrellisPath &path = nBestList.at(i);
        Phrase phrase = path.GetTargetPhrase();

        vector<const Word*> translation;
        for (size_t pos = 0; pos < phrase.GetSize(); ++pos) {
        	const Word &word = phrase.GetWord(pos);
        	Word *newWord = new Word(word);
        	translation.push_back(newWord);
        }
        translations.push_back(translation);
    }

    return translations;
  }

  vector< vector<const Word*> > MosesDecoder::runChartDecoder(const std::string& source,
                              size_t sentenceid,
                              size_t nBestSize,
                              float bleuObjectiveWeight,
                              float bleuScoreWeight,
                              vector< ScoreComponentCollection>& featureValues,
                              vector< float>& bleuScores,
                              vector< float>& modelScores,
                              size_t numReturnedTranslations,
                              bool realBleu,
                              bool distinct,
                              size_t rank,
                              size_t epoch,
                              const TranslationSystem& system) {
  	// run the decoder
    m_chartManager = new ChartManager(*m_sentence, &system);
    m_chartManager->ProcessSentence();
    ChartTrellisPathList nBestList;
    m_chartManager->CalcNBest(nBestSize, nBestList, distinct);

    // read off the feature values and bleu scores for each sentence in the nbest list
    ChartTrellisPathList::const_iterator iter;
    for (iter = nBestList.begin() ; iter != nBestList.end() ; ++iter) {
    	const Moses::ChartTrellisPath &path = **iter;
    	featureValues.push_back(path.GetScoreBreakdown());
    	float bleuScore, dynBleuScore, realBleuScore;
    	dynBleuScore = getBleuScore(featureValues.back());  
    	realBleuScore = m_bleuScoreFeature->CalculateBleu(path.GetOutputPhrase());
    	bleuScore = realBleu ? realBleuScore : dynBleuScore; 
    	bleuScores.push_back(bleuScore);

    	//std::cout << "Score breakdown: " << path.GetScoreBreakdown() << endl;
    	float scoreWithoutBleu = path.GetTotalScore() - (bleuObjectiveWeight * bleuScoreWeight * bleuScore);
    	modelScores.push_back(scoreWithoutBleu);

    	if (iter != nBestList.begin())
    	  cerr << endl;
    	cerr << "Rank " << rank << ", epoch " << epoch << ", \"" << path.GetOutputPhrase() << "\", score: " 
    		 << scoreWithoutBleu << ", Bleu: " << bleuScore << ", total: " << path.GetTotalScore();
	if (m_bleuScoreFeature->Enabled() && realBleu)
	  cerr << " (d-bleu: " << dynBleuScore << ", r-bleu: " << realBleuScore << ") ";

    	// set bleu score to zero in the feature vector since we do not want to optimise its weight
    	setBleuScore(featureValues.back(), 0);
    }

    // prepare translations to return
    vector< vector<const Word*> > translations;
    for (iter = nBestList.begin() ; iter != nBestList.end() ; ++iter) {
        const ChartTrellisPath &path = **iter;
        Phrase phrase = path.GetOutputPhrase();

        vector<const Word*> translation;
        for (size_t pos = 0; pos < phrase.GetSize(); ++pos) {
        	const Word &word = phrase.GetWord(pos);
        	Word *newWord = new Word(word);
        	translation.push_back(newWord);
        }
        translations.push_back(translation);
    }

    return translations;
  }

  void MosesDecoder::outputNBestList(const std::string& source, size_t sentenceid,
  														size_t nBestSize, float bleuObjectiveWeight, float bleuScoreWeight,
  														bool distinctNbest, bool avgRefLength, string filename, ofstream& streamOut) {
  	StaticData &staticData = StaticData::InstanceNonConst();
  	bool chartDecoding = (staticData.GetSearchAlgorithm() == ChartDecoding);
  	initialize(staticData, source, sentenceid, bleuObjectiveWeight, bleuScoreWeight, avgRefLength, chartDecoding);
    const TranslationSystem& system = staticData.GetTranslationSystem(TranslationSystem::DEFAULT);

    if (chartDecoding) {
      m_chartManager = new ChartManager(*m_sentence, &system);
      m_chartManager->ProcessSentence();
      ChartTrellisPathList nBestList;
      m_chartManager->CalcNBest(nBestSize, nBestList, distinctNbest);

      cerr << "generate nbest list " << filename << endl;
      cerr << "not implemented.." << endl;
      exit(1);
    	if (filename != "") {
    		ofstream out(filename.c_str());
    		if (!out) {
    			ostringstream msg;
    			msg << "Unable to open " << filename;
    			throw runtime_error(msg.str());
    		}
    		// TODO: handle sentence id (for now always 0)
//    		OutputNBestList(const ChartTrellisPathList &nBestList, const ChartHypothesis *bestHypo, const TranslationSystem* system, long translationId, false)
//    		OutputNBest(out, nBestList, StaticData::Instance().GetOutputFactorOrder(),m_manager->GetTranslationSystem(), 0, false);
    		out.close();
    	}
    	else {
//    		OutputNBest(streamOut, nBestList, StaticData::Instance().GetOutputFactorOrder(),m_manager->GetTranslationSystem(), sentenceid, false);
    	}
    }
    else {
    	// run the decoder
      m_manager = new Moses::Manager(0,*m_sentence, staticData.GetSearchAlgorithm(), &system);
      m_manager->ProcessSentence();
      TrellisPathList nBestList;
      m_manager->CalcNBest(nBestSize, nBestList, distinctNbest);

      if (filename != "") {
    		ofstream out(filename.c_str());
    		if (!out) {
    			ostringstream msg;
    			msg << "Unable to open " << filename;
    			throw runtime_error(msg.str());
    		}
    		// TODO: handle sentence id (for now always 0)
    		//OutputNBest(out, nBestList, StaticData::Instance().GetOutputFactorOrder(),m_manager->GetTranslationSystem(), 0, false);
    		out.close();
      }
      else {
	//OutputNBest(streamOut, nBestList, StaticData::Instance().GetOutputFactorOrder(),m_manager->GetTranslationSystem(), sentenceid, false);
    	  streamOut.flush();
      }
    }
  }

  void MosesDecoder::initialize(StaticData& staticData, const std::string& source, size_t sentenceid,
      							float bleuObjectiveWeight, float bleuScoreWeight, bool avgRefLength, bool chartDecoding) {
  	m_sentence = new Sentence();
    stringstream in(source + "\n");
    const std::vector<FactorType> &inputFactorOrder = staticData.GetInputFactorOrder();
    m_sentence->Read(in,inputFactorOrder);

    // set weight of BleuScoreFeature
    //cerr << "Reload Bleu feature weight: " << bleuObjectiveWeight*bleuScoreWeight << " (" << bleuObjectiveWeight << "*" << bleuScoreWeight << ")" << endl;
    staticData.ReLoadBleuScoreFeatureParameter(bleuObjectiveWeight*bleuScoreWeight);

    m_bleuScoreFeature->SetCurrSourceLength((*m_sentence).GetSize());
    if (chartDecoding)
      m_bleuScoreFeature->SetCurrNormSourceLength((*m_sentence).GetSize()-2);
    else
      m_bleuScoreFeature->SetCurrNormSourceLength((*m_sentence).GetSize());
    	
    if (avgRefLength)
    	m_bleuScoreFeature->SetCurrAvgRefLength(sentenceid);
    else
    	m_bleuScoreFeature->SetCurrShortestRefLength(sentenceid);
    m_bleuScoreFeature->SetCurrReferenceNgrams(sentenceid);
  }

  float MosesDecoder::getBleuScore(const ScoreComponentCollection& scores) {
    return scores.GetScoreForProducer(m_bleuScoreFeature);
  }

  void MosesDecoder::setBleuScore(ScoreComponentCollection& scores, float bleu) {
    scores.Assign(m_bleuScoreFeature, bleu);
  }

  ScoreComponentCollection MosesDecoder::getWeights() {
    return StaticData::Instance().GetAllWeights();
  }

  void MosesDecoder::setWeights(const ScoreComponentCollection& weights) {
    StaticData::InstanceNonConst().SetAllWeights(weights);
  }

  void MosesDecoder::updateHistory(const vector<const Word*>& words) {
    m_bleuScoreFeature->UpdateHistory(words);
  }

  void MosesDecoder::updateHistory(const vector< vector< const Word*> >& words, vector<size_t>& sourceLengths, vector<size_t>& ref_ids, size_t rank, size_t epoch) {
	  m_bleuScoreFeature->UpdateHistory(words, sourceLengths, ref_ids, rank, epoch);
  }

  void MosesDecoder::printBleuFeatureHistory(std::ostream& out) {
  	m_bleuScoreFeature->PrintHistory(out);
  }

  size_t MosesDecoder::getClosestReferenceLength(size_t ref_id, int hypoLength) {
  	return m_bleuScoreFeature->GetClosestRefLength(ref_id, hypoLength);
  }

  size_t MosesDecoder::getShortestReferenceIndex(size_t ref_id) {
  	return m_bleuScoreFeature->GetShortestRefIndex(ref_id);
  }

  void MosesDecoder::setBleuParameters(bool disable, bool sentenceBleu, bool scaleByInputLength, bool scaleByAvgInputLength,
		  bool scaleByInverseLength, bool scaleByAvgInverseLength,
				       float scaleByX, float historySmoothing, size_t scheme, bool simpleHistoryBleu) {
	  m_bleuScoreFeature->SetBleuParameters(disable, sentenceBleu, scaleByInputLength, scaleByAvgInputLength,
			  scaleByInverseLength, scaleByAvgInverseLength,
			  scaleByX, historySmoothing, scheme, simpleHistoryBleu);
  }
} 

