/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief CPU warm-up utility, used to counteract CPU throttling.
 *//*--------------------------------------------------------------------*/

#include "tcuCPUWarmup.hpp"
#include "deDefs.hpp"
#include "deMath.h"
#include "deClock.h"

#include <algorithm>

namespace tcu
{

namespace warmupCPUInternal
{

volatile Unused g_unused;

}

template <typename T, int Size>
static inline float floatMedian(const T (&v)[Size])
{
    T temp[Size];
    for (int i = 0; i < Size; i++)
        temp[i] = v[i];

    std::sort(DE_ARRAY_BEGIN(temp), DE_ARRAY_END(temp));

    return Size % 2 == 0 ? 0.5f * ((float)temp[Size / 2 - 1] + (float)temp[Size / 2]) : (float)temp[Size / 2];
}

template <typename T, int Size>
static inline float floatRelativeMedianAbsoluteDeviation(const T (&v)[Size])
{
    const float median = floatMedian(v);
    float absoluteDeviations[Size];

    for (int i = 0; i < Size; i++)
        absoluteDeviations[i] = deFloatAbs((float)v[i] - median);

    return floatMedian(absoluteDeviations) / median;
}

static inline float unusedComputation(float initial, int numIterations)
{
    float a = initial;
    int b   = 123;

    for (int i = 0; i < numIterations; i++)
    {
        // Arbitrary computations.
        for (int j = 0; j < 4; j++)
        {
            a = deFloatCos(a + (float)b);
            b = (b + 63) % 107 + de::abs((int)(a * 10.0f));
        }
    }

    return a + (float)b;
}

void warmupCPU(void)
{
    float unused        = *warmupCPUInternal::g_unused.m_v;
    int computationSize = 1;

    // Do a rough calibration for computationSize to get unusedComputation's running time above a certain threshold.
    while (computationSize <
           1 << 30) // \note This condition is unlikely to be met. The "real" loop exit is the break below.
    {
        const float singleMeasurementThreshold = 10000.0f;
        const int numMeasurements              = 3;
        int64_t times[numMeasurements];

        for (int i = 0; i < numMeasurements; i++)
        {
            const uint64_t startTime = deGetMicroseconds();
            unused                   = unusedComputation(unused, computationSize);
            times[i]                 = (int64_t)(deGetMicroseconds() - startTime);
        }

        if (floatMedian(times) >= singleMeasurementThreshold)
            break;

        computationSize *= 2;
    }

    // Do unusedComputations until running time seems stable enough.
    {
        const int maxNumMeasurements                         = 50;
        const int numConsecutiveMeasurementsRequired         = 5;
        const float relativeMedianAbsoluteDeviationThreshold = 0.05f;
        int64_t latestTimes[numConsecutiveMeasurementsRequired];

        for (int measurementNdx = 0;

             measurementNdx < maxNumMeasurements &&
             (measurementNdx < numConsecutiveMeasurementsRequired ||
              floatRelativeMedianAbsoluteDeviation(latestTimes) > relativeMedianAbsoluteDeviationThreshold);

             measurementNdx++)
        {
            const uint64_t startTime = deGetMicroseconds();
            unused                   = unusedComputation(unused, computationSize);
            latestTimes[measurementNdx % numConsecutiveMeasurementsRequired] =
                (int64_t)(deGetMicroseconds() - startTime);
        }
    }

    *warmupCPUInternal::g_unused.m_v = unused;
}

} // namespace tcu
