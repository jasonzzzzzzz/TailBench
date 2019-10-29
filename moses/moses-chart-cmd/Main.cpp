// $Id$

/***********************************************************************
Moses - factored phrase-based language decoder
Copyright (c) 2006 University of Edinburgh
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
			this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
			this list of conditions and the following disclaimer in the documentation
			and/or other materials provided with the distribution.
    * Neither the name of the University of Edinburgh nor the names of its contributors
			may be used to endorse or promote products derived from this software
			without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/

// example file on how to use moses library

#ifdef WIN32
// Include Visual Leak Detector
//#include <vld.h>
#endif

#include <exception>
#include <fstream>
#include "Main.h"
#include "TranslationAnalysis.h"
#include "mbr.h"
#include "IOWrapper.h"

#include "moses/DummyScoreProducers.h"
#include "moses/FactorCollection.h"
#include "moses/Manager.h"
#include "moses/Phrase.h"
#include "moses/Util.h"
#include "moses/Timer.h"
#include "moses/Sentence.h"
#include "moses/ConfusionNet.h"
#include "moses/WordLattice.h"
#include "moses/TreeInput.h"
#include "moses/ThreadPool.h"
#include "moses/ChartManager.h"
#include "moses/ChartHypothesis.h"
#include "moses/ChartTrellisPath.h"
#include "moses/ChartTrellisPathList.h"
#include "moses/Incremental.h"

#include "util/usage.hh"

using namespace std;
using namespace Moses;
using namespace MosesChartCmd;

/**
  * Translates a sentence.
 **/
class TranslationTask : public Task
{
public:
  TranslationTask(InputType *source, IOWrapper &ioWrapper)
    : m_source(source)
    , m_ioWrapper(ioWrapper)
  {}

  ~TranslationTask() {
    delete m_source;
  }

  void Run() {
    const StaticData &staticData = StaticData::Instance();
    const TranslationSystem &system = staticData.GetTranslationSystem(TranslationSystem::DEFAULT);
    const size_t translationId = m_source->GetTranslationId();

    VERBOSE(2,"\nTRANSLATING(" << translationId << "): " << *m_source);

    if (staticData.GetSearchAlgorithm() == ChartIncremental) {
      Incremental::Manager manager(*m_source, system);
      const std::vector<search::Applied> &nbest = manager.ProcessSentence();
      if (!nbest.empty()) {
        m_ioWrapper.OutputBestHypo(nbest[0], translationId);
      } else {
        m_ioWrapper.OutputBestNone(translationId);
      }
      if (staticData.GetNBestSize() > 0)
        m_ioWrapper.OutputNBestList(nbest, system, translationId);
      return;
    }

    ChartManager manager(*m_source, &system);
    manager.ProcessSentence();

    CHECK(!staticData.UseMBR());

    // 1-best
    const ChartHypothesis *bestHypo = manager.GetBestHypothesis();
    m_ioWrapper.OutputBestHypo(bestHypo, translationId);
    IFVERBOSE(2) {
      PrintUserTime("Best Hypothesis Generation Time:");
    }

    if (!staticData.GetAlignmentOutputFile().empty()) {
      m_ioWrapper.OutputAlignment(translationId, bestHypo);
    }

    if (staticData.IsDetailedTranslationReportingEnabled()) {
      const Sentence &sentence = dynamic_cast<const Sentence &>(*m_source);
      m_ioWrapper.OutputDetailedTranslationReport(bestHypo, sentence, translationId);
    }

    // n-best
    size_t nBestSize = staticData.GetNBestSize();
    if (nBestSize > 0) {
      VERBOSE(2,"WRITING " << nBestSize << " TRANSLATION ALTERNATIVES TO " << staticData.GetNBestFilePath() << endl);
      ChartTrellisPathList nBestList;
      manager.CalcNBest(nBestSize, nBestList,staticData.GetDistinctNBest());
      m_ioWrapper.OutputNBestList(nBestList, &system, translationId);
      IFVERBOSE(2) {
        PrintUserTime("N-Best Hypotheses Generation Time:");
      }
    }

    if (staticData.GetOutputSearchGraph()) {
      std::ostringstream out;
      manager.GetSearchGraph(translationId, out);
      OutputCollector *oc = m_ioWrapper.GetSearchGraphOutputCollector();
      CHECK(oc);
      oc->Write(translationId, out.str());
    }

    IFVERBOSE(2) {
      PrintUserTime("Sentence Decoding Time:");
    }
    manager.CalcDecoderStatistics();
  }

private:
  // Non-copyable: copy constructor and assignment operator not implemented.
  TranslationTask(const TranslationTask &);
  TranslationTask &operator=(const TranslationTask &);

  InputType *m_source;
  IOWrapper &m_ioWrapper;
};

bool ReadInput(IOWrapper &ioWrapper, InputTypeEnum inputType, InputType*& source)
{
  delete source;
  switch(inputType) {
  case SentenceInput:
    source = ioWrapper.GetInput(new Sentence);
    break;
  case ConfusionNetworkInput:
    source = ioWrapper.GetInput(new ConfusionNet);
    break;
  case WordLatticeInput:
    source = ioWrapper.GetInput(new WordLattice);
    break;
  case TreeInputType:
    source = ioWrapper.GetInput(new TreeInput);
    break;
  default:
    TRACE_ERR("Unknown input type: " << inputType << "\n");
  }
  return (source ? true : false);
}
static void PrintFeatureWeight(const FeatureFunction* ff)
{
  size_t numScoreComps = ff->GetNumScoreComponents();
  if (numScoreComps != ScoreProducer::unlimited) {
    vector<float> values = StaticData::Instance().GetAllWeights().GetScoresForProducer(ff);
    for (size_t i = 0; i < numScoreComps; ++i) 
      cout << ff->GetScoreProducerDescription() <<  " "
           << ff->GetScoreProducerWeightShortName() << " "
           << values[i] << endl;
  }
  else {
  	if (ff->GetSparseProducerWeight() == 1)
  		cout << ff->GetScoreProducerDescription() << " " <<
  		ff->GetScoreProducerWeightShortName() << " sparse" << endl;
  	else
  		cout << ff->GetScoreProducerDescription() << " " <<
  		ff->GetScoreProducerWeightShortName() << " " << ff->GetSparseProducerWeight() << endl;
  }
}

