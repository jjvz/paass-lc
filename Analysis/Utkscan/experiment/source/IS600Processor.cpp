///@file IS600Processor.cpp
///@brief A class to process data from ISOLDE 599 and 600 experiments using
/// VANDLE.
///@author S. V. Paulauskas
///@date July 14, 2015
#include <fstream>
#include <iostream>

#include <cmath>

#include "BarBuilder.hpp"
#include "DammPlotIds.hpp"
#include "DoubleBetaProcessor.hpp"
#include "DetectorDriver.hpp"
#include "CloverProcessor.hpp"
#include "IS600Processor.hpp"
#include "VandleProcessor.hpp"

static double tof_;
static double qdc_;

namespace dammIds {
    namespace experiment {
        const int DD_DEBUGGING0 = 0; //!<QDC CTof- No Tape Move
        const int DD_DEBUGGING1 = 1; //!<QDC ToF Ungated
        const int DD_DEBUGGING2 = 2; //!<Cor ToF vs. Gamma E
        const int DD_DEBUGGING3 = 3; //!<Vandle Multiplicity
        const int DD_DEBUGGING4 = 4; //!<QDC vs Cor Tof Mult1
        const int DD_DEBUGGING5 = 5; //!<Mult2 Sym Plot Tof
        const int DD_DEBUGGING6 = 6; //!<LaBr3 RAW
        const int DD_PROTONBETA2TDIFF_VS_BETA2EN = 13; //!< BetaProton Tdiff vs. Beta Energy
        const int D_ENERGY = 14; //!< Gamma singles ungated
        const int D_ENERGYBETA = 15; //!< Gamma singles beta gated
        const int DD_PROTONGAMMATDIFF_VS_GAMMAEN = 16; //!< GammaProton TDIFF vs. Gamma Energy
    }
}//namespace dammIds

using namespace std;
using namespace dammIds::experiment;

void IS600Processor::DeclarePlots(void) {
    histo.DeclareHistogram2D(DD_DEBUGGING0, SC, SD, "QDC CTof- No Tape Move");
    histo.DeclareHistogram2D(DD_DEBUGGING1, SC, SD, "QDC ToF Ungated");
    histo.DeclareHistogram2D(DD_DEBUGGING2, SC, SC, "Cor ToF vs. Gamma E");
    histo.DeclareHistogram1D(DD_DEBUGGING3, S7, "Vandle Multiplicity");
    histo.DeclareHistogram2D(DD_DEBUGGING4, SC, SC, "QDC vs Cor Tof Mult1");
    histo.DeclareHistogram2D(DD_DEBUGGING5, SC, SC, "Mult2 Sym Plot Tof ");
    histo.DeclareHistogram1D(DD_DEBUGGING6, SE, "LaBr3 RAW");
    histo.DeclareHistogram2D(DD_PROTONBETA2TDIFF_VS_BETA2EN, SD, SA,
                       "BetaProton Tdiff vs. Beta Energy");

    const int energyBins1 = SD;
    histo.DeclareHistogram1D(D_ENERGY, energyBins1,
                       "Gamma singles ungated");
    histo.DeclareHistogram1D(D_ENERGYBETA, energyBins1,
                       "Gamma singles beta gated");
    histo.DeclareHistogram2D(DD_PROTONGAMMATDIFF_VS_GAMMAEN,
                       SD, SB, "GammaProton TDIFF vs. Gamma Energy");
}

IS600Processor::IS600Processor() : EventProcessor(OFFSET, RANGE,
                                                  "IS600PRocessor") {
    associatedTypes.insert("vandle");
    associatedTypes.insert("labr3");
    associatedTypes.insert("beta");
    associatedTypes.insert("ge");

    stringstream name;
    name << Globals::get()->GetOutputPath()
         << Globals::get()->GetOutputFileName() << ".dat";
    outstream = new ofstream(name.str().c_str());

    stringstream rootname;
    rootname << Globals::get()->GetOutputPath()
             << Globals::get()->GetOutputFileName() << "-IS600.root";
    cout << rootname.str() << endl;
    rootfile_ = new TFile(rootname.str().c_str(), "RECREATE");
    roottree_ = new TTree("vandle", "");
    roottree_->Branch("tof", &tof_, "tof/D");
    roottree_->Branch("qdc", &qdc_, "qdc/D");
    qdctof_ = new TH2D("qdctof", "", 1000, -100, 900, 16000, 0, 16000);
    vsize_ = new TH1D("vsize", "", 40, 0, 40);
}

