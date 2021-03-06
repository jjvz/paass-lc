/*! \file DetectorDriver.hpp
 *  \brief Main Driver for event processing.
 * \author S. N. Liddick, D. Miller, K. Miernik, S. V. Paulauskas
 * \date July 2, 2007
 *
 * The main analysis program.  A complete event is create in PixieStd
 * passed into this class. In this file are the details for experimental
 * processing of a raw event created by ScanList() in PixieStd.cpp.  Event
 * processing includes things that do not change from experiment to experiment
 * (such as energy calibration and raw parameter plotting) and things that do
 * (differences between MTC and RMS experiment, for example).
 * See manual for further details.
 *
 * SNL - 7-2-07
 * SNL - 7-12-07
 *       Add root analysis. If the ROOT program has been
 *       detected on the computer system the and the
 *       makefile has the useroot flag declared, ROOT
 *       analysis will be included.
 * DTM - Oct. '09
 *      Significant structural/cosmetic changes. Event processing is
 *      now primarily handled by individual event processors which
 *     handle their own DetectorDrivers
 *
 * SVP - Oct. '10
 *       Added the VandleProcessor for use with VANDLE.
 *       Added the PulserProcessor for use with Pulsers.
 *       Added the WaveformProcessor to determine ps time resolutions.
 * KM  - Dec. '12, Jan. '13
 *       Huge changes due to switching to XML configuration file.
 *       See git commits comments.
*/
#ifndef __DETECTORDRIVER_HPP_
#define __DETECTORDRIVER_HPP_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Calibrator.hpp"
#include "ChanEvent.hpp"
#include "Globals.hpp"
#include "Messenger.hpp"
#include "Plots.hpp"
#include "WalkCorrector.hpp"

class Calibration;

class RawEvent;

class EventProcessor;

class TraceAnalyzer;

/*! \brief DetectorDriver controls event processing

  This class controls the processing of each event and includes the
  calibration of all raw energies, the plotting of raw and calibrated
  energies, and any experiment specific processing requirements
*/
class DetectorDriver {
public:
    /*! \return Instance is created upon first call */
    static DetectorDriver *get();

    const WalkCorrector *walk_; //!< Instance of the walk correction
    const Calibrator *cali_;//!< Instance of the calibrator
    Plots histo_;//!< Instance of the histogram class

    /*! \brief Control of the event processing
    *
    * The ProcessEvent() function is called from ScanList() in PixieStd.cpp
    * after an event has been constructed. This function is passed the mode
    * the analysis is currently in (the options are either "scan" or
    * "standaloneroot").  The function checks the thresholds for the individual
    * channels in the event and calibrates their energies.
    * The raw and calibrated energies are plotted if the appropriate DAMM spectra
    * have been created.  Then experiment specific processing is performed.
    * Currently, both RMS and MTC processing is available.  After all processing
    * has occured, appropriate plotting routines are called.
    * \param [in] rawev : the raw event to process */
    void ProcessEvent(RawEvent &rawev);

    /*! \brief Check threshold and calibrate each channel.
     * Check the thresholds and calibrate the energy for each channel using the
     * calibrations contained in the calibration vector filled during ReadCal()
     * \param [in] chan : the channel to do the calibration on
     * \param [in] rawev : the raw event to write the information into
     * \return an unused integer (maybe change to void) */
    int ThreshAndCal(ChanEvent *chan, RawEvent &rawev);

    /*! Called from PixieStd.cpp during initialization.
     * The calibration file Config.xml is read using the function ReadCal() and
     * checked to make sure that all channels have a calibration.
     * \param [in] rawev : the raw event to initialize with */
    void Init(RawEvent &rawev);

    /*! Plot the raw energies of each channel into the damm spectrum number
     * assigned to it in the map file with an offset as defined in
     * DammPlotIds.hpp
     * \param [in] chan : the channel to plot
     * \return an unused integer maybe use void*/
    int PlotRaw(const ChanEvent *chan);

    /*! Plot the calibrated energies of each channel into the damm spectrum
     * number assigned to it in the map file with an offset as defined in
     * DammPlotIds.hpp
     * \return an unused integer maybe use void*/
    int PlotCal(const ChanEvent *);

    /** Declares the raw plots that are used, and loops over all the declare
     * plots methods in all of the analyzers and processors. */
    void DeclarePlots();

    /** Use Exceptions to throw an exception here if sanity check was
     * not succesful */
    void SanityCheck(void) const {};

    /** Correlates the pixie clock to the wall clock
     * \param [in] d : the pixie time to correlate
     * \param [in] t : the wall time to correlate */
    void CorrelateClock(double d, time_t t) {
        pixieToWallClock = std::make_pair(d, t);
    }

    /** \return The wall time
     * \param [in] d : the pixie time to convert to wall time */
    time_t GetWallTime(double d) const {
        return (time_t) ((d - pixieToWallClock.first) *
                         Globals::get()->GetClockInSeconds() +
                         pixieToWallClock.second);
    }

    /** \return the list of the Event Processors in the analysis */
    const std::vector<EventProcessor *> &GetProcessors(void) const {
        return vecProcess;
    }

    /** \return The requested event processor
     * \param [in] name : the name of the processor to return */
    EventProcessor *GetProcessor(const std::string &name) const;

    /** \return the set of detectors used in the analysis */
    const std::set<std::string> &GetUsedDetectors(void) const;

    ///Sets the processor list
    ///@param[in] a : The vector containing the pointer to the event processors
    void SetEventProcessors(const std::vector<EventProcessor *> &a) {
        vecProcess = a;
    }

    ///Sets the analyzer list
    ///@param[in] a : The vector containing the pointer to the Trace Analyzers
    void SetTraceAnalyzers(const std::vector<TraceAnalyzer *> &a) {
        vecAnalyzer = a;
    }

    /** Default Destructor */
    virtual ~DetectorDriver();

private:
    /** Constructor that initializes the various processors and analyzers. */
    DetectorDriver();

    DetectorDriver(const DetectorDriver &); //!< Overloaded constructor
    DetectorDriver &operator=(DetectorDriver const &);//!< Equality constructor
    static DetectorDriver *instance;//!< The only instance of DetectorDriver

    std::vector<EventProcessor *> vecProcess; /**< vector of processors to handle each event */

    std::vector<TraceAnalyzer *> vecAnalyzer; /**< object which analyzes traces of channels to extract
                   energy and time information */
    std::set<std::string> knownDetectors; /**< list of valid detectors that can
                   be used as detector types */
    std::string cfg_; //!< The configuration file to read
    std::pair<double, time_t> pixieToWallClock; /**< rough estimate of pixie to wall clock */
};

#endif // __DETECTORDRIVER_HPP_
