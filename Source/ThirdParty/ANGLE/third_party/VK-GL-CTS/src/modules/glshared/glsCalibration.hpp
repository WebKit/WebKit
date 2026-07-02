#ifndef _GLSCALIBRATION_HPP
#define _GLSCALIBRATION_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Calibration tools.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "gluRenderContext.hpp"

#include <limits>

namespace deqp
{
namespace gls
{

struct LineParameters
{
    float offset;
    float coefficient;

    LineParameters(float offset_, float coefficient_) : offset(offset_), coefficient(coefficient_)
    {
    }
};

// Basic Theil-Sen linear estimate. Calculates median of all possible slope coefficients through two of the data points
// and median of offsets corresponding with the median slope
LineParameters theilSenLinearRegression(const std::vector<tcu::Vec2> &dataPoints);

struct LineParametersWithConfidence
{
    float offset;
    float offsetConfidenceUpper;
    float offsetConfidenceLower;

    float coefficient;
    float coefficientConfidenceUpper;
    float coefficientConfidenceLower;

    float confidence;
};

// Median-of-medians version of Theil-Sen estimate. Calculates median of medians of slopes through a point and all other points.
// Confidence interval is given as the range that contains the given fraction of all slopes/offsets
LineParametersWithConfidence theilSenSiegelLinearRegression(const std::vector<tcu::Vec2> &dataPoints,
                                                            float reportedConfidence);

struct MeasureState
{
    MeasureState(void) : maxNumFrames(0), frameShortcutTime(std::numeric_limits<float>::infinity()), numDrawCalls(0)
    {
    }

    void clear(void);
    void start(int maxNumFrames, float frameShortcutTime, int numDrawCalls);

    bool isDone(void) const;
    uint64_t getTotalTime(void) const;

    int maxNumFrames;
    float frameShortcutTime;
    int numDrawCalls;
    std::vector<uint64_t> frameTimes;
};

struct CalibrateIteration
{
    CalibrateIteration(int numDrawCalls_, float frameTime_) : numDrawCalls(numDrawCalls_), frameTime(frameTime_)
    {
    }

    CalibrateIteration(void) : numDrawCalls(0), frameTime(0.0f)
    {
    }

    int numDrawCalls;
    float frameTime;
};

struct CalibratorParameters
{
    CalibratorParameters(
        int numInitialCalls_,
        int maxCalibrateIterationFrames_, //!< Maximum (and default) number of frames per one calibrate iteration.
        float
            calibrateIterationShortcutThresholdMs_, //!< If the times of two consecutive frames exceed this, stop the iteration even if maxCalibrateIterationFrames isn't reached.
        int maxCalibrateIterations_, float targetFrameTimeMs_, float frameTimeCapMs_, float targetMeasureDurationMs_)
        : numInitialCalls(numInitialCalls_)
        , maxCalibrateIterationFrames(maxCalibrateIterationFrames_)
        , calibrateIterationShortcutThreshold(1000.0f * calibrateIterationShortcutThresholdMs_)
        , maxCalibrateIterations(maxCalibrateIterations_)
        , targetFrameTimeUs(1000.0f * targetFrameTimeMs_)
        , frameTimeCapUs(1000.0f * frameTimeCapMs_)
        , targetMeasureDurationUs(1000.0f * targetMeasureDurationMs_)
    {
    }

    int numInitialCalls;
    int maxCalibrateIterationFrames;
    float calibrateIterationShortcutThreshold;
    int maxCalibrateIterations;
    float targetFrameTimeUs;
    float frameTimeCapUs;
    float targetMeasureDurationUs;
};

class TheilSenCalibrator
{
public:
    enum State
    {
        STATE_RECOMPUTE_PARAMS = 0,
        STATE_MEASURE,
        STATE_FINISHED,

        STATE_LAST
    };

    TheilSenCalibrator(void);
    TheilSenCalibrator(const CalibratorParameters &params);
    ~TheilSenCalibrator(void);

    void clear(void);
    void clear(const CalibratorParameters &params);

    State getState(void) const;
    int getCallCount(void) const
    {
        return m_measureState.numDrawCalls;
    }

    // Should be called when getState() returns STATE_RECOMPUTE_PARAMS
    void recomputeParameters(void);

    // Should be called when getState() returns STATE_MEASURE
    void recordIteration(uint64_t frameTime);

    const CalibratorParameters &getParameters(void) const
    {
        return m_params;
    }
    const MeasureState &getMeasureState(void) const
    {
        return m_measureState;
    }
    const std::vector<CalibrateIteration> &getCalibrationInfo(void) const
    {
        return m_calibrateIterations;
    }

private:
    enum InternalState
    {
        INTERNALSTATE_CALIBRATING = 0,
        INTERNALSTATE_RUNNING,
        INTERNALSTATE_FINISHED,

        INTERNALSTATE_LAST
    };

    CalibratorParameters m_params;

    InternalState m_state;
    MeasureState m_measureState;

    std::vector<CalibrateIteration> m_calibrateIterations;
};

void logCalibrationInfo(tcu::TestLog &log, const TheilSenCalibrator &calibrator);

} // namespace gls
} // namespace deqp

#endif // _GLSCALIBRATION_HPP
