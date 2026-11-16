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

#include "glsCalibration.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"
#include "deStringUtil.hpp"
#include "deMath.h"
#include "deClock.h"

#include <algorithm>
#include <limits>

using std::string;
using std::vector;
using tcu::TestLog;
using tcu::TestNode;
using tcu::Vec2;
using namespace glu;

namespace deqp
{
namespace gls
{

// Reorders input arbitrarily, linear complexity and no allocations
template <typename T>
float destructiveMedian(vector<T> &data)
{
    const typename vector<T>::iterator mid = data.begin() + data.size() / 2;

    std::nth_element(data.begin(), mid, data.end());

    if (data.size() % 2 == 0) // Even number of elements, need average of two centermost elements
        return (*mid + *std::max_element(data.begin(), mid)) *
               0.5f; // Data is partially sorted around mid, mid is half an item after center
    else
        return *mid;
}

LineParameters theilSenLinearRegression(const std::vector<tcu::Vec2> &dataPoints)
{
    const float epsilon = 1e-6f;

    const int numDataPoints = (int)dataPoints.size();
    vector<float> pairwiseCoefficients;
    vector<float> pointwiseOffsets;
    LineParameters result(0.0f, 0.0f);

    // Compute the pairwise coefficients.
    for (int i = 0; i < numDataPoints; i++)
    {
        const Vec2 &ptA = dataPoints[i];

        for (int j = 0; j < i; j++)
        {
            const Vec2 &ptB = dataPoints[j];

            if (de::abs(ptA.x() - ptB.x()) > epsilon)
                pairwiseCoefficients.push_back((ptA.y() - ptB.y()) / (ptA.x() - ptB.x()));
        }
    }

    // Find the median of the pairwise coefficients.
    // \note If there are no data point pairs with differing x values, the coefficient variable will stay zero as initialized.
    if (!pairwiseCoefficients.empty())
        result.coefficient = destructiveMedian(pairwiseCoefficients);

    // Compute the offsets corresponding to the median coefficient, for all data points.
    for (int i = 0; i < numDataPoints; i++)
        pointwiseOffsets.push_back(dataPoints[i].y() - result.coefficient * dataPoints[i].x());

    // Find the median of the offsets.
    // \note If there are no data points, the offset variable will stay zero as initialized.
    if (!pointwiseOffsets.empty())
        result.offset = destructiveMedian(pointwiseOffsets);

    return result;
}

// Sample from given values using linear interpolation at a given position as if values were laid to range [0, 1]
template <typename T>
static float linearSample(const std::vector<T> &values, float position)
{
    DE_ASSERT(position >= 0.0f);
    DE_ASSERT(position <= 1.0f);

    const int maxNdx     = (int)values.size() - 1;
    const float floatNdx = (float)maxNdx * position;
    const int lowerNdx   = (int)deFloatFloor(floatNdx);
    const int higherNdx  = lowerNdx + (lowerNdx == maxNdx ? 0 : 1); // Use only last element if position is 1.0
    const float interpolationFactor = floatNdx - (float)lowerNdx;

    DE_ASSERT(lowerNdx >= 0 && lowerNdx < (int)values.size());
    DE_ASSERT(higherNdx >= 0 && higherNdx < (int)values.size());
    DE_ASSERT(interpolationFactor >= 0 && interpolationFactor < 1.0f);

    return tcu::mix((float)values[lowerNdx], (float)values[higherNdx], interpolationFactor);
}

LineParametersWithConfidence theilSenSiegelLinearRegression(const std::vector<tcu::Vec2> &dataPoints,
                                                            float reportedConfidence)
{
    DE_ASSERT(!dataPoints.empty());

    // Siegel's variation

    const float epsilon     = 1e-6f;
    const int numDataPoints = (int)dataPoints.size();
    std::vector<float> medianSlopes;
    std::vector<float> pointwiseOffsets;
    LineParametersWithConfidence result;

    // Compute the median slope via each element
    for (int i = 0; i < numDataPoints; i++)
    {
        const tcu::Vec2 &ptA = dataPoints[i];
        std::vector<float> slopes;

        slopes.reserve(numDataPoints);

        for (int j = 0; j < numDataPoints; j++)
        {
            const tcu::Vec2 &ptB = dataPoints[j];

            if (de::abs(ptA.x() - ptB.x()) > epsilon)
                slopes.push_back((ptA.y() - ptB.y()) / (ptA.x() - ptB.x()));
        }

        // Add median of slopes through point i
        medianSlopes.push_back(destructiveMedian(slopes));
    }

    DE_ASSERT(!medianSlopes.empty());

    // Find the median of the pairwise coefficients.
    std::sort(medianSlopes.begin(), medianSlopes.end());
    result.coefficient = linearSample(medianSlopes, 0.5f);

    // Compute the offsets corresponding to the median coefficient, for all data points.
    for (int i = 0; i < numDataPoints; i++)
        pointwiseOffsets.push_back(dataPoints[i].y() - result.coefficient * dataPoints[i].x());

    // Find the median of the offsets.
    std::sort(pointwiseOffsets.begin(), pointwiseOffsets.end());
    result.offset = linearSample(pointwiseOffsets, 0.5f);

    // calculate confidence intervals
    result.coefficientConfidenceLower = linearSample(medianSlopes, 0.5f - reportedConfidence * 0.5f);
    result.coefficientConfidenceUpper = linearSample(medianSlopes, 0.5f + reportedConfidence * 0.5f);

    result.offsetConfidenceLower = linearSample(pointwiseOffsets, 0.5f - reportedConfidence * 0.5f);
    result.offsetConfidenceUpper = linearSample(pointwiseOffsets, 0.5f + reportedConfidence * 0.5f);

    result.confidence = reportedConfidence;

    return result;
}

bool MeasureState::isDone(void) const
{
    return (int)frameTimes.size() >= maxNumFrames ||
           (frameTimes.size() >= 2 && frameTimes[frameTimes.size() - 2] >= (uint64_t)frameShortcutTime &&
            frameTimes[frameTimes.size() - 1] >= (uint64_t)frameShortcutTime);
}

uint64_t MeasureState::getTotalTime(void) const
{
    uint64_t time = 0;
    for (int i = 0; i < (int)frameTimes.size(); i++)
        time += frameTimes[i];
    return time;
}

void MeasureState::clear(void)
{
    maxNumFrames      = 0;
    frameShortcutTime = std::numeric_limits<float>::infinity();
    numDrawCalls      = 0;
    frameTimes.clear();
}

void MeasureState::start(int maxNumFrames_, float frameShortcutTime_, int numDrawCalls_)
{
    frameTimes.clear();
    frameTimes.reserve(maxNumFrames_);
    maxNumFrames      = maxNumFrames_;
    frameShortcutTime = frameShortcutTime_;
    numDrawCalls      = numDrawCalls_;
}

TheilSenCalibrator::TheilSenCalibrator(void)
    : m_params(1 /* initial calls */, 10 /* calibrate iter frames */, 2000.0f /* calibrate iter shortcut threshold */,
               31 /* max calibration iterations */, 1000.0f / 30.0f /* target frame time */,
               1000.0f / 60.0f /* frame time cap */, 1000.0f /* target measure duration */)
    , m_state(INTERNALSTATE_LAST)
{
    clear();
}

TheilSenCalibrator::TheilSenCalibrator(const CalibratorParameters &params)
    : m_params(params)
    , m_state(INTERNALSTATE_LAST)
{
    clear();
}

TheilSenCalibrator::~TheilSenCalibrator()
{
}

void TheilSenCalibrator::clear(void)
{
    m_measureState.clear();
    m_calibrateIterations.clear();
    m_state = INTERNALSTATE_CALIBRATING;
}

void TheilSenCalibrator::clear(const CalibratorParameters &params)
{
    m_params = params;
    clear();
}

TheilSenCalibrator::State TheilSenCalibrator::getState(void) const
{
    if (m_state == INTERNALSTATE_FINISHED)
        return STATE_FINISHED;
    else
    {
        DE_ASSERT(m_state == INTERNALSTATE_CALIBRATING || !m_measureState.isDone());
        return m_measureState.isDone() ? STATE_RECOMPUTE_PARAMS : STATE_MEASURE;
    }
}

void TheilSenCalibrator::recordIteration(uint64_t iterationTime)
{
    DE_ASSERT((m_state == INTERNALSTATE_CALIBRATING || m_state == INTERNALSTATE_RUNNING) && !m_measureState.isDone());
    m_measureState.frameTimes.push_back(iterationTime);

    if (m_state == INTERNALSTATE_RUNNING && m_measureState.isDone())
        m_state = INTERNALSTATE_FINISHED;
}

void TheilSenCalibrator::recomputeParameters(void)
{
    DE_ASSERT(m_state == INTERNALSTATE_CALIBRATING);
    DE_ASSERT(m_measureState.isDone());

    // Minimum and maximum acceptable frame times.
    const float minGoodFrameTimeUs = m_params.targetFrameTimeUs * 0.95f;
    const float maxGoodFrameTimeUs = m_params.targetFrameTimeUs * 1.15f;

    const int numIterations = (int)m_calibrateIterations.size();

    // Record frame time.
    if (numIterations > 0)
    {
        m_calibrateIterations.back().frameTime =
            (float)((double)m_measureState.getTotalTime() / (double)m_measureState.frameTimes.size());

        // Check if we're good enough to stop calibrating.
        {
            bool endCalibration = false;

            // Is the maximum calibration iteration limit reached?
            endCalibration = endCalibration || (int)m_calibrateIterations.size() >= m_params.maxCalibrateIterations;

            // Do a few past iterations have frame time in acceptable range?
            {
                const int numRelevantPastIterations = 2;

                if (!endCalibration && (int)m_calibrateIterations.size() >= numRelevantPastIterations)
                {
                    const CalibrateIteration *const past =
                        &m_calibrateIterations[m_calibrateIterations.size() - numRelevantPastIterations];
                    bool allInGoodRange = true;

                    for (int i = 0; i < numRelevantPastIterations && allInGoodRange; i++)
                    {
                        const float frameTimeUs = past[i].frameTime;
                        if (!de::inRange(frameTimeUs, minGoodFrameTimeUs, maxGoodFrameTimeUs))
                            allInGoodRange = false;
                    }

                    endCalibration = endCalibration || allInGoodRange;
                }
            }

            // Do a few past iterations have similar-enough call counts?
            {
                const int numRelevantPastIterations = 3;
                if (!endCalibration && (int)m_calibrateIterations.size() >= numRelevantPastIterations)
                {
                    const CalibrateIteration *const past =
                        &m_calibrateIterations[m_calibrateIterations.size() - numRelevantPastIterations];
                    int minCallCount = std::numeric_limits<int>::max();
                    int maxCallCount = std::numeric_limits<int>::min();

                    for (int i = 0; i < numRelevantPastIterations; i++)
                    {
                        minCallCount = de::min(minCallCount, past[i].numDrawCalls);
                        maxCallCount = de::max(maxCallCount, past[i].numDrawCalls);
                    }

                    if ((float)(maxCallCount - minCallCount) <= (float)minCallCount * 0.1f)
                        endCalibration = true;
                }
            }

            // Is call count just 1, and frame time still way too high?
            endCalibration =
                endCalibration || (m_calibrateIterations.back().numDrawCalls == 1 &&
                                   m_calibrateIterations.back().frameTime > m_params.targetFrameTimeUs * 2.0f);

            if (endCalibration)
            {
                const int minFrames  = 10;
                const int maxFrames  = 60;
                int numMeasureFrames = deClamp32(
                    deRoundFloatToInt32(m_params.targetMeasureDurationUs / m_calibrateIterations.back().frameTime),
                    minFrames, maxFrames);

                m_state = INTERNALSTATE_RUNNING;
                m_measureState.start(numMeasureFrames, m_params.calibrateIterationShortcutThreshold,
                                     m_calibrateIterations.back().numDrawCalls);
                return;
            }
        }
    }

    DE_ASSERT(m_state == INTERNALSTATE_CALIBRATING);

    // Estimate new call count.
    {
        int newCallCount;

        if (numIterations == 0)
            newCallCount = m_params.numInitialCalls;
        else
        {
            vector<Vec2> dataPoints;
            for (int i = 0; i < numIterations; i++)
            {
                if (m_calibrateIterations[i].numDrawCalls == 1 ||
                    m_calibrateIterations[i].frameTime >
                        m_params.frameTimeCapUs * 1.05f) // Only account for measurements not too near the cap.
                    dataPoints.push_back(
                        Vec2((float)m_calibrateIterations[i].numDrawCalls, m_calibrateIterations[i].frameTime));
            }

            if (numIterations == 1)
                dataPoints.push_back(
                    Vec2(0.0f,
                         0.0f)); // If there's just one measurement so far, this will help in getting the next estimate.

            {
                const float targetFrameTimeUs = m_params.targetFrameTimeUs;
                const float coeffEpsilon =
                    0.001f; // Coefficient must be large enough (and positive) to be considered sensible.

                const LineParameters estimatorLine = theilSenLinearRegression(dataPoints);

                int prevMaxCalls = 0;

                // Find the maximum of the past call counts.
                for (int i = 0; i < numIterations; i++)
                    prevMaxCalls = de::max(prevMaxCalls, m_calibrateIterations[i].numDrawCalls);

                if (estimatorLine.coefficient <
                    coeffEpsilon) // Coefficient not good for sensible estimation; increase call count enough to get a reasonably different value.
                    newCallCount = 2 * prevMaxCalls;
                else
                {
                    // Solve newCallCount such that approximately targetFrameTime = offset + coefficient*newCallCount.
                    newCallCount = (int)((targetFrameTimeUs - estimatorLine.offset) / estimatorLine.coefficient + 0.5f);

                    // We should generally prefer FPS counts below the target rather than above (i.e. higher frame times rather than lower).
                    if (estimatorLine.offset + estimatorLine.coefficient * (float)newCallCount < minGoodFrameTimeUs)
                        newCallCount++;
                }

                // Make sure we have at least minimum amount of calls, and don't allow increasing call count too much in one iteration.
                newCallCount = de::clamp(newCallCount, 1, prevMaxCalls * 10);
            }
        }

        m_measureState.start(m_params.maxCalibrateIterationFrames, m_params.calibrateIterationShortcutThreshold,
                             newCallCount);
        m_calibrateIterations.push_back(CalibrateIteration(newCallCount, 0.0f));
    }
}

void logCalibrationInfo(tcu::TestLog &log, const TheilSenCalibrator &calibrator)
{
    const CalibratorParameters &params                         = calibrator.getParameters();
    const std::vector<CalibrateIteration> &calibrateIterations = calibrator.getCalibrationInfo();

    // Write out default calibration info.

    log << TestLog::Section("CalibrationInfo", "Calibration Info") << TestLog::Message
        << "Target frame time: " << params.targetFrameTimeUs << " us (" << 1000000 / params.targetFrameTimeUs << " fps)"
        << TestLog::EndMessage;

    for (int iterNdx = 0; iterNdx < (int)calibrateIterations.size(); iterNdx++)
    {
        log << TestLog::Message << "  iteration " << iterNdx << ": " << calibrateIterations[iterNdx].numDrawCalls
            << " calls => " << de::floatToString(calibrateIterations[iterNdx].frameTime, 2) << " us ("
            << de::floatToString(1000000.0f / calibrateIterations[iterNdx].frameTime, 2) << " fps)"
            << TestLog::EndMessage;
    }
    log << TestLog::Integer("CallCount", "Calibrated call count", "", QP_KEY_TAG_NONE,
                            calibrator.getMeasureState().numDrawCalls)
        << TestLog::Integer("FrameCount", "Calibrated frame count", "", QP_KEY_TAG_NONE,
                            (int)calibrator.getMeasureState().frameTimes.size());
    log << TestLog::EndSection;
}

} // namespace gls
} // namespace deqp