static void ShowWeights()
{
  cout.precision(6);
  const StaticData& staticData = StaticData::Instance();
  const TranslationSystem& system = staticData.GetTranslationSystem(TranslationSystem::DEFAULT);
  //This has to match the order in the nbest list

  //LMs
  const LMList& lml = system.GetLanguageModels();
  LMList::const_iterator lmi = lml.begin();
  for (; lmi != lml.end(); ++lmi) {
    PrintFeatureWeight(*lmi);
  }

  //sparse stateful ffs
  const vector<const StatefulFeatureFunction*>& sff = system.GetStatefulFeatureFunctions();
  for( size_t i=0; i<sff.size(); i++ ) {
    if (sff[i]->GetNumScoreComponents() == ScoreProducer::unlimited) {
      PrintFeatureWeight(sff[i]);
    }
  }

  // translation components - phrase dicts
  const vector<PhraseDictionaryFeature*>& pds = system.GetPhraseDictionaries();
  for( size_t i=0; i<pds.size(); i++ ) {
    PrintFeatureWeight(pds[i]);
  }

  //word penalty
  PrintFeatureWeight(system.GetWordPenaltyProducer());

  //generation dicts
  const vector<GenerationDictionary*>& gds = system.GetGenerationDictionaries();
  for( size_t i=0; i<gds.size(); i++ ) {
    PrintFeatureWeight(gds[i]);
  }

  //sparse stateless ffs
  const vector<const StatelessFeatureFunction*>& slf = system.GetStatelessFeatureFunctions();
  for( size_t i=0; i<slf.size(); i++ ) {
    if (slf[i]->GetNumScoreComponents() == ScoreProducer::unlimited) {
      PrintFeatureWeight(slf[i]);
    }
  }


}


int main(int argc, char* argv[])
{
  try {
    IFVERBOSE(1) {
      TRACE_ERR("command: ");
      for(int i=0; i<argc; ++i) TRACE_ERR(argv[i]<<" ");
      TRACE_ERR(endl);
    }

    IOWrapper::FixPrecision(cout);
    IOWrapper::FixPrecision(cerr);

    // load data structures
    Parameter parameter;
    if (!parameter.LoadParam(argc, argv)) {
      return EXIT_FAILURE;
    }

    const StaticData &staticData = StaticData::Instance();
    if (!StaticData::LoadDataStatic(&parameter, argv[0]))
      return EXIT_FAILURE;

    if (parameter.isParamSpecified("show-weights")) {
      ShowWeights();
      exit(0);
    }
  
    CHECK(staticData.IsChart());
  
    // set up read/writing class
    IOWrapper *ioWrapper = GetIOWrapper(staticData);
  
    // check on weights
    const ScoreComponentCollection& weights = staticData.GetAllWeights();
    IFVERBOSE(2) {
      TRACE_ERR("The global weight vector looks like this: ");
      TRACE_ERR(weights);
      TRACE_ERR("\n");
    }

    if (ioWrapper == NULL)
      return EXIT_FAILURE;

#ifdef WITH_THREADS
    ThreadPool pool(staticData.ThreadCount());
#endif
  
    // read each sentence & decode
    InputType *source=0;
    while(ReadInput(*ioWrapper,staticData.GetInputType(),source)) {
      IFVERBOSE(1)
      ResetUserTime();
      TranslationTask *task = new TranslationTask(source, *ioWrapper);
      source = NULL;  // task will delete source
#ifdef WITH_THREADS
      pool.Submit(task);  // pool will delete task
#else
      task->Run();
      delete task;
#endif
    }
  
#ifdef WITH_THREADS
    pool.Stop(true);  // flush remaining jobs
#endif
  
    delete ioWrapper;
  
    IFVERBOSE(1)
    PrintUserTime("End.");
  
  } catch (const std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  IFVERBOSE(1) util::PrintUsage(std::cerr);

#ifndef EXIT_RETURN
  //This avoids that detructors are called (it can take a long time)
  exit(EXIT_SUCCESS);
#else
  return EXIT_SUCCESS;
#endif
}

IOWrapper *GetIOWrapper(const StaticData &staticData)
{
  IOWrapper *ioWrapper;
  const std::vector<FactorType> &inputFactorOrder = staticData.GetInputFactorOrder()
      ,&outputFactorOrder = staticData.GetOutputFactorOrder();
  FactorMask inputFactorUsed(inputFactorOrder);

  // io
  if (staticData.GetParam("input-file").size() == 1) {
    VERBOSE(2,"IO from File" << endl);
    string filePath = staticData.GetParam("input-file")[0];

    ioWrapper = new IOWrapper(inputFactorOrder, outputFactorOrder, inputFactorUsed
                              , staticData.GetNBestSize()
                              , staticData.GetNBestFilePath()
                              , filePath);
  } else {
    VERBOSE(1,"IO from STDOUT/STDIN" << endl);
    ioWrapper = new IOWrapper(inputFactorOrder, outputFactorOrder, inputFactorUsed
                              , staticData.GetNBestSize()
                              , staticData.GetNBestFilePath());
  }
  ioWrapper->ResetTranslationId();

  IFVERBOSE(1)
  PrintUserTime("Created input-output object");

  return ioWrapper;
}