IS600Processor::~IS600Processor() {
    outstream->close();
    delete (outstream);
    rootfile_->Write();
    rootfile_->Close();
    delete (rootfile_);
}

///We do nothing here since we're completely dependent on the resutls of others
bool IS600Processor::PreProcess(RawEvent &event) {
    if (!EventProcessor::PreProcess(event))
        return (false);
    return (true);
}

bool IS600Processor::Process(RawEvent &event) {
    if (!EventProcessor::Process(event))
        return (false);
    double plotMult_ = 2;
    double plotOffset_ = 1000;

    BarMap vbars, betas;
    map<unsigned int, pair<double, double> > lrtBetas;
    vector < ChanEvent * > geEvts;
    vector <vector<AddBackEvent>> geAddback;

    if (event.GetSummary("vandle")->GetList().size() != 0)
        vbars = ((VandleProcessor *) DetectorDriver::get()->
                GetProcessor("VandleProcessor"))->GetBars();
    if (event.GetSummary("beta:double")->GetList().size() != 0) {
        betas = ((DoubleBetaProcessor *) DetectorDriver::get()->
                GetProcessor("DoubleBetaProcessor"))->GetBars();
        lrtBetas = ((DoubleBetaProcessor *) DetectorDriver::get()->
                GetProcessor("DoubleBetaProcessor"))->GetLowResBars();
    }
    if (event.GetSummary("ge")->GetList().size() != 0) {
        geEvts = ((CloverProcessor *) DetectorDriver::get()->
                GetProcessor("CloverProcessor"))->GetGeEvents();
        geAddback = ((CloverProcessor *) DetectorDriver::get()->
                GetProcessor("CloverProcessor"))->GetAddbackEvents();
    }
    static const vector<ChanEvent *> &labr3Evts =
            event.GetSummary("labr3:mrbig")->GetList();

    vsize_->Fill(vbars.size());

    //Obtain some useful logic statuses
    double lastProtonTime =
            TreeCorrelator::get()->place("logic_t1_0")->last().time;
    bool isTapeMoving = TreeCorrelator::get()->place("TapeMove")->status();

    int bananaNum = 2;
    bool hasMultOne = vbars.size() == 1;
    bool hasMultTwo = vbars.size() == 2;
    //bool isFirst = true;

    histo.Plot(DD_DEBUGGING3, vbars.size());

    //Begin processing for VANDLE bars
    for (BarMap::iterator it = vbars.begin(); it != vbars.end(); it++) {
        TimingDefs::TimingIdentifier barId = (*it).first;
        BarDetector bar = (*it).second;

        if (!bar.GetHasEvent() || bar.GetType() == "small")
            continue;

        unsigned int barLoc = barId.first;
        TimingCalibration cal = bar.GetCalibration();

        for (BarMap::iterator itStart = betas.begin();
             itStart != betas.end(); itStart++) {
            unsigned int startLoc = (*itStart).first.first;
            BarDetector start = (*itStart).second;
            if (!start.GetHasEvent())
                continue;

            double tofOffset = cal.GetTofOffset(startLoc);
            double tof = bar.GetCorTimeAve() -
                         start.GetCorTimeAve() + tofOffset;

            double corTof =
                    ((VandleProcessor *) DetectorDriver::get()->
                            GetProcessor("VandleProcessor"))->
                            CorrectTOF(tof, bar.GetFlightPath(), cal.GetZ0());

            bool inPeel = histo.BananaTest(bananaNum,
                                           corTof * plotMult_ + plotOffset_,
                                           bar.GetQdc());
            bool isLowStart = start.GetQdc() < 300;

            *outstream << tof << " " << bar.GetQdc() << endl;

            qdctof_->Fill(tof, bar.GetQdc());
            qdc_ = bar.GetQdc();
            tof_ = tof;
            roottree_->Fill();
            qdc_ = tof_ = -9999;

            histo.Plot(DD_DEBUGGING1, tof * plotMult_ + plotOffset_, bar.GetQdc());
            if (!isTapeMoving && !isLowStart)
                histo.Plot(DD_DEBUGGING0, corTof * plotMult_ + plotOffset_,
                     bar.GetQdc());
            if (hasMultOne)
                histo.Plot(DD_DEBUGGING4, corTof * plotMult_ + plotOffset_,
                     bar.GetQdc());

            ///Starting to look for 2n coincidences in VANDLE
            BarMap::iterator itTemp = it;
            itTemp++;

            for (BarMap::iterator it2 = itTemp; it2 != vbars.end(); it2++) {
                TimingDefs::TimingIdentifier barId2 = (*it2).first;
                BarDetector bar2 = (*it2).second;

                if (!bar.GetHasEvent())
                    continue;

                unsigned int barLoc2 = barId2.first;

                bool isAdjacent = abs((int) barLoc2 - (int) barLoc) < 1;

                TimingCalibration cal2 = bar2.GetCalibration();

                double tofOffset2 = cal2.GetTofOffset(startLoc);
                double tof2 = bar2.GetCorTimeAve() -
                              start.GetCorTimeAve() + tofOffset2;

                double corTof2 =
                        ((VandleProcessor *) DetectorDriver::get()->
                                GetProcessor("VandleProcessor"))->
                                CorrectTOF(tof2, bar2.GetFlightPath(),
                                           cal2.GetZ0());

                bool inPeel2 = histo.BananaTest(bananaNum,
                                                corTof2 * plotMult_ +
                                                plotOffset_,
                                                bar2.GetQdc());

                if (hasMultTwo && inPeel && inPeel2 && !isAdjacent) {
                    histo.Plot(DD_DEBUGGING5, corTof * plotMult_ + plotOffset_,
                         corTof2 * plotMult_ + plotOffset_);
                    histo.Plot(DD_DEBUGGING5, corTof2 * plotMult_ + plotOffset_,
                         corTof * plotMult_ + plotOffset_);
                }
            }
            ///End 2n coincidence routine

            // if (geSummary_ && notPrompt && hasMultOne) {
            //     if (geSummary_->GetMult() > 0) {
            //         const vector<ChanEvent *> &geList = geSummary_->GetList();
            //         for (vector<ChanEvent *>::const_iterator itGe = geList.begin();
            //             itGe != geList.end(); itGe++) {
            //             double calEnergy = (*itGe)->GetCalEnergy();
            // 		histo.Plot(DD_DEBUGGING2, calEnergy, corTof*plotMult_+plotOffset_);
            //         }
            //     } else {
            // 	  //histo.Plot(DD_TQDCAVEVSTOF_VETO+histTypeOffset, tof, bar.GetQdc());
            // 	  //histo.Plot(DD_TOFBARS_VETO+histTypeOffset, tof, barPlusStartLoc);
            //     }
            //}
        } // for(TimingMap::iterator itStart
    } //(BarMap::iterator itBar
    //End processing for VANDLE bars

    //-------------- LaBr3 Processing ---------------
    for (vector<ChanEvent *>::const_iterator it = labr3Evts.begin();
         it != labr3Evts.end(); it++)
        histo.Plot(DD_DEBUGGING6, (*it)->GetEnergy());


    //------------------ Double Beta Processing --------------
    for (map<unsigned int, pair<double, double> >
         ::iterator it = lrtBetas.begin();
         it != lrtBetas.end();
         it++)
        histo.Plot(DD_PROTONBETA2TDIFF_VS_BETA2EN, it->second.second,
             (it->second.first - lastProtonTime) /
             (10e-3 / Globals::get()->GetClockInSeconds()));


    //----------------- GE Processing -------------------
    bool hasBeta = TreeCorrelator::get()->place("Beta")->status();
    double clockInSeconds = Globals::get()->GetClockInSeconds();
    // plot with 10 ms bins
    const double plotResolution = 10e-3 / clockInSeconds;

    for (vector<ChanEvent *>::iterator it1 = geEvts.begin();
         it1 != geEvts.end(); ++it1) {
        ChanEvent *chan = *it1;

        double gEnergy = chan->GetCalibratedEnergy();
        double gTime = chan->GetWalkCorrectedTime();
        if (gEnergy < 10.) //hard coded fix later.
            continue;

        histo.Plot(D_ENERGY, gEnergy);
        if (hasBeta)
            histo.Plot(D_ENERGYBETA, gEnergy);
        histo.Plot(DD_PROTONGAMMATDIFF_VS_GAMMAEN, gEnergy,
             (gTime - lastProtonTime) / plotResolution);
    }

    EndProcess();
    return (true);
}
