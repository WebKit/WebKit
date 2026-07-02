/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
 * -------------------------------------------------
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
 * \brief Buffer data upload performance tests.
 *//*--------------------------------------------------------------------*/

#include "es3pBufferDataUploadTests.hpp"
#include "glsCalibration.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuSurface.hpp"
#include "tcuCPUWarmup.hpp"
#include "tcuRenderTarget.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "gluStrUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "gluObjectWrapper.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "deClock.h"
#include "deMath.h"
#include "deStringUtil.hpp"
#include "deRandom.hpp"
#include "deMemory.h"
#include "deThread.h"
#include "deMeta.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>

namespace deqp
{
namespace gles3
{
namespace Performance
{
namespace
{

using de::meta::EnableIf;
using de::meta::Not;
using gls::LineParametersWithConfidence;
using gls::theilSenSiegelLinearRegression;

static const char *const s_minimalVertexShader = "#version 300 es\n"
                                                 "in highp vec4 a_position;\n"
                                                 "void main (void)\n"
                                                 "{\n"
                                                 "    gl_Position = a_position;\n"
                                                 "}\n";

static const char *const s_minimalFragnentShader = "#version 300 es\n"
                                                   "layout(location = 0) out mediump vec4 dEQP_FragColor;\n"
                                                   "void main (void)\n"
                                                   "{\n"
                                                   "    dEQP_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
                                                   "}\n";

static const char *const s_colorVertexShader = "#version 300 es\n"
                                               "in highp vec4 a_position;\n"
                                               "in highp vec4 a_color;\n"
                                               "out highp vec4 v_color;\n"
                                               "void main (void)\n"
                                               "{\n"
                                               "    gl_Position = a_position;\n"
                                               "    v_color = a_color;\n"
                                               "}\n";

static const char *const s_colorFragmentShader = "#version 300 es\n"
                                                 "layout(location = 0) out mediump vec4 dEQP_FragColor;\n"
                                                 "in mediump vec4 v_color;\n"
                                                 "void main (void)\n"
                                                 "{\n"
                                                 "    dEQP_FragColor = v_color;\n"
                                                 "}\n";

struct SingleOperationDuration
{
    uint64_t totalDuration;
    uint64_t fitResponseDuration; // used for fitting
};

struct MapBufferRangeDuration
{
    uint64_t mapDuration;
    uint64_t unmapDuration;
    uint64_t writeDuration;
    uint64_t allocDuration;
    uint64_t totalDuration;

    uint64_t fitResponseDuration;
};

struct MapBufferRangeDurationNoAlloc
{
    uint64_t mapDuration;
    uint64_t unmapDuration;
    uint64_t writeDuration;
    uint64_t totalDuration;

    uint64_t fitResponseDuration;
};

struct MapBufferRangeFlushDuration
{
    uint64_t mapDuration;
    uint64_t unmapDuration;
    uint64_t writeDuration;
    uint64_t flushDuration;
    uint64_t allocDuration;
    uint64_t totalDuration;

    uint64_t fitResponseDuration;
};

struct MapBufferRangeFlushDurationNoAlloc
{
    uint64_t mapDuration;
    uint64_t unmapDuration;
    uint64_t writeDuration;
    uint64_t flushDuration;
    uint64_t totalDuration;

    uint64_t fitResponseDuration;
};

struct RenderReadDuration
{
    uint64_t renderDuration;
    uint64_t readDuration;
    uint64_t renderReadDuration;
    uint64_t totalDuration;

    uint64_t fitResponseDuration;
};

struct UnrelatedUploadRenderReadDuration
{
    uint64_t renderDuration;
    uint64_t readDuration;
    uint64_t renderReadDuration;
    uint64_t totalDuration;

    uint64_t fitResponseDuration;
};

struct UploadRenderReadDuration
{
    uint64_t uploadDuration;
    uint64_t renderDuration;
    uint64_t readDuration;
    uint64_t totalDuration;
    uint64_t renderReadDuration;

    uint64_t fitResponseDuration;
};

struct UploadRenderReadDurationWithUnrelatedUploadSize
{
    uint64_t uploadDuration;
    uint64_t renderDuration;
    uint64_t readDuration;
    uint64_t totalDuration;
    uint64_t renderReadDuration;

    uint64_t fitResponseDuration;
};

struct RenderUploadRenderReadDuration
{
    uint64_t firstRenderDuration;
    uint64_t uploadDuration;
    uint64_t secondRenderDuration;
    uint64_t readDuration;
    uint64_t totalDuration;
    uint64_t renderReadDuration;

    uint64_t fitResponseDuration;
};

template <typename SampleT>
struct UploadSampleResult
{
    typedef SampleT SampleType;

    int bufferSize;
    int allocatedSize;
    int writtenSize;
    SampleType duration;
};

template <typename SampleT>
struct RenderSampleResult
{
    typedef SampleT SampleType;

    int uploadedDataSize;
    int renderDataSize;
    int unrelatedDataSize;
    int numVertices;
    SampleT duration;
};

struct SingleOperationStatistics
{
    float minTime;
    float maxTime;
    float medianTime;
    float min2DecileTime; // !< minimum value in the 2nd decile
    float max9DecileTime; // !< maximum value in the 9th decile
};

struct SingleCallStatistics
{
    SingleOperationStatistics result;

    float medianRate;
    float maxDiffTime;
    float maxDiff9DecileTime;
    float medianDiffTime;

    float maxRelDiffTime;
    float max9DecileRelDiffTime;
    float medianRelDiffTime;
};

struct MapCallStatistics
{
    SingleOperationStatistics map;
    SingleOperationStatistics unmap;
    SingleOperationStatistics write;
    SingleOperationStatistics alloc;
    SingleOperationStatistics result;

    float medianRate;
    float maxDiffTime;
    float maxDiff9DecileTime;
    float medianDiffTime;

    float maxRelDiffTime;
    float max9DecileRelDiffTime;
    float medianRelDiffTime;
};

struct MapFlushCallStatistics
{
    SingleOperationStatistics map;
    SingleOperationStatistics unmap;
    SingleOperationStatistics write;
    SingleOperationStatistics flush;
    SingleOperationStatistics alloc;
    SingleOperationStatistics result;

    float medianRate;
    float maxDiffTime;
    float maxDiff9DecileTime;
    float medianDiffTime;

    float maxRelDiffTime;
    float max9DecileRelDiffTime;
    float medianRelDiffTime;
};

struct RenderReadStatistics
{
    SingleOperationStatistics render;
    SingleOperationStatistics read;
    SingleOperationStatistics result;
    SingleOperationStatistics total;

    float medianRate;
    float maxDiffTime;
    float maxDiff9DecileTime;
    float medianDiffTime;

    float maxRelDiffTime;
    float max9DecileRelDiffTime;
    float medianRelDiffTime;
};

struct UploadRenderReadStatistics
{
    SingleOperationStatistics upload;
    SingleOperationStatistics render;
    SingleOperationStatistics read;
    SingleOperationStatistics result;
    SingleOperationStatistics total;

    float medianRate;
    float maxDiffTime;
    float maxDiff9DecileTime;
    float medianDiffTime;

    float maxRelDiffTime;
    float max9DecileRelDiffTime;
    float medianRelDiffTime;
};

struct RenderUploadRenderReadStatistics
{
    SingleOperationStatistics firstRender;
    SingleOperationStatistics upload;
    SingleOperationStatistics secondRender;
    SingleOperationStatistics read;
    SingleOperationStatistics result;
    SingleOperationStatistics total;

    float medianRate;
    float maxDiffTime;
    float maxDiff9DecileTime;
    float medianDiffTime;

    float maxRelDiffTime;
    float max9DecileRelDiffTime;
    float medianRelDiffTime;
};

template <typename T>
struct SampleTypeTraits
{
};

template <>
struct SampleTypeTraits<SingleOperationDuration>
{
    typedef SingleCallStatistics StatsType;

    enum
    {
        HAS_MAP_STATS = 0
    };
    enum
    {
        HAS_UNMAP_STATS = 0
    };
    enum
    {
        HAS_WRITE_STATS = 0
    };
    enum
    {
        HAS_FLUSH_STATS = 0
    };
    enum
    {
        HAS_ALLOC_STATS = 0
    };
    enum
    {
        LOG_CONTRIBUTIONS = 0
    };
};

template <>
struct SampleTypeTraits<MapBufferRangeDuration>
{
    typedef MapCallStatistics StatsType;

    enum
    {
        HAS_MAP_STATS = 1
    };
    enum
    {
        HAS_UNMAP_STATS = 1
    };
    enum
    {
        HAS_WRITE_STATS = 1
    };
    enum
    {
        HAS_FLUSH_STATS = 0
    };
    enum
    {
        HAS_ALLOC_STATS = 1
    };
    enum
    {
        LOG_CONTRIBUTIONS = 1
    };
};

template <>
struct SampleTypeTraits<MapBufferRangeDurationNoAlloc>
{
    typedef MapCallStatistics StatsType;

    enum
    {
        HAS_MAP_STATS = 1
    };
    enum
    {
        HAS_UNMAP_STATS = 1
    };
    enum
    {
        HAS_WRITE_STATS = 1
    };
    enum
    {
        HAS_FLUSH_STATS = 0
    };
    enum
    {
        HAS_ALLOC_STATS = 0
    };
    enum
    {
        LOG_CONTRIBUTIONS = 1
    };
};

template <>
struct SampleTypeTraits<MapBufferRangeFlushDuration>
{
    typedef MapFlushCallStatistics StatsType;

    enum
    {
        HAS_MAP_STATS = 1
    };
    enum
    {
        HAS_UNMAP_STATS = 1
    };
    enum
    {
        HAS_WRITE_STATS = 1
    };
    enum
    {
        HAS_FLUSH_STATS = 1
    };
    enum
    {
        HAS_ALLOC_STATS = 1
    };
    enum
    {
        LOG_CONTRIBUTIONS = 1
    };
};

template <>
struct SampleTypeTraits<MapBufferRangeFlushDurationNoAlloc>
{
    typedef MapFlushCallStatistics StatsType;

    enum
    {
        HAS_MAP_STATS = 1
    };
    enum
    {
        HAS_UNMAP_STATS = 1
    };
    enum
    {
        HAS_WRITE_STATS = 1
    };
    enum
    {
        HAS_FLUSH_STATS = 1
    };
    enum
    {
        HAS_ALLOC_STATS = 0
    };
    enum
    {
        LOG_CONTRIBUTIONS = 1
    };
};

template <>
struct SampleTypeTraits<RenderReadDuration>
{
    typedef RenderReadStatistics StatsType;

    enum
    {
        HAS_RENDER_STATS = 1
    };
    enum
    {
        HAS_READ_STATS = 1
    };
    enum
    {
        HAS_UPLOAD_STATS = 0
    };
    enum
    {
        HAS_TOTAL_STATS = 1
    };
    enum
    {
        HAS_FIRST_RENDER_STATS = 0
    };
    enum
    {
        HAS_SECOND_RENDER_STATS = 0
    };

    enum
    {
        LOG_CONTRIBUTIONS = 1
    };
};

template <>
struct SampleTypeTraits<UnrelatedUploadRenderReadDuration>
{
    typedef RenderReadStatistics StatsType;

    enum
    {
        HAS_RENDER_STATS = 1
    };
    enum
    {
        HAS_READ_STATS = 1
    };
    enum
    {
        HAS_UPLOAD_STATS = 0
    };
    enum
    {
        HAS_TOTAL_STATS = 1
    };
    enum
    {
        HAS_FIRST_RENDER_STATS = 0
    };
    enum
    {
        HAS_SECOND_RENDER_STATS = 0
    };

    enum
    {
        LOG_CONTRIBUTIONS = 1
    };
};

template <>
struct SampleTypeTraits<UploadRenderReadDuration>
{
    typedef UploadRenderReadStatistics StatsType;

    enum
    {
        HAS_RENDER_STATS = 1
    };
    enum
    {
        HAS_READ_STATS = 1
    };
    enum
    {
        HAS_UPLOAD_STATS = 1
    };
    enum
    {
        HAS_TOTAL_STATS = 1
    };
    enum
    {
        HAS_FIRST_RENDER_STATS = 0
    };
    enum
    {
        HAS_SECOND_RENDER_STATS = 0
    };

    enum
    {
        LOG_CONTRIBUTIONS = 1
    };
    enum
    {
        LOG_UNRELATED_UPLOAD_SIZE = 0
    };
};

template <>
struct SampleTypeTraits<UploadRenderReadDurationWithUnrelatedUploadSize>
{
    typedef UploadRenderReadStatistics StatsType;

    enum
    {
        HAS_RENDER_STATS = 1
    };
    enum
    {
        HAS_READ_STATS = 1
    };
    enum
    {
        HAS_UPLOAD_STATS = 1
    };
    enum
    {
        HAS_TOTAL_STATS = 1
    };
    enum
    {
        HAS_FIRST_RENDER_STATS = 0
    };
    enum
    {
        HAS_SECOND_RENDER_STATS = 0
    };

    enum
    {
        LOG_CONTRIBUTIONS = 1
    };
    enum
    {
        LOG_UNRELATED_UPLOAD_SIZE = 1
    };
};

template <>
struct SampleTypeTraits<RenderUploadRenderReadDuration>
{
    typedef RenderUploadRenderReadStatistics StatsType;

    enum
    {
        HAS_RENDER_STATS = 0
    };
    enum
    {
        HAS_READ_STATS = 1
    };
    enum
    {
        HAS_UPLOAD_STATS = 1
    };
    enum
    {
        HAS_TOTAL_STATS = 1
    };
    enum
    {
        HAS_FIRST_RENDER_STATS = 1
    };
    enum
    {
        HAS_SECOND_RENDER_STATS = 1
    };

    enum
    {
        LOG_CONTRIBUTIONS = 1
    };
    enum
    {
        LOG_UNRELATED_UPLOAD_SIZE = 1
    };
};

struct UploadSampleAnalyzeResult
{
    float transferRateMedian;
    float transferRateAtRange;
    float transferRateAtInfinity;
};

struct RenderSampleAnalyzeResult
{
    float renderRateMedian;
    float renderRateAtRange;
    float renderRateAtInfinity;
};

class UnmapFailureError : public std::exception
{
public:
    UnmapFailureError(void) : std::exception()
    {
    }
};

static std::string getHumanReadableByteSize(int numBytes)
{
    std::ostringstream buf;

    if (numBytes < 1024)
        buf << numBytes << " byte(s)";
    else if (numBytes < 1024 * 1024)
        buf << de::floatToString((float)numBytes / 1024.0f, 1) << " KiB";
    else
        buf << de::floatToString((float)numBytes / 1024.0f / 1024.0f, 1) << " MiB";

    return buf.str();
}

static uint64_t medianTimeMemcpy(void *dst, const void *src, int numBytes)
{
    // Time used by memcpy is assumed to be asymptotically linear

    // With large numBytes, the probability of context switch or other random
    // event is high. Apply memcpy in parts and report how much time would
    // memcpy have used with the median transfer rate.

    // Less than 1MiB, no need to do anything special
    if (numBytes < 1048576)
    {
        uint64_t startTime;
        uint64_t endTime;

        deYield();

        startTime = deGetMicroseconds();
        deMemcpy(dst, src, numBytes);
        endTime = deGetMicroseconds();

        return endTime - startTime;
    }
    else
    {
        // Do memcpy in multiple parts

        const int numSections  = 5;
        const int sectionAlign = 16;

        int sectionStarts[numSections + 1];
        int sectionLens[numSections];
        uint64_t sectionTimes[numSections];
        uint64_t medianTime;
        uint64_t bestTime = 0;

        for (int sectionNdx = 0; sectionNdx < numSections; ++sectionNdx)
            sectionStarts[sectionNdx] = deAlign32((numBytes * sectionNdx / numSections), sectionAlign);
        sectionStarts[numSections] = numBytes;

        for (int sectionNdx = 0; sectionNdx < numSections; ++sectionNdx)
            sectionLens[sectionNdx] = sectionStarts[sectionNdx + 1] - sectionStarts[sectionNdx];

        // Memcpy is usually called after mapbuffer range which may take
        // a lot of time. To prevent power management from kicking in during
        // copy, warm up more.
        {
            deYield();
            tcu::warmupCPU();
            deYield();
        }

        for (int sectionNdx = 0; sectionNdx < numSections; ++sectionNdx)
        {
            uint64_t startTime;
            uint64_t endTime;

            startTime = deGetMicroseconds();
            deMemcpy((uint8_t *)dst + sectionStarts[sectionNdx], (const uint8_t *)src + sectionStarts[sectionNdx],
                     sectionLens[sectionNdx]);
            endTime = deGetMicroseconds();

            sectionTimes[sectionNdx] = endTime - startTime;

            if (!bestTime || sectionTimes[sectionNdx] < bestTime)
                bestTime = sectionTimes[sectionNdx];

            // Detect if write takes 50% longer than it should, and warm up if that happened
            if (sectionNdx != numSections - 1 && (float)sectionTimes[sectionNdx] > 1.5f * (float)bestTime)
            {
                deYield();
                tcu::warmupCPU();
                deYield();
            }
        }

        std::sort(sectionTimes, sectionTimes + numSections);

        if ((numSections % 2) == 0)
            medianTime = (sectionTimes[numSections / 2 - 1] + sectionTimes[numSections / 2]) / 2;
        else
            medianTime = sectionTimes[numSections / 2];

        return medianTime * numSections;
    }
}

static float busyworkCalculation(float initial, int workSize)
{
    float a = initial;
    int b   = 123;

    for (int ndx = 0; ndx < workSize; ++ndx)
    {
        a = deFloatCos(a + (float)b);
        b = (b + 63) % 107 + de::abs((int)(a * 10.0f));
    }

    return a + (float)b;
}

static void busyWait(int microseconds)
{
    const uint64_t maxSingleWaitTime = 1000; // 1ms
    const uint64_t endTime           = deGetMicroseconds() + microseconds;
    float unused                     = *tcu::warmupCPUInternal::g_unused.m_v;
    int workSize                     = 500;

    // exponentially increase work, cap to 1ms
    while (deGetMicroseconds() < endTime)
    {
        const uint64_t startTime = deGetMicroseconds();
        uint64_t totalTime;

        unused = busyworkCalculation(unused, workSize);

        totalTime = deGetMicroseconds() - startTime;

        if (totalTime >= maxSingleWaitTime)
            break;
        else
            workSize *= 2;
    }

    // "wait"
    while (deGetMicroseconds() < endTime)
        unused = busyworkCalculation(unused, workSize);

    *tcu::warmupCPUInternal::g_unused.m_v = unused;
}

// Sample from given values using linear interpolation at a given position as if values were laid to range [0, 1]
template <typename T>
static float linearSample(const std::vector<T> &values, float position)
{
    DE_ASSERT(position >= 0.0f);
    DE_ASSERT(position <= 1.0f);

    const float floatNdx            = (float)(values.size() - 1) * position;
    const int lowerNdx              = (int)deFloatFloor(floatNdx);
    const int higherNdx             = lowerNdx + 1;
    const float interpolationFactor = floatNdx - (float)lowerNdx;

    DE_ASSERT(lowerNdx >= 0 && lowerNdx < (int)values.size());
    DE_ASSERT(higherNdx >= 0 && higherNdx < (int)values.size());
    DE_ASSERT(interpolationFactor >= 0 && interpolationFactor < 1.0f);

    return tcu::mix((float)values[lowerNdx], (float)values[higherNdx], interpolationFactor);
}

template <typename T>
SingleOperationStatistics calculateSingleOperationStatistics(const std::vector<T> &samples,
                                                             uint64_t T::SampleType::*target)
{
    SingleOperationStatistics stats;
    std::vector<uint64_t> values(samples.size());

    for (int ndx = 0; ndx < (int)samples.size(); ++ndx)
        values[ndx] = samples[ndx].duration.*target;

    std::sort(values.begin(), values.end());

    stats.minTime        = (float)values.front();
    stats.maxTime        = (float)values.back();
    stats.medianTime     = linearSample(values, 0.5f);
    stats.min2DecileTime = linearSample(values, 0.1f);
    stats.max9DecileTime = linearSample(values, 0.9f);

    return stats;
}

template <typename StatisticsType, typename SampleType>
void calculateBasicStatistics(StatisticsType &stats, const LineParametersWithConfidence &fit,
                              const std::vector<SampleType> &samples, int SampleType::*predictor)
{
    std::vector<uint64_t> values(samples.size());

    for (int ndx = 0; ndx < (int)samples.size(); ++ndx)
        values[ndx] = samples[ndx].duration.fitResponseDuration;

    // median rate
    {
        std::vector<float> processingRates(samples.size());

        for (int ndx = 0; ndx < (int)samples.size(); ++ndx)
        {
            const float timeInSeconds = (float)values[ndx] / 1000.0f / 1000.0f;
            processingRates[ndx]      = (float)(samples[ndx].*predictor) / timeInSeconds;
        }

        std::sort(processingRates.begin(), processingRates.end());

        stats.medianRate = linearSample(processingRates, 0.5f);
    }

    // results compared to the approximation
    {
        std::vector<float> timeDiffs(samples.size());

        for (int ndx = 0; ndx < (int)samples.size(); ++ndx)
        {
            const float prediction = (float)(samples[ndx].*predictor) * fit.coefficient + fit.offset;
            const float actual     = (float)values[ndx];
            timeDiffs[ndx]         = actual - prediction;
        }
        std::sort(timeDiffs.begin(), timeDiffs.end());

        stats.maxDiffTime        = timeDiffs.back();
        stats.maxDiff9DecileTime = linearSample(timeDiffs, 0.9f);
        stats.medianDiffTime     = linearSample(timeDiffs, 0.5f);
    }

    // relative comparison to the approximation
    {
        std::vector<float> relativeDiffs(samples.size());

        for (int ndx = 0; ndx < (int)samples.size(); ++ndx)
        {
            const float prediction = (float)(samples[ndx].*predictor) * fit.coefficient + fit.offset;
            const float actual     = (float)values[ndx];

            // Ignore cases where we predict negative times, or if
            // ratio would be (nearly) infinite: ignore if predicted
            // time is less than 1 microsecond
            if (prediction < 1.0f)
                relativeDiffs[ndx] = 0.0f;
            else
                relativeDiffs[ndx] = (actual - prediction) / prediction;
        }
        std::sort(relativeDiffs.begin(), relativeDiffs.end());

        stats.maxRelDiffTime        = relativeDiffs.back();
        stats.max9DecileRelDiffTime = linearSample(relativeDiffs, 0.9f);
        stats.medianRelDiffTime     = linearSample(relativeDiffs, 0.5f);
    }

    // values calculated using sorted timings

    std::sort(values.begin(), values.end());

    stats.result.minTime        = (float)values.front();
    stats.result.maxTime        = (float)values.back();
    stats.result.medianTime     = linearSample(values, 0.5f);
    stats.result.min2DecileTime = linearSample(values, 0.1f);
    stats.result.max9DecileTime = linearSample(values, 0.9f);
}

template <typename StatisticsType, typename SampleType>
void calculateBasicTransferStatistics(StatisticsType &stats, const LineParametersWithConfidence &fit,
                                      const std::vector<SampleType> &samples)
{
    calculateBasicStatistics(stats, fit, samples, &SampleType::writtenSize);
}

template <typename StatisticsType, typename SampleType>
void calculateBasicRenderStatistics(StatisticsType &stats, const LineParametersWithConfidence &fit,
                                    const std::vector<SampleType> &samples)
{
    calculateBasicStatistics(stats, fit, samples, &SampleType::renderDataSize);
}

static SingleCallStatistics calculateSampleStatistics(
    const LineParametersWithConfidence &fit, const std::vector<UploadSampleResult<SingleOperationDuration>> &samples)
{
    SingleCallStatistics stats;

    calculateBasicTransferStatistics(stats, fit, samples);

    return stats;
}

static MapCallStatistics calculateSampleStatistics(
    const LineParametersWithConfidence &fit, const std::vector<UploadSampleResult<MapBufferRangeDuration>> &samples)
{
    MapCallStatistics stats;

    calculateBasicTransferStatistics(stats, fit, samples);

    stats.map   = calculateSingleOperationStatistics(samples, &MapBufferRangeDuration::mapDuration);
    stats.unmap = calculateSingleOperationStatistics(samples, &MapBufferRangeDuration::unmapDuration);
    stats.write = calculateSingleOperationStatistics(samples, &MapBufferRangeDuration::writeDuration);
    stats.alloc = calculateSingleOperationStatistics(samples, &MapBufferRangeDuration::allocDuration);

    return stats;
}

static MapFlushCallStatistics calculateSampleStatistics(
    const LineParametersWithConfidence &fit,
    const std::vector<UploadSampleResult<MapBufferRangeFlushDuration>> &samples)
{
    MapFlushCallStatistics stats;

    calculateBasicTransferStatistics(stats, fit, samples);

    stats.map   = calculateSingleOperationStatistics(samples, &MapBufferRangeFlushDuration::mapDuration);
    stats.unmap = calculateSingleOperationStatistics(samples, &MapBufferRangeFlushDuration::unmapDuration);
    stats.write = calculateSingleOperationStatistics(samples, &MapBufferRangeFlushDuration::writeDuration);
    stats.flush = calculateSingleOperationStatistics(samples, &MapBufferRangeFlushDuration::flushDuration);
    stats.alloc = calculateSingleOperationStatistics(samples, &MapBufferRangeFlushDuration::allocDuration);

    return stats;
}

static MapCallStatistics calculateSampleStatistics(
    const LineParametersWithConfidence &fit,
    const std::vector<UploadSampleResult<MapBufferRangeDurationNoAlloc>> &samples)
{
    MapCallStatistics stats;

    calculateBasicTransferStatistics(stats, fit, samples);

    stats.map   = calculateSingleOperationStatistics(samples, &MapBufferRangeDurationNoAlloc::mapDuration);
    stats.unmap = calculateSingleOperationStatistics(samples, &MapBufferRangeDurationNoAlloc::unmapDuration);
    stats.write = calculateSingleOperationStatistics(samples, &MapBufferRangeDurationNoAlloc::writeDuration);

    return stats;
}

static MapFlushCallStatistics calculateSampleStatistics(
    const LineParametersWithConfidence &fit,
    const std::vector<UploadSampleResult<MapBufferRangeFlushDurationNoAlloc>> &samples)
{
    MapFlushCallStatistics stats;

    calculateBasicTransferStatistics(stats, fit, samples);

    stats.map   = calculateSingleOperationStatistics(samples, &MapBufferRangeFlushDurationNoAlloc::mapDuration);
    stats.unmap = calculateSingleOperationStatistics(samples, &MapBufferRangeFlushDurationNoAlloc::unmapDuration);
    stats.write = calculateSingleOperationStatistics(samples, &MapBufferRangeFlushDurationNoAlloc::writeDuration);
    stats.flush = calculateSingleOperationStatistics(samples, &MapBufferRangeFlushDurationNoAlloc::flushDuration);

    return stats;
}

static RenderReadStatistics calculateSampleStatistics(
    const LineParametersWithConfidence &fit, const std::vector<RenderSampleResult<RenderReadDuration>> &samples)
{
    RenderReadStatistics stats;

    calculateBasicRenderStatistics(stats, fit, samples);

    stats.render = calculateSingleOperationStatistics(samples, &RenderReadDuration::renderDuration);
    stats.read   = calculateSingleOperationStatistics(samples, &RenderReadDuration::readDuration);
    stats.total  = calculateSingleOperationStatistics(samples, &RenderReadDuration::totalDuration);

    return stats;
}

static RenderReadStatistics calculateSampleStatistics(
    const LineParametersWithConfidence &fit,
    const std::vector<RenderSampleResult<UnrelatedUploadRenderReadDuration>> &samples)
{
    RenderReadStatistics stats;

    calculateBasicRenderStatistics(stats, fit, samples);

    stats.render = calculateSingleOperationStatistics(samples, &UnrelatedUploadRenderReadDuration::renderDuration);
    stats.read   = calculateSingleOperationStatistics(samples, &UnrelatedUploadRenderReadDuration::readDuration);
    stats.total  = calculateSingleOperationStatistics(samples, &UnrelatedUploadRenderReadDuration::totalDuration);

    return stats;
}

static UploadRenderReadStatistics calculateSampleStatistics(
    const LineParametersWithConfidence &fit, const std::vector<RenderSampleResult<UploadRenderReadDuration>> &samples)
{
    UploadRenderReadStatistics stats;

    calculateBasicRenderStatistics(stats, fit, samples);

    stats.upload = calculateSingleOperationStatistics(samples, &UploadRenderReadDuration::uploadDuration);
    stats.render = calculateSingleOperationStatistics(samples, &UploadRenderReadDuration::renderDuration);
    stats.read   = calculateSingleOperationStatistics(samples, &UploadRenderReadDuration::readDuration);
    stats.total  = calculateSingleOperationStatistics(samples, &UploadRenderReadDuration::totalDuration);

    return stats;
}

static UploadRenderReadStatistics calculateSampleStatistics(
    const LineParametersWithConfidence &fit,
    const std::vector<RenderSampleResult<UploadRenderReadDurationWithUnrelatedUploadSize>> &samples)
{
    UploadRenderReadStatistics stats;

    calculateBasicRenderStatistics(stats, fit, samples);

    stats.upload =
        calculateSingleOperationStatistics(samples, &UploadRenderReadDurationWithUnrelatedUploadSize::uploadDuration);
    stats.render =
        calculateSingleOperationStatistics(samples, &UploadRenderReadDurationWithUnrelatedUploadSize::renderDuration);
    stats.read =
        calculateSingleOperationStatistics(samples, &UploadRenderReadDurationWithUnrelatedUploadSize::readDuration);
    stats.total =
        calculateSingleOperationStatistics(samples, &UploadRenderReadDurationWithUnrelatedUploadSize::totalDuration);

    return stats;
}

static RenderUploadRenderReadStatistics calculateSampleStatistics(
    const LineParametersWithConfidence &fit,
    const std::vector<RenderSampleResult<RenderUploadRenderReadDuration>> &samples)
{
    RenderUploadRenderReadStatistics stats;

    calculateBasicRenderStatistics(stats, fit, samples);

    stats.firstRender =
        calculateSingleOperationStatistics(samples, &RenderUploadRenderReadDuration::firstRenderDuration);
    stats.upload = calculateSingleOperationStatistics(samples, &RenderUploadRenderReadDuration::uploadDuration);
    stats.secondRender =
        calculateSingleOperationStatistics(samples, &RenderUploadRenderReadDuration::secondRenderDuration);
    stats.read  = calculateSingleOperationStatistics(samples, &RenderUploadRenderReadDuration::readDuration);
    stats.total = calculateSingleOperationStatistics(samples, &RenderUploadRenderReadDuration::totalDuration);

    return stats;
}

template <typename DurationType>
static LineParametersWithConfidence fitLineToSamples(
    const std::vector<UploadSampleResult<DurationType>> &samples, int beginNdx, int endNdx, int step,
    uint64_t DurationType::*target = &DurationType::fitResponseDuration)
{
    std::vector<tcu::Vec2> samplePoints;

    for (int sampleNdx = beginNdx; sampleNdx < endNdx; sampleNdx += step)
    {
        tcu::Vec2 point;

        point.x() = (float)(samples[sampleNdx].writtenSize);
        point.y() = (float)(samples[sampleNdx].duration.*target);

        samplePoints.push_back(point);
    }

    return theilSenSiegelLinearRegression(samplePoints, 0.6f);
}

template <typename DurationType>
static LineParametersWithConfidence fitLineToSamples(
    const std::vector<RenderSampleResult<DurationType>> &samples, int beginNdx, int endNdx, int step,
    uint64_t DurationType::*target = &DurationType::fitResponseDuration)
{
    std::vector<tcu::Vec2> samplePoints;

    for (int sampleNdx = beginNdx; sampleNdx < endNdx; sampleNdx += step)
    {
        tcu::Vec2 point;

        point.x() = (float)(samples[sampleNdx].renderDataSize);
        point.y() = (float)(samples[sampleNdx].duration.*target);

        samplePoints.push_back(point);
    }

    return theilSenSiegelLinearRegression(samplePoints, 0.6f);
}

template <typename T>
static LineParametersWithConfidence fitLineToSamples(
    const std::vector<T> &samples, int beginNdx, int endNdx,
    uint64_t T::SampleType::*target = &T::SampleType::fitResponseDuration)
{
    return fitLineToSamples(samples, beginNdx, endNdx, 1, target);
}

template <typename T>
static LineParametersWithConfidence fitLineToSamples(
    const std::vector<T> &samples, uint64_t T::SampleType::*target = &T::SampleType::fitResponseDuration)
{
    return fitLineToSamples(samples, 0, (int)samples.size(), target);
}

static float getAreaBetweenLines(float xmin, float xmax, float lineAOffset, float lineACoefficient, float lineBOffset,
                                 float lineBCoefficient)
{
    const float lineAMin     = lineAOffset + lineACoefficient * xmin;
    const float lineAMax     = lineAOffset + lineACoefficient * xmax;
    const float lineBMin     = lineBOffset + lineBCoefficient * xmin;
    const float lineBMax     = lineBOffset + lineBCoefficient * xmax;
    const bool aOverBAtBegin = (lineAMin > lineBMin);
    const bool aOverBAtEnd   = (lineAMax > lineBMax);

    if (aOverBAtBegin == aOverBAtEnd)
    {
        // lines do not intersect

        const float midpoint = (xmin + xmax) / 2.0f;
        const float width    = (xmax - xmin);

        const float lineAHeight = lineAOffset + lineACoefficient * midpoint;
        const float lineBHeight = lineBOffset + lineBCoefficient * midpoint;

        return width * de::abs(lineAHeight - lineBHeight);
    }
    else
    {

        // lines intersect

        const float approachCoeffient = de::abs(lineACoefficient - lineBCoefficient);
        const float epsilon           = 0.0001f;
        const float leftHeight        = de::abs(lineAMin - lineBMin);
        const float rightHeight       = de::abs(lineAMax - lineBMax);

        if (approachCoeffient < epsilon)
            return 0.0f;

        return (0.5f * leftHeight * (leftHeight / approachCoeffient)) +
               (0.5f * rightHeight * (rightHeight / approachCoeffient));
    }
}

template <typename T>
static float calculateSampleFitLinearity(const std::vector<T> &samples, int T::*predictor)
{
    // Compare the fitted line of first half of the samples to the fitted line of
    // the second half of the samples. Calculate a AABB that fully contains every
    // sample's x component and both fit lines in this range. Calculate the ratio
    // of the area between the lines and the AABB.

    const float epsilon = 1.e-6f;
    const int midPoint  = (int)samples.size() / 2;
    const LineParametersWithConfidence startApproximation =
        fitLineToSamples(samples, 0, midPoint, &T::SampleType::fitResponseDuration);
    const LineParametersWithConfidence endApproximation =
        fitLineToSamples(samples, midPoint, (int)samples.size(), &T::SampleType::fitResponseDuration);

    const float aabbMinX = (float)(samples.front().*predictor);
    const float aabbMinY = de::min(startApproximation.offset + startApproximation.coefficient * aabbMinX,
                                   endApproximation.offset + endApproximation.coefficient * aabbMinX);
    const float aabbMaxX = (float)(samples.back().*predictor);
    const float aabbMaxY = de::max(startApproximation.offset + startApproximation.coefficient * aabbMaxX,
                                   endApproximation.offset + endApproximation.coefficient * aabbMaxX);

    const float aabbArea = (aabbMaxX - aabbMinX) * (aabbMaxY - aabbMinY);
    const float areaBetweenLines =
        getAreaBetweenLines(aabbMinX, aabbMaxX, startApproximation.offset, startApproximation.coefficient,
                            endApproximation.offset, endApproximation.coefficient);
    const float errorAreaRatio = (aabbArea < epsilon) ? (1.0f) : (areaBetweenLines / aabbArea);

    return de::clamp(1.0f - errorAreaRatio, 0.0f, 1.0f);
}

template <typename DurationType>
static float calculateSampleFitLinearity(const std::vector<UploadSampleResult<DurationType>> &samples)
{
    return calculateSampleFitLinearity(samples, &UploadSampleResult<DurationType>::writtenSize);
}

template <typename DurationType>
static float calculateSampleFitLinearity(const std::vector<RenderSampleResult<DurationType>> &samples)
{
    return calculateSampleFitLinearity(samples, &RenderSampleResult<DurationType>::renderDataSize);
}

template <typename T>
static float calculateSampleTemporalStability(const std::vector<T> &samples, int T::*predictor)
{
    // Samples are sampled in the following order: 1) even samples (in random order) 2) odd samples (in random order)
    // Compare the fitted line of even samples to the fitted line of the odd samples. Calculate a AABB that fully
    // contains every sample's x component and both fit lines in this range. Calculate the ratio of the area between
    // the lines and the AABB.

    const float epsilon = 1.e-6f;
    const LineParametersWithConfidence evenApproximation =
        fitLineToSamples(samples, 0, (int)samples.size(), 2, &T::SampleType::fitResponseDuration);
    const LineParametersWithConfidence oddApproximation =
        fitLineToSamples(samples, 1, (int)samples.size(), 2, &T::SampleType::fitResponseDuration);

    const float aabbMinX = (float)(samples.front().*predictor);
    const float aabbMinY = de::min(evenApproximation.offset + evenApproximation.coefficient * aabbMinX,
                                   oddApproximation.offset + oddApproximation.coefficient * aabbMinX);
    const float aabbMaxX = (float)(samples.back().*predictor);
    const float aabbMaxY = de::max(evenApproximation.offset + evenApproximation.coefficient * aabbMaxX,
                                   oddApproximation.offset + oddApproximation.coefficient * aabbMaxX);

    const float aabbArea = (aabbMaxX - aabbMinX) * (aabbMaxY - aabbMinY);
    const float areaBetweenLines =
        getAreaBetweenLines(aabbMinX, aabbMaxX, evenApproximation.offset, evenApproximation.coefficient,
                            oddApproximation.offset, oddApproximation.coefficient);
    const float errorAreaRatio = (aabbArea < epsilon) ? (1.0f) : (areaBetweenLines / aabbArea);

    return de::clamp(1.0f - errorAreaRatio, 0.0f, 1.0f);
}

template <typename DurationType>
static float calculateSampleTemporalStability(const std::vector<UploadSampleResult<DurationType>> &samples)
{
    return calculateSampleTemporalStability(samples, &UploadSampleResult<DurationType>::writtenSize);
}

template <typename DurationType>
static float calculateSampleTemporalStability(const std::vector<RenderSampleResult<DurationType>> &samples)
{
    return calculateSampleTemporalStability(samples, &RenderSampleResult<DurationType>::renderDataSize);
}

template <typename DurationType>
static void bucketizeSamplesUniformly(const std::vector<UploadSampleResult<DurationType>> &samples,
                                      std::vector<UploadSampleResult<DurationType>> *buckets, int numBuckets,
                                      int &minBufferSize, int &maxBufferSize)
{
    minBufferSize = 0;
    maxBufferSize = 0;

    for (int sampleNdx = 0; sampleNdx < (int)samples.size(); ++sampleNdx)
    {
        DE_ASSERT(samples[sampleNdx].allocatedSize != 0);

        if (!minBufferSize || samples[sampleNdx].allocatedSize < minBufferSize)
            minBufferSize = samples[sampleNdx].allocatedSize;
        if (!maxBufferSize || samples[sampleNdx].allocatedSize > maxBufferSize)
            maxBufferSize = samples[sampleNdx].allocatedSize;
    }

    for (int sampleNdx = 0; sampleNdx < (int)samples.size(); ++sampleNdx)
    {
        const float bucketNdxFloat = (float)(samples[sampleNdx].allocatedSize - minBufferSize) /
                                     (float)(maxBufferSize - minBufferSize) * (float)numBuckets;
        const int bucketNdx = de::clamp((int)deFloatFloor(bucketNdxFloat), 0, numBuckets - 1);

        buckets[bucketNdx].push_back(samples[sampleNdx]);
    }
}

template <typename SampleType>
static typename EnableIf<void, SampleTypeTraits<SampleType>::HAS_MAP_STATS>::Type logMapRangeStats(
    tcu::TestLog &log, const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    log << tcu::TestLog::Float("MapRangeMin", "MapRange: Min time", "us", QP_KEY_TAG_TIME, stats.map.minTime)
        << tcu::TestLog::Float("MapRangeMax", "MapRange: Max time", "us", QP_KEY_TAG_TIME, stats.map.maxTime)
        << tcu::TestLog::Float("MapRangeMin90", "MapRange: 90%-Min time", "us", QP_KEY_TAG_TIME,
                               stats.map.min2DecileTime)
        << tcu::TestLog::Float("MapRangeMax90", "MapRange: 90%-Max time", "us", QP_KEY_TAG_TIME,
                               stats.map.max9DecileTime)
        << tcu::TestLog::Float("MapRangeMedian", "MapRange: Median time", "us", QP_KEY_TAG_TIME, stats.map.medianTime);
}

template <typename SampleType>
static typename EnableIf<void, SampleTypeTraits<SampleType>::HAS_UNMAP_STATS>::Type logUnmapStats(
    tcu::TestLog &log, const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    log << tcu::TestLog::Float("UnmapMin", "Unmap: Min time", "us", QP_KEY_TAG_TIME, stats.unmap.minTime)
        << tcu::TestLog::Float("UnmapMax", "Unmap: Max time", "us", QP_KEY_TAG_TIME, stats.unmap.maxTime)
        << tcu::TestLog::Float("UnmapMin90", "Unmap: 90%-Min time", "us", QP_KEY_TAG_TIME, stats.unmap.min2DecileTime)
        << tcu::TestLog::Float("UnmapMax90", "Unmap: 90%-Max time", "us", QP_KEY_TAG_TIME, stats.unmap.max9DecileTime)
        << tcu::TestLog::Float("UnmapMedian", "Unmap: Median time", "us", QP_KEY_TAG_TIME, stats.unmap.medianTime);
}

template <typename SampleType>
static typename EnableIf<void, SampleTypeTraits<SampleType>::HAS_WRITE_STATS>::Type logWriteStats(
    tcu::TestLog &log, const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    log << tcu::TestLog::Float("WriteMin", "Write: Min time", "us", QP_KEY_TAG_TIME, stats.write.minTime)
        << tcu::TestLog::Float("WriteMax", "Write: Max time", "us", QP_KEY_TAG_TIME, stats.write.maxTime)
        << tcu::TestLog::Float("WriteMin90", "Write: 90%-Min time", "us", QP_KEY_TAG_TIME, stats.write.min2DecileTime)
        << tcu::TestLog::Float("WriteMax90", "Write: 90%-Max time", "us", QP_KEY_TAG_TIME, stats.write.max9DecileTime)
        << tcu::TestLog::Float("WriteMedian", "Write: Median time", "us", QP_KEY_TAG_TIME, stats.write.medianTime);
}

template <typename SampleType>
static typename EnableIf<void, SampleTypeTraits<SampleType>::HAS_FLUSH_STATS>::Type logFlushStats(
    tcu::TestLog &log, const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    log << tcu::TestLog::Float("FlushMin", "Flush: Min time", "us", QP_KEY_TAG_TIME, stats.flush.minTime)
        << tcu::TestLog::Float("FlushMax", "Flush: Max time", "us", QP_KEY_TAG_TIME, stats.flush.maxTime)
        << tcu::TestLog::Float("FlushMin90", "Flush: 90%-Min time", "us", QP_KEY_TAG_TIME, stats.flush.min2DecileTime)
        << tcu::TestLog::Float("FlushMax90", "Flush: 90%-Max time", "us", QP_KEY_TAG_TIME, stats.flush.max9DecileTime)
        << tcu::TestLog::Float("FlushMedian", "Flush: Median time", "us", QP_KEY_TAG_TIME, stats.flush.medianTime);
}

template <typename SampleType>
static typename EnableIf<void, SampleTypeTraits<SampleType>::HAS_ALLOC_STATS>::Type logAllocStats(
    tcu::TestLog &log, const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    log << tcu::TestLog::Float("AllocMin", "Alloc: Min time", "us", QP_KEY_TAG_TIME, stats.alloc.minTime)
        << tcu::TestLog::Float("AllocMax", "Alloc: Max time", "us", QP_KEY_TAG_TIME, stats.alloc.maxTime)
        << tcu::TestLog::Float("AllocMin90", "Alloc: 90%-Min time", "us", QP_KEY_TAG_TIME, stats.alloc.min2DecileTime)
        << tcu::TestLog::Float("AllocMax90", "Alloc: 90%-Max time", "us", QP_KEY_TAG_TIME, stats.alloc.max9DecileTime)
        << tcu::TestLog::Float("AllocMedian", "Alloc: Median time", "us", QP_KEY_TAG_TIME, stats.alloc.medianTime);
}

template <typename SampleType>
static typename EnableIf<void, Not<SampleTypeTraits<SampleType>::HAS_MAP_STATS>::Value>::Type logMapRangeStats(
    tcu::TestLog &log, const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    DE_UNREF(log);
    DE_UNREF(stats);
}

template <typename SampleType>
static typename EnableIf<void, Not<SampleTypeTraits<SampleType>::HAS_UNMAP_STATS>::Value>::Type logUnmapStats(
    tcu::TestLog &log, const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    DE_UNREF(log);
    DE_UNREF(stats);
}

template <typename SampleType>
static typename EnableIf<void, Not<SampleTypeTraits<SampleType>::HAS_WRITE_STATS>::Value>::Type logWriteStats(
    tcu::TestLog &log, const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    DE_UNREF(log);
    DE_UNREF(stats);
}

template <typename SampleType>
static typename EnableIf<void, Not<SampleTypeTraits<SampleType>::HAS_FLUSH_STATS>::Value>::Type logFlushStats(
    tcu::TestLog &log, const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    DE_UNREF(log);
    DE_UNREF(stats);
}

template <typename SampleType>
static typename EnableIf<void, Not<SampleTypeTraits<SampleType>::HAS_ALLOC_STATS>::Value>::Type logAllocStats(
    tcu::TestLog &log, const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    DE_UNREF(log);
    DE_UNREF(stats);
}

template <typename SampleType>
static typename EnableIf<void, SampleTypeTraits<SampleType>::HAS_MAP_STATS>::Type logMapContribution(
    tcu::TestLog &log, const std::vector<UploadSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    const LineParametersWithConfidence contributionFitting = fitLineToSamples(samples, &SampleType::mapDuration);
    log << tcu::TestLog::Float("MapConstantCost", "Map: Approximated contant cost", "us", QP_KEY_TAG_TIME,
                               contributionFitting.offset)
        << tcu::TestLog::Float("MapLinearCost", "Map: Approximated linear cost", "us / MB", QP_KEY_TAG_TIME,
                               contributionFitting.coefficient * 1024.0f * 1024.0f)
        << tcu::TestLog::Float("MapMedianCost", "Map: Median cost", "us", QP_KEY_TAG_TIME, stats.map.medianTime);
}

template <typename SampleType>
static typename EnableIf<void, SampleTypeTraits<SampleType>::HAS_UNMAP_STATS>::Type logUnmapContribution(
    tcu::TestLog &log, const std::vector<UploadSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    const LineParametersWithConfidence contributionFitting = fitLineToSamples(samples, &SampleType::unmapDuration);
    log << tcu::TestLog::Float("UnmapConstantCost", "Unmap: Approximated contant cost", "us", QP_KEY_TAG_TIME,
                               contributionFitting.offset)
        << tcu::TestLog::Float("UnmapLinearCost", "Unmap: Approximated linear cost", "us / MB", QP_KEY_TAG_TIME,
                               contributionFitting.coefficient * 1024.0f * 1024.0f)
        << tcu::TestLog::Float("UnmapMedianCost", "Unmap: Median cost", "us", QP_KEY_TAG_TIME, stats.unmap.medianTime);
}

template <typename SampleType>
static typename EnableIf<void, SampleTypeTraits<SampleType>::HAS_WRITE_STATS>::Type logWriteContribution(
    tcu::TestLog &log, const std::vector<UploadSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    const LineParametersWithConfidence contributionFitting = fitLineToSamples(samples, &SampleType::writeDuration);
    log << tcu::TestLog::Float("WriteConstantCost", "Write: Approximated contant cost", "us", QP_KEY_TAG_TIME,
                               contributionFitting.offset)
        << tcu::TestLog::Float("WriteLinearCost", "Write: Approximated linear cost", "us / MB", QP_KEY_TAG_TIME,
                               contributionFitting.coefficient * 1024.0f * 1024.0f)
        << tcu::TestLog::Float("WriteMedianCost", "Write: Median cost", "us", QP_KEY_TAG_TIME, stats.write.medianTime);
}

template <typename SampleType>
static typename EnableIf<void, SampleTypeTraits<SampleType>::HAS_FLUSH_STATS>::Type logFlushContribution(
    tcu::TestLog &log, const std::vector<UploadSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    const LineParametersWithConfidence contributionFitting = fitLineToSamples(samples, &SampleType::flushDuration);
    log << tcu::TestLog::Float("FlushConstantCost", "Flush: Approximated contant cost", "us", QP_KEY_TAG_TIME,
                               contributionFitting.offset)
        << tcu::TestLog::Float("FlushLinearCost", "Flush: Approximated linear cost", "us / MB", QP_KEY_TAG_TIME,
                               contributionFitting.coefficient * 1024.0f * 1024.0f)
        << tcu::TestLog::Float("FlushMedianCost", "Flush: Median cost", "us", QP_KEY_TAG_TIME, stats.flush.medianTime);
}

template <typename SampleType>
static typename EnableIf<void, SampleTypeTraits<SampleType>::HAS_ALLOC_STATS>::Type logAllocContribution(
    tcu::TestLog &log, const std::vector<UploadSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    const LineParametersWithConfidence contributionFitting = fitLineToSamples(samples, &SampleType::allocDuration);
    log << tcu::TestLog::Float("AllocConstantCost", "Alloc: Approximated contant cost", "us", QP_KEY_TAG_TIME,
                               contributionFitting.offset)
        << tcu::TestLog::Float("AllocLinearCost", "Alloc: Approximated linear cost", "us / MB", QP_KEY_TAG_TIME,
                               contributionFitting.coefficient * 1024.0f * 1024.0f)
        << tcu::TestLog::Float("AllocMedianCost", "Alloc: Median cost", "us", QP_KEY_TAG_TIME, stats.alloc.medianTime);
}

template <typename SampleType>
static typename EnableIf<void, SampleTypeTraits<SampleType>::HAS_RENDER_STATS>::Type logRenderContribution(
    tcu::TestLog &log, const std::vector<RenderSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    const LineParametersWithConfidence contributionFitting = fitLineToSamples(samples, &SampleType::renderDuration);
    log << tcu::TestLog::Float("DrawCallConstantCost", "DrawCall: Approximated contant cost", "us", QP_KEY_TAG_TIME,
                               contributionFitting.offset)
        << tcu::TestLog::Float("DrawCallLinearCost", "DrawCall: Approximated linear cost", "us / MB", QP_KEY_TAG_TIME,
                               contributionFitting.coefficient * 1024.0f * 1024.0f)
        << tcu::TestLog::Float("DrawCallMedianCost", "DrawCall: Median cost", "us", QP_KEY_TAG_TIME,
                               stats.render.medianTime);
}

template <typename SampleType>
static typename EnableIf<void, SampleTypeTraits<SampleType>::HAS_READ_STATS>::Type logReadContribution(
    tcu::TestLog &log, const std::vector<RenderSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    const LineParametersWithConfidence contributionFitting = fitLineToSamples(samples, &SampleType::readDuration);
    log << tcu::TestLog::Float("ReadConstantCost", "Read: Approximated contant cost", "us", QP_KEY_TAG_TIME,
                               contributionFitting.offset)
        << tcu::TestLog::Float("ReadLinearCost", "Read: Approximated linear cost", "us / MB", QP_KEY_TAG_TIME,
                               contributionFitting.coefficient * 1024.0f * 1024.0f)
        << tcu::TestLog::Float("ReadMedianCost", "Read: Median cost", "us", QP_KEY_TAG_TIME, stats.read.medianTime);
}

template <typename SampleType>
static typename EnableIf<void, SampleTypeTraits<SampleType>::HAS_UPLOAD_STATS>::Type logUploadContribution(
    tcu::TestLog &log, const std::vector<RenderSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    const LineParametersWithConfidence contributionFitting = fitLineToSamples(samples, &SampleType::uploadDuration);
    log << tcu::TestLog::Float("UploadConstantCost", "Upload: Approximated contant cost", "us", QP_KEY_TAG_TIME,
                               contributionFitting.offset)
        << tcu::TestLog::Float("UploadLinearCost", "Upload: Approximated linear cost", "us / MB", QP_KEY_TAG_TIME,
                               contributionFitting.coefficient * 1024.0f * 1024.0f)
        << tcu::TestLog::Float("UploadMedianCost", "Upload: Median cost", "us", QP_KEY_TAG_TIME,
                               stats.upload.medianTime);
}

template <typename SampleType>
static typename EnableIf<void, SampleTypeTraits<SampleType>::HAS_TOTAL_STATS>::Type logTotalContribution(
    tcu::TestLog &log, const std::vector<RenderSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    const LineParametersWithConfidence contributionFitting = fitLineToSamples(samples, &SampleType::totalDuration);
    log << tcu::TestLog::Float("TotalConstantCost", "Total: Approximated contant cost", "us", QP_KEY_TAG_TIME,
                               contributionFitting.offset)
        << tcu::TestLog::Float("TotalLinearCost", "Total: Approximated linear cost", "us / MB", QP_KEY_TAG_TIME,
                               contributionFitting.coefficient * 1024.0f * 1024.0f)
        << tcu::TestLog::Float("TotalMedianCost", "Total: Median cost", "us", QP_KEY_TAG_TIME, stats.total.medianTime);
}

template <typename SampleType>
static typename EnableIf<void, SampleTypeTraits<SampleType>::HAS_FIRST_RENDER_STATS>::Type logFirstRenderContribution(
    tcu::TestLog &log, const std::vector<RenderSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    const LineParametersWithConfidence contributionFitting =
        fitLineToSamples(samples, &SampleType::firstRenderDuration);
    log << tcu::TestLog::Float("FirstDrawCallConstantCost", "First DrawCall: Approximated contant cost", "us",
                               QP_KEY_TAG_TIME, contributionFitting.offset)
        << tcu::TestLog::Float("FirstDrawCallLinearCost", "First DrawCall: Approximated linear cost", "us / MB",
                               QP_KEY_TAG_TIME, contributionFitting.coefficient * 1024.0f * 1024.0f)
        << tcu::TestLog::Float("FirstDrawCallMedianCost", "First DrawCall: Median cost", "us", QP_KEY_TAG_TIME,
                               stats.firstRender.medianTime);
}

template <typename SampleType>
static typename EnableIf<void, SampleTypeTraits<SampleType>::HAS_SECOND_RENDER_STATS>::Type logSecondRenderContribution(
    tcu::TestLog &log, const std::vector<RenderSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    const LineParametersWithConfidence contributionFitting =
        fitLineToSamples(samples, &SampleType::secondRenderDuration);
    log << tcu::TestLog::Float("SecondDrawCallConstantCost", "Second DrawCall: Approximated contant cost", "us",
                               QP_KEY_TAG_TIME, contributionFitting.offset)
        << tcu::TestLog::Float("SecondDrawCallLinearCost", "Second DrawCall: Approximated linear cost", "us / MB",
                               QP_KEY_TAG_TIME, contributionFitting.coefficient * 1024.0f * 1024.0f)
        << tcu::TestLog::Float("SecondDrawCallMedianCost", "Second DrawCall: Median cost", "us", QP_KEY_TAG_TIME,
                               stats.secondRender.medianTime);
}

template <typename SampleType>
static typename EnableIf<void, Not<SampleTypeTraits<SampleType>::HAS_MAP_STATS>::Value>::Type logMapContribution(
    tcu::TestLog &log, const std::vector<UploadSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    DE_UNREF(log);
    DE_UNREF(samples);
    DE_UNREF(stats);
}

template <typename SampleType>
static typename EnableIf<void, Not<SampleTypeTraits<SampleType>::HAS_UNMAP_STATS>::Value>::Type logUnmapContribution(
    tcu::TestLog &log, const std::vector<UploadSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    DE_UNREF(log);
    DE_UNREF(samples);
    DE_UNREF(stats);
}

template <typename SampleType>
static typename EnableIf<void, Not<SampleTypeTraits<SampleType>::HAS_WRITE_STATS>::Value>::Type logWriteContribution(
    tcu::TestLog &log, const std::vector<UploadSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    DE_UNREF(log);
    DE_UNREF(samples);
    DE_UNREF(stats);
}

template <typename SampleType>
static typename EnableIf<void, Not<SampleTypeTraits<SampleType>::HAS_FLUSH_STATS>::Value>::Type logFlushContribution(
    tcu::TestLog &log, const std::vector<UploadSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    DE_UNREF(log);
    DE_UNREF(samples);
    DE_UNREF(stats);
}

template <typename SampleType>
static typename EnableIf<void, Not<SampleTypeTraits<SampleType>::HAS_ALLOC_STATS>::Value>::Type logAllocContribution(
    tcu::TestLog &log, const std::vector<UploadSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    DE_UNREF(log);
    DE_UNREF(samples);
    DE_UNREF(stats);
}

template <typename SampleType>
static typename EnableIf<void, Not<SampleTypeTraits<SampleType>::HAS_RENDER_STATS>::Value>::Type logRenderContribution(
    tcu::TestLog &log, const std::vector<RenderSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    DE_UNREF(log);
    DE_UNREF(samples);
    DE_UNREF(stats);
}

template <typename SampleType>
static typename EnableIf<void, Not<SampleTypeTraits<SampleType>::HAS_READ_STATS>::Value>::Type logReadContribution(
    tcu::TestLog &log, const std::vector<RenderSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    DE_UNREF(log);
    DE_UNREF(samples);
    DE_UNREF(stats);
}

template <typename SampleType>
static typename EnableIf<void, Not<SampleTypeTraits<SampleType>::HAS_UPLOAD_STATS>::Value>::Type logUploadContribution(
    tcu::TestLog &log, const std::vector<RenderSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    DE_UNREF(log);
    DE_UNREF(samples);
    DE_UNREF(stats);
}

template <typename SampleType>
static typename EnableIf<void, Not<SampleTypeTraits<SampleType>::HAS_TOTAL_STATS>::Value>::Type logTotalContribution(
    tcu::TestLog &log, const std::vector<RenderSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    DE_UNREF(log);
    DE_UNREF(samples);
    DE_UNREF(stats);
}

template <typename SampleType>
static typename EnableIf<void, Not<SampleTypeTraits<SampleType>::HAS_FIRST_RENDER_STATS>::Value>::Type logFirstRenderContribution(
    tcu::TestLog &log, const std::vector<RenderSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    DE_UNREF(log);
    DE_UNREF(samples);
    DE_UNREF(stats);
}

template <typename SampleType>
static typename EnableIf<void, Not<SampleTypeTraits<SampleType>::HAS_SECOND_RENDER_STATS>::Value>::Type logSecondRenderContribution(
    tcu::TestLog &log, const std::vector<RenderSampleResult<SampleType>> &samples,
    const typename SampleTypeTraits<SampleType>::StatsType &stats)
{
    DE_UNREF(log);
    DE_UNREF(samples);
    DE_UNREF(stats);
}

void logSampleList(tcu::TestLog &log, const LineParametersWithConfidence &theilSenFitting,
                   const std::vector<UploadSampleResult<SingleOperationDuration>> &samples)
{
    log << tcu::TestLog::SampleList("Samples", "Samples") << tcu::TestLog::SampleInfo
        << tcu::TestLog::ValueInfo("WrittenSize", "Written size", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("BufferSize", "Buffer size", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("UploadTime", "Upload time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("FitResidual", "Fit residual", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::EndSampleInfo;

    for (int sampleNdx = 0; sampleNdx < (int)samples.size(); ++sampleNdx)
    {
        const float fitResidual =
            (float)samples[sampleNdx].duration.fitResponseDuration -
            (theilSenFitting.offset + theilSenFitting.coefficient * (float)samples[sampleNdx].writtenSize);
        log << tcu::TestLog::Sample << samples[sampleNdx].writtenSize << samples[sampleNdx].bufferSize
            << (int)samples[sampleNdx].duration.totalDuration << fitResidual << tcu::TestLog::EndSample;
    }

    log << tcu::TestLog::EndSampleList;
}

void logSampleList(tcu::TestLog &log, const LineParametersWithConfidence &theilSenFitting,
                   const std::vector<UploadSampleResult<MapBufferRangeDuration>> &samples)
{
    log << tcu::TestLog::SampleList("Samples", "Samples") << tcu::TestLog::SampleInfo
        << tcu::TestLog::ValueInfo("WrittenSize", "Written size", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("BufferSize", "Buffer size", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("TotalTime", "Total time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("AllocTime", "Alloc time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("MapTime", "Map time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("UnmapTime", "Unmap time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("WriteTime", "Write time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("FitResidual", "Fit residual", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::EndSampleInfo;

    for (int sampleNdx = 0; sampleNdx < (int)samples.size(); ++sampleNdx)
    {
        const float fitResidual =
            (float)samples[sampleNdx].duration.fitResponseDuration -
            (theilSenFitting.offset + theilSenFitting.coefficient * (float)samples[sampleNdx].writtenSize);
        log << tcu::TestLog::Sample << samples[sampleNdx].writtenSize << samples[sampleNdx].bufferSize
            << (int)samples[sampleNdx].duration.totalDuration << (int)samples[sampleNdx].duration.allocDuration
            << (int)samples[sampleNdx].duration.mapDuration << (int)samples[sampleNdx].duration.unmapDuration
            << (int)samples[sampleNdx].duration.writeDuration << fitResidual << tcu::TestLog::EndSample;
    }

    log << tcu::TestLog::EndSampleList;
}

void logSampleList(tcu::TestLog &log, const LineParametersWithConfidence &theilSenFitting,
                   const std::vector<UploadSampleResult<MapBufferRangeDurationNoAlloc>> &samples)
{
    log << tcu::TestLog::SampleList("Samples", "Samples") << tcu::TestLog::SampleInfo
        << tcu::TestLog::ValueInfo("WrittenSize", "Written size", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("BufferSize", "Buffer size", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("TotalTime", "Total time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("MapTime", "Map time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("UnmapTime", "Unmap time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("WriteTime", "Write time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("FitResidual", "Fit residual", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::EndSampleInfo;

    for (int sampleNdx = 0; sampleNdx < (int)samples.size(); ++sampleNdx)
    {
        const float fitResidual =
            (float)samples[sampleNdx].duration.fitResponseDuration -
            (theilSenFitting.offset + theilSenFitting.coefficient * (float)samples[sampleNdx].writtenSize);
        log << tcu::TestLog::Sample << samples[sampleNdx].writtenSize << samples[sampleNdx].bufferSize
            << (int)samples[sampleNdx].duration.totalDuration << (int)samples[sampleNdx].duration.mapDuration
            << (int)samples[sampleNdx].duration.unmapDuration << (int)samples[sampleNdx].duration.writeDuration
            << fitResidual << tcu::TestLog::EndSample;
    }

    log << tcu::TestLog::EndSampleList;
}

void logSampleList(tcu::TestLog &log, const LineParametersWithConfidence &theilSenFitting,
                   const std::vector<UploadSampleResult<MapBufferRangeFlushDuration>> &samples)
{
    log << tcu::TestLog::SampleList("Samples", "Samples") << tcu::TestLog::SampleInfo
        << tcu::TestLog::ValueInfo("WrittenSize", "Written size", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("BufferSize", "Buffer size", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("TotalTime", "Total time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("AllocTime", "Alloc time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("MapTime", "Map time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("UnmapTime", "Unmap time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("WriteTime", "Write time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("FlushTime", "Flush time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("FitResidual", "Fit residual", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::EndSampleInfo;

    for (int sampleNdx = 0; sampleNdx < (int)samples.size(); ++sampleNdx)
    {
        const float fitResidual =
            (float)samples[sampleNdx].duration.fitResponseDuration -
            (theilSenFitting.offset + theilSenFitting.coefficient * (float)samples[sampleNdx].writtenSize);
        log << tcu::TestLog::Sample << samples[sampleNdx].writtenSize << samples[sampleNdx].bufferSize
            << (int)samples[sampleNdx].duration.totalDuration << (int)samples[sampleNdx].duration.allocDuration
            << (int)samples[sampleNdx].duration.mapDuration << (int)samples[sampleNdx].duration.unmapDuration
            << (int)samples[sampleNdx].duration.writeDuration << (int)samples[sampleNdx].duration.flushDuration
            << fitResidual << tcu::TestLog::EndSample;
    }

    log << tcu::TestLog::EndSampleList;
}

void logSampleList(tcu::TestLog &log, const LineParametersWithConfidence &theilSenFitting,
                   const std::vector<UploadSampleResult<MapBufferRangeFlushDurationNoAlloc>> &samples)
{
    log << tcu::TestLog::SampleList("Samples", "Samples") << tcu::TestLog::SampleInfo
        << tcu::TestLog::ValueInfo("WrittenSize", "Written size", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("BufferSize", "Buffer size", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("TotalTime", "Total time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("MapTime", "Map time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("UnmapTime", "Unmap time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("WriteTime", "Write time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("FlushTime", "Flush time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("FitResidual", "Fit residual", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::EndSampleInfo;

    for (int sampleNdx = 0; sampleNdx < (int)samples.size(); ++sampleNdx)
    {
        const float fitResidual =
            (float)samples[sampleNdx].duration.fitResponseDuration -
            (theilSenFitting.offset + theilSenFitting.coefficient * (float)samples[sampleNdx].writtenSize);
        log << tcu::TestLog::Sample << samples[sampleNdx].writtenSize << samples[sampleNdx].bufferSize
            << (int)samples[sampleNdx].duration.totalDuration << (int)samples[sampleNdx].duration.mapDuration
            << (int)samples[sampleNdx].duration.unmapDuration << (int)samples[sampleNdx].duration.writeDuration
            << (int)samples[sampleNdx].duration.flushDuration << fitResidual << tcu::TestLog::EndSample;
    }

    log << tcu::TestLog::EndSampleList;
}

void logSampleList(tcu::TestLog &log, const LineParametersWithConfidence &theilSenFitting,
                   const std::vector<RenderSampleResult<RenderReadDuration>> &samples)
{
    log << tcu::TestLog::SampleList("Samples", "Samples") << tcu::TestLog::SampleInfo
        << tcu::TestLog::ValueInfo("DataSize", "Data processed", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("VertexCount", "Number of vertices", "vertices", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("TotalTime", "Total time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("DrawCallTime", "Draw call time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("ReadTime", "ReadPixels time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("FitResidual", "Fit residual", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::EndSampleInfo;

    for (int sampleNdx = 0; sampleNdx < (int)samples.size(); ++sampleNdx)
    {
        const float fitResidual =
            (float)samples[sampleNdx].duration.fitResponseDuration -
            (theilSenFitting.offset + theilSenFitting.coefficient * (float)samples[sampleNdx].renderDataSize);
        log << tcu::TestLog::Sample << samples[sampleNdx].renderDataSize << samples[sampleNdx].numVertices
            << (int)samples[sampleNdx].duration.renderReadDuration << (int)samples[sampleNdx].duration.renderDuration
            << (int)samples[sampleNdx].duration.readDuration << fitResidual << tcu::TestLog::EndSample;
    }

    log << tcu::TestLog::EndSampleList;
}

void logSampleList(tcu::TestLog &log, const LineParametersWithConfidence &theilSenFitting,
                   const std::vector<RenderSampleResult<UnrelatedUploadRenderReadDuration>> &samples)
{
    log << tcu::TestLog::SampleList("Samples", "Samples") << tcu::TestLog::SampleInfo
        << tcu::TestLog::ValueInfo("DataSize", "Data processed", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("VertexCount", "Number of vertices", "vertices", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("UnrelatedUploadSize", "Unrelated upload size", "bytes",
                                   QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("TotalTime", "Total time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("DrawCallTime", "Draw call time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("ReadTime", "ReadPixels time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("FitResidual", "Fit residual", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::EndSampleInfo;

    for (int sampleNdx = 0; sampleNdx < (int)samples.size(); ++sampleNdx)
    {
        const float fitResidual =
            (float)samples[sampleNdx].duration.fitResponseDuration -
            (theilSenFitting.offset + theilSenFitting.coefficient * (float)samples[sampleNdx].renderDataSize);
        log << tcu::TestLog::Sample << samples[sampleNdx].renderDataSize << samples[sampleNdx].numVertices
            << samples[sampleNdx].unrelatedDataSize << (int)samples[sampleNdx].duration.renderReadDuration
            << (int)samples[sampleNdx].duration.renderDuration << (int)samples[sampleNdx].duration.readDuration
            << fitResidual << tcu::TestLog::EndSample;
    }

    log << tcu::TestLog::EndSampleList;
}

void logSampleList(tcu::TestLog &log, const LineParametersWithConfidence &theilSenFitting,
                   const std::vector<RenderSampleResult<UploadRenderReadDuration>> &samples)
{
    log << tcu::TestLog::SampleList("Samples", "Samples") << tcu::TestLog::SampleInfo
        << tcu::TestLog::ValueInfo("DataSize", "Data processed", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("UploadSize", "Data uploaded", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("VertexCount", "Number of vertices", "vertices", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("DrawReadTime", "Draw call and ReadPixels time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("TotalTime", "Total time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("Upload time", "Upload time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("DrawCallTime", "Draw call time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("ReadTime", "ReadPixels time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("FitResidual", "Fit residual", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::EndSampleInfo;

    for (int sampleNdx = 0; sampleNdx < (int)samples.size(); ++sampleNdx)
    {
        const float fitResidual =
            (float)samples[sampleNdx].duration.fitResponseDuration -
            (theilSenFitting.offset + theilSenFitting.coefficient * (float)samples[sampleNdx].renderDataSize);
        log << tcu::TestLog::Sample << samples[sampleNdx].renderDataSize << samples[sampleNdx].uploadedDataSize
            << samples[sampleNdx].numVertices << (int)samples[sampleNdx].duration.renderReadDuration
            << (int)samples[sampleNdx].duration.totalDuration << (int)samples[sampleNdx].duration.uploadDuration
            << (int)samples[sampleNdx].duration.renderDuration << (int)samples[sampleNdx].duration.readDuration
            << fitResidual << tcu::TestLog::EndSample;
    }

    log << tcu::TestLog::EndSampleList;
}

void logSampleList(tcu::TestLog &log, const LineParametersWithConfidence &theilSenFitting,
                   const std::vector<RenderSampleResult<UploadRenderReadDurationWithUnrelatedUploadSize>> &samples)
{
    log << tcu::TestLog::SampleList("Samples", "Samples") << tcu::TestLog::SampleInfo
        << tcu::TestLog::ValueInfo("DataSize", "Data processed", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("UploadSize", "Data uploaded", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("VertexCount", "Number of vertices", "vertices", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("UnrelatedUploadSize", "Unrelated upload size", "bytes",
                                   QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("DrawReadTime", "Draw call and ReadPixels time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("TotalTime", "Total time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("Upload time", "Upload time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("DrawCallTime", "Draw call time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("ReadTime", "ReadPixels time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("FitResidual", "Fit residual", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::EndSampleInfo;

    for (int sampleNdx = 0; sampleNdx < (int)samples.size(); ++sampleNdx)
    {
        const float fitResidual =
            (float)samples[sampleNdx].duration.fitResponseDuration -
            (theilSenFitting.offset + theilSenFitting.coefficient * (float)samples[sampleNdx].renderDataSize);
        log << tcu::TestLog::Sample << samples[sampleNdx].renderDataSize << samples[sampleNdx].uploadedDataSize
            << samples[sampleNdx].numVertices << samples[sampleNdx].unrelatedDataSize
            << (int)samples[sampleNdx].duration.renderReadDuration << (int)samples[sampleNdx].duration.totalDuration
            << (int)samples[sampleNdx].duration.uploadDuration << (int)samples[sampleNdx].duration.renderDuration
            << (int)samples[sampleNdx].duration.readDuration << fitResidual << tcu::TestLog::EndSample;
    }

    log << tcu::TestLog::EndSampleList;
}

void logSampleList(tcu::TestLog &log, const LineParametersWithConfidence &theilSenFitting,
                   const std::vector<RenderSampleResult<RenderUploadRenderReadDuration>> &samples)
{
    log << tcu::TestLog::SampleList("Samples", "Samples") << tcu::TestLog::SampleInfo
        << tcu::TestLog::ValueInfo("DataSize", "Data processed", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("UploadSize", "Data uploaded", "bytes", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("VertexCount", "Number of vertices", "vertices", QP_SAMPLE_VALUE_TAG_PREDICTOR)
        << tcu::TestLog::ValueInfo("DrawReadTime", "Second draw call and ReadPixels time", "us",
                                   QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("TotalTime", "Total time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("FirstDrawCallTime", "First draw call time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("Upload time", "Upload time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("SecondDrawCallTime", "Second draw call time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("ReadTime", "ReadPixels time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::ValueInfo("FitResidual", "Fit residual", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
        << tcu::TestLog::EndSampleInfo;

    for (int sampleNdx = 0; sampleNdx < (int)samples.size(); ++sampleNdx)
    {
        const float fitResidual =
            (float)samples[sampleNdx].duration.fitResponseDuration -
            (theilSenFitting.offset + theilSenFitting.coefficient * (float)samples[sampleNdx].renderDataSize);
        log << tcu::TestLog::Sample << samples[sampleNdx].renderDataSize << samples[sampleNdx].uploadedDataSize
            << samples[sampleNdx].numVertices << (int)samples[sampleNdx].duration.renderReadDuration
            << (int)samples[sampleNdx].duration.totalDuration << (int)samples[sampleNdx].duration.firstRenderDuration
            << (int)samples[sampleNdx].duration.uploadDuration << (int)samples[sampleNdx].duration.secondRenderDuration
            << (int)samples[sampleNdx].duration.readDuration << fitResidual << tcu::TestLog::EndSample;
    }

    log << tcu::TestLog::EndSampleList;
}

template <typename SampleType>
static UploadSampleAnalyzeResult analyzeSampleResults(tcu::TestLog &log,
                                                      const std::vector<UploadSampleResult<SampleType>> &samples,
                                                      bool logBucketPerformance)
{
    // Assume data is linear with some outliers, fit a line
    const LineParametersWithConfidence theilSenFitting = fitLineToSamples(samples);
    const typename SampleTypeTraits<SampleType>::StatsType resultStats =
        calculateSampleStatistics(theilSenFitting, samples);
    float approximatedTransferRate;
    float approximatedTransferRateNoConstant;

    // Output raw samples
    {
        const tcu::ScopedLogSection section(log, "Samples", "Samples");
        logSampleList(log, theilSenFitting, samples);
    }

    // Calculate results for different ranges
    if (logBucketPerformance)
    {
        const int numBuckets = 4;
        int minBufferSize    = 0;
        int maxBufferSize    = 0;
        std::vector<UploadSampleResult<SampleType>> buckets[numBuckets];

        bucketizeSamplesUniformly(samples, &buckets[0], numBuckets, minBufferSize, maxBufferSize);

        for (int bucketNdx = 0; bucketNdx < numBuckets; ++bucketNdx)
        {
            if (buckets[bucketNdx].empty())
                continue;

            // Print a nice result summary

            const int bucketRangeMin =
                minBufferSize + (int)(((float)bucketNdx / (float)numBuckets) * (float)(maxBufferSize - minBufferSize));
            const int bucketRangeMax = minBufferSize + (int)(((float)(bucketNdx + 1) / (float)numBuckets) *
                                                             (float)(maxBufferSize - minBufferSize));
            const typename SampleTypeTraits<SampleType>::StatsType stats =
                calculateSampleStatistics(theilSenFitting, buckets[bucketNdx]);
            const tcu::ScopedLogSection section(
                log, "BufferSizeRange",
                std::string("Transfer performance with buffer size in range [")
                    .append(getHumanReadableByteSize(bucketRangeMin)
                                .append(", ")
                                .append(getHumanReadableByteSize(bucketRangeMax).append("]"))));

            logMapRangeStats<SampleType>(log, stats);
            logUnmapStats<SampleType>(log, stats);
            logWriteStats<SampleType>(log, stats);
            logFlushStats<SampleType>(log, stats);
            logAllocStats<SampleType>(log, stats);

            log << tcu::TestLog::Float("Min", "Total: Min time", "us", QP_KEY_TAG_TIME, stats.result.minTime)
                << tcu::TestLog::Float("Max", "Total: Max time", "us", QP_KEY_TAG_TIME, stats.result.maxTime)
                << tcu::TestLog::Float("Min90", "Total: 90%-Min time", "us", QP_KEY_TAG_TIME,
                                       stats.result.min2DecileTime)
                << tcu::TestLog::Float("Max90", "Total: 90%-Max time", "us", QP_KEY_TAG_TIME,
                                       stats.result.max9DecileTime)
                << tcu::TestLog::Float("Median", "Total: Median time", "us", QP_KEY_TAG_TIME, stats.result.medianTime)
                << tcu::TestLog::Float("MedianTransfer", "Median transfer rate", "MB / s", QP_KEY_TAG_PERFORMANCE,
                                       stats.medianRate / 1024.0f / 1024.0f)
                << tcu::TestLog::Float("MaxDiff", "Max difference to approximated", "us", QP_KEY_TAG_TIME,
                                       stats.maxDiffTime)
                << tcu::TestLog::Float("Max90Diff", "90%-Max difference to approximated", "us", QP_KEY_TAG_TIME,
                                       stats.maxDiff9DecileTime)
                << tcu::TestLog::Float("MedianDiff", "Median difference to approximated", "us", QP_KEY_TAG_TIME,
                                       stats.medianDiffTime)
                << tcu::TestLog::Float("MaxRelDiff", "Max relative difference to approximated", "%", QP_KEY_TAG_NONE,
                                       stats.maxRelDiffTime * 100.0f)
                << tcu::TestLog::Float("Max90RelDiff", "90%-Max relative difference to approximated", "%",
                                       QP_KEY_TAG_NONE, stats.max9DecileRelDiffTime * 100.0f)
                << tcu::TestLog::Float("MedianRelDiff", "Median relative difference to approximated", "%",
                                       QP_KEY_TAG_NONE, stats.medianRelDiffTime * 100.0f);
        }
    }

    // Contributions
    if (SampleTypeTraits<SampleType>::LOG_CONTRIBUTIONS)
    {
        const tcu::ScopedLogSection section(log, "Contribution", "Contributions");

        logMapContribution(log, samples, resultStats);
        logUnmapContribution(log, samples, resultStats);
        logWriteContribution(log, samples, resultStats);
        logFlushContribution(log, samples, resultStats);
        logAllocContribution(log, samples, resultStats);
    }

    // Print results
    {
        const tcu::ScopedLogSection section(log, "Results", "Results");

        const int medianBufferSize = (samples.front().bufferSize + samples.back().bufferSize) / 2;
        const float approximatedTransferTime =
            (theilSenFitting.offset + theilSenFitting.coefficient * (float)medianBufferSize) / 1000.0f / 1000.0f;
        const float approximatedTransferTimeNoConstant =
            (theilSenFitting.coefficient * (float)medianBufferSize) / 1000.0f / 1000.0f;
        const float sampleLinearity         = calculateSampleFitLinearity(samples);
        const float sampleTemporalStability = calculateSampleTemporalStability(samples);

        approximatedTransferRateNoConstant = (float)medianBufferSize / approximatedTransferTimeNoConstant;
        approximatedTransferRate           = (float)medianBufferSize / approximatedTransferTime;

        log << tcu::TestLog::Float("ResultLinearity", "Sample linearity", "%", QP_KEY_TAG_QUALITY,
                                   sampleLinearity * 100.0f)
            << tcu::TestLog::Float("SampleTemporalStability", "Sample temporal stability", "%", QP_KEY_TAG_QUALITY,
                                   sampleTemporalStability * 100.0f)
            << tcu::TestLog::Float("ApproximatedConstantCost", "Approximated contant cost", "us", QP_KEY_TAG_TIME,
                                   theilSenFitting.offset)
            << tcu::TestLog::Float("ApproximatedConstantCostConfidence60Lower",
                                   "Approximated contant cost 60% confidence lower limit", "us", QP_KEY_TAG_TIME,
                                   theilSenFitting.offsetConfidenceLower)
            << tcu::TestLog::Float("ApproximatedConstantCostConfidence60Upper",
                                   "Approximated contant cost 60% confidence upper limit", "us", QP_KEY_TAG_TIME,
                                   theilSenFitting.offsetConfidenceUpper)
            << tcu::TestLog::Float("ApproximatedLinearCost", "Approximated linear cost", "us / MB", QP_KEY_TAG_TIME,
                                   theilSenFitting.coefficient * 1024.0f * 1024.0f)
            << tcu::TestLog::Float("ApproximatedLinearCostConfidence60Lower",
                                   "Approximated linear cost 60% confidence lower limit", "us / MB", QP_KEY_TAG_TIME,
                                   theilSenFitting.coefficientConfidenceLower * 1024.0f * 1024.0f)
            << tcu::TestLog::Float("ApproximatedLinearCostConfidence60Upper",
                                   "Approximated linear cost 60% confidence upper limit", "us / MB", QP_KEY_TAG_TIME,
                                   theilSenFitting.coefficientConfidenceUpper * 1024.0f * 1024.0f)
            << tcu::TestLog::Float("ApproximatedTransferRate", "Approximated transfer rate", "MB / s",
                                   QP_KEY_TAG_PERFORMANCE, approximatedTransferRate / 1024.0f / 1024.0f)
            << tcu::TestLog::Float("ApproximatedTransferRateNoConstant",
                                   "Approximated transfer rate without constant cost", "MB / s", QP_KEY_TAG_PERFORMANCE,
                                   approximatedTransferRateNoConstant / 1024.0f / 1024.0f)
            << tcu::TestLog::Float("SampleMedianTime", "Median sample time", "us", QP_KEY_TAG_TIME,
                                   resultStats.result.medianTime)
            << tcu::TestLog::Float("SampleMedianTransfer", "Median transfer rate", "MB / s", QP_KEY_TAG_PERFORMANCE,
                                   resultStats.medianRate / 1024.0f / 1024.0f);
    }

    // return approximated transfer rate
    {
        UploadSampleAnalyzeResult result;

        result.transferRateMedian     = resultStats.medianRate;
        result.transferRateAtRange    = approximatedTransferRate;
        result.transferRateAtInfinity = approximatedTransferRateNoConstant;

        return result;
    }
}

template <typename SampleType>
static RenderSampleAnalyzeResult analyzeSampleResults(tcu::TestLog &log,
                                                      const std::vector<RenderSampleResult<SampleType>> &samples)
{
    // Assume data is linear with some outliers, fit a line
    const LineParametersWithConfidence theilSenFitting = fitLineToSamples(samples);
    const typename SampleTypeTraits<SampleType>::StatsType resultStats =
        calculateSampleStatistics(theilSenFitting, samples);
    float approximatedProcessingRate;
    float approximatedProcessingRateNoConstant;

    // output raw samples
    {
        const tcu::ScopedLogSection section(log, "Samples", "Samples");
        logSampleList(log, theilSenFitting, samples);
    }

    // Contributions
    if (SampleTypeTraits<SampleType>::LOG_CONTRIBUTIONS)
    {
        const tcu::ScopedLogSection section(log, "Contribution", "Contributions");

        logFirstRenderContribution(log, samples, resultStats);
        logUploadContribution(log, samples, resultStats);
        logRenderContribution(log, samples, resultStats);
        logSecondRenderContribution(log, samples, resultStats);
        logReadContribution(log, samples, resultStats);
        logTotalContribution(log, samples, resultStats);
    }

    // print results
    {
        const tcu::ScopedLogSection section(log, "Results", "Results");

        const int medianDataSize = (samples.front().renderDataSize + samples.back().renderDataSize) / 2;
        const float approximatedRenderTime =
            (theilSenFitting.offset + theilSenFitting.coefficient * (float)medianDataSize) / 1000.0f / 1000.0f;
        const float approximatedRenderTimeNoConstant =
            (theilSenFitting.coefficient * (float)medianDataSize) / 1000.0f / 1000.0f;
        const float sampleLinearity         = calculateSampleFitLinearity(samples);
        const float sampleTemporalStability = calculateSampleTemporalStability(samples);

        approximatedProcessingRateNoConstant = (float)medianDataSize / approximatedRenderTimeNoConstant;
        approximatedProcessingRate           = (float)medianDataSize / approximatedRenderTime;

        log << tcu::TestLog::Float("ResultLinearity", "Sample linearity", "%", QP_KEY_TAG_QUALITY,
                                   sampleLinearity * 100.0f)
            << tcu::TestLog::Float("SampleTemporalStability", "Sample temporal stability", "%", QP_KEY_TAG_QUALITY,
                                   sampleTemporalStability * 100.0f)
            << tcu::TestLog::Float("ApproximatedConstantCost", "Approximated contant cost", "us", QP_KEY_TAG_TIME,
                                   theilSenFitting.offset)
            << tcu::TestLog::Float("ApproximatedConstantCostConfidence60Lower",
                                   "Approximated contant cost 60% confidence lower limit", "us", QP_KEY_TAG_TIME,
                                   theilSenFitting.offsetConfidenceLower)
            << tcu::TestLog::Float("ApproximatedConstantCostConfidence60Upper",
                                   "Approximated contant cost 60% confidence upper limit", "us", QP_KEY_TAG_TIME,
                                   theilSenFitting.offsetConfidenceUpper)
            << tcu::TestLog::Float("ApproximatedLinearCost", "Approximated linear cost", "us / MB", QP_KEY_TAG_TIME,
                                   theilSenFitting.coefficient * 1024.0f * 1024.0f)
            << tcu::TestLog::Float("ApproximatedLinearCostConfidence60Lower",
                                   "Approximated linear cost 60% confidence lower limit", "us / MB", QP_KEY_TAG_TIME,
                                   theilSenFitting.coefficientConfidenceLower * 1024.0f * 1024.0f)
            << tcu::TestLog::Float("ApproximatedLinearCostConfidence60Upper",
                                   "Approximated linear cost 60% confidence upper limit", "us / MB", QP_KEY_TAG_TIME,
                                   theilSenFitting.coefficientConfidenceUpper * 1024.0f * 1024.0f)
            << tcu::TestLog::Float("ApproximatedProcessRate", "Approximated processing rate", "MB / s",
                                   QP_KEY_TAG_PERFORMANCE, approximatedProcessingRate / 1024.0f / 1024.0f)
            << tcu::TestLog::Float("ApproximatedProcessRateNoConstant",
                                   "Approximated processing rate without constant cost", "MB / s",
                                   QP_KEY_TAG_PERFORMANCE, approximatedProcessingRateNoConstant / 1024.0f / 1024.0f)
            << tcu::TestLog::Float("SampleMedianTime", "Median sample time", "us", QP_KEY_TAG_TIME,
                                   resultStats.result.medianTime)
            << tcu::TestLog::Float("SampleMedianProcess", "Median processing rate", "MB / s", QP_KEY_TAG_PERFORMANCE,
                                   resultStats.medianRate / 1024.0f / 1024.0f);
    }

    // return approximated render rate
    {
        RenderSampleAnalyzeResult result;

        result.renderRateMedian     = resultStats.medianRate;
        result.renderRateAtRange    = approximatedProcessingRate;
        result.renderRateAtInfinity = approximatedProcessingRateNoConstant;

        return result;
    }
    return RenderSampleAnalyzeResult();
}

static void generateTwoPassRandomIterationOrder(std::vector<int> &iterationOrder, int numSamples)
{
    de::Random rnd(0xabc);
    const int midPoint = (numSamples + 1) / 2; // !< ceil(m_numSamples / 2)

    DE_ASSERT((int)iterationOrder.size() == numSamples);

    // Two "passes" over range, randomize order in both passes
    // This allows to us detect if iterations are not independent
    // (first run and later run samples differ significantly?)

    for (int sampleNdx = 0; sampleNdx < midPoint; ++sampleNdx)
        iterationOrder[sampleNdx] = sampleNdx * 2;
    for (int sampleNdx = midPoint; sampleNdx < numSamples; ++sampleNdx)
        iterationOrder[sampleNdx] = (sampleNdx - midPoint) * 2 + 1;

    for (int ndx = 0; ndx < midPoint; ++ndx)
        std::swap(iterationOrder[ndx], iterationOrder[rnd.getInt(0, midPoint - 1)]);
    for (int ndx = midPoint; ndx < (int)iterationOrder.size(); ++ndx)
        std::swap(iterationOrder[ndx], iterationOrder[rnd.getInt(midPoint, (int)iterationOrder.size() - 1)]);
}

template <typename SampleType>
class BasicBufferCase : public TestCase
{
public:
    enum Flags
    {
        FLAG_ALLOCATE_LARGER_BUFFER = 0x01,
    };
    BasicBufferCase(Context &context, const char *name, const char *desc, int bufferSizeMin, int bufferSizeMax,
                    int numSamples, int flags);
    ~BasicBufferCase(void);

    virtual void init(void);
    virtual void deinit(void);

protected:
    IterateResult iterate(void);

    virtual bool runSample(int iteration, UploadSampleResult<SampleType> &sample)                = 0;
    virtual void logAndSetTestResult(const std::vector<UploadSampleResult<SampleType>> &results) = 0;

    void disableGLWarmup(void);
    void waitGLResults(void);

    enum
    {
        UNUSED_RENDER_AREA_SIZE = 32
    };

    glu::ShaderProgram *m_minimalProgram;
    int32_t m_minimalProgramPosLoc;
    uint32_t m_bufferID;

    const int m_numSamples;
    const int m_bufferSizeMin;
    const int m_bufferSizeMax;
    const bool m_allocateLargerBuffer;

private:
    int m_iteration;
    std::vector<int> m_iterationOrder;
    std::vector<UploadSampleResult<SampleType>> m_results;

    bool m_useGL;
    int m_bufferRandomizerTimer;
};

template <typename SampleType>
BasicBufferCase<SampleType>::BasicBufferCase(Context &context, const char *name, const char *desc, int bufferSizeMin,
                                             int bufferSizeMax, int numSamples, int flags)
    : TestCase(context, tcu::NODETYPE_PERFORMANCE, name, desc)
    , m_minimalProgram(nullptr)
    , m_minimalProgramPosLoc(-1)
    , m_bufferID(0)
    , m_numSamples(numSamples)
    , m_bufferSizeMin(bufferSizeMin)
    , m_bufferSizeMax(bufferSizeMax)
    , m_allocateLargerBuffer((flags & FLAG_ALLOCATE_LARGER_BUFFER) != 0)
    , m_iteration(0)
    , m_iterationOrder(numSamples)
    , m_results(numSamples)
    , m_useGL(true)
    , m_bufferRandomizerTimer(0)
{
    // "randomize" iteration order. Deterministic, patternless
    generateTwoPassRandomIterationOrder(m_iterationOrder, m_numSamples);

    // choose buffer sizes
    for (int sampleNdx = 0; sampleNdx < m_numSamples; ++sampleNdx)
    {
        const int rawBufferSize =
            (int)deFloatFloor((float)bufferSizeMin +
                              (float)(bufferSizeMax - bufferSizeMin) * ((float)(sampleNdx + 1) / (float)m_numSamples));
        const int bufferSize = deAlign32(rawBufferSize, 16);
        const int allocatedBufferSize =
            deAlign32((m_allocateLargerBuffer) ? ((int)((float)bufferSize * 1.5f)) : (bufferSize), 16);

        m_results[sampleNdx].bufferSize    = bufferSize;
        m_results[sampleNdx].allocatedSize = allocatedBufferSize;
        m_results[sampleNdx].writtenSize   = -1;
    }
}

template <typename SampleType>
BasicBufferCase<SampleType>::~BasicBufferCase(void)
{
    deinit();
}

template <typename SampleType>
void BasicBufferCase<SampleType>::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    if (!m_useGL)
        return;

    // \note Viewport size is not checked, it won't matter if the render target actually is smaller than UNUSED_RENDER_AREA_SIZE

    // minimal shader

    m_minimalProgram = new glu::ShaderProgram(m_context.getRenderContext(),
                                              glu::ProgramSources() << glu::VertexSource(s_minimalVertexShader)
                                                                    << glu::FragmentSource(s_minimalFragnentShader));
    if (!m_minimalProgram->isOk())
    {
        m_testCtx.getLog() << *m_minimalProgram;
        throw tcu::TestError("failed to build shader program");
    }

    m_minimalProgramPosLoc = gl.getAttribLocation(m_minimalProgram->getProgram(), "a_position");
    if (m_minimalProgramPosLoc == -1)
        throw tcu::TestError("a_position location was -1");
}

template <typename SampleType>
void BasicBufferCase<SampleType>::deinit(void)
{
    if (m_bufferID)
    {
        m_context.getRenderContext().getFunctions().deleteBuffers(1, &m_bufferID);
        m_bufferID = 0;
    }

    delete m_minimalProgram;
    m_minimalProgram = nullptr;
}

template <typename SampleType>
TestCase::IterateResult BasicBufferCase<SampleType>::iterate(void)
{
    const glw::Functions &gl    = m_context.getRenderContext().getFunctions();
    static bool buffersWarmedUp = false;

    static const uint32_t usages[] = {
        GL_STREAM_DRAW, GL_STREAM_READ,  GL_STREAM_COPY,  GL_STATIC_DRAW,  GL_STATIC_READ,
        GL_STATIC_COPY, GL_DYNAMIC_DRAW, GL_DYNAMIC_READ, GL_DYNAMIC_COPY,
    };

    // Allocate some random sized buffers and remove them to
    // make sure the first samples too have some buffers removed
    // just before their allocation. This is only needed by the
    // the first test.

    if (m_useGL && !buffersWarmedUp)
    {
        const int numRandomBuffers = 6;
        const int numRepeats       = 10;
        const int maxBufferSize    = 16777216;
        const std::vector<uint8_t> zeroData(maxBufferSize, 0x00);
        de::Random rnd(0x1234);
        uint32_t bufferIDs[numRandomBuffers] = {0};

        gl.useProgram(m_minimalProgram->getProgram());
        gl.viewport(0, 0, UNUSED_RENDER_AREA_SIZE, UNUSED_RENDER_AREA_SIZE);
        gl.enableVertexAttribArray(m_minimalProgramPosLoc);

        for (int ndx = 0; ndx < numRepeats; ++ndx)
        {
            // Create buffer and maybe draw from it
            for (int randomBufferNdx = 0; randomBufferNdx < numRandomBuffers; ++randomBufferNdx)
            {
                const int randomSize = deAlign32(rnd.getInt(1, maxBufferSize), 4 * 4);
                const uint32_t usage = usages[rnd.getUint32() % (uint32_t)DE_LENGTH_OF_ARRAY(usages)];

                gl.genBuffers(1, &bufferIDs[randomBufferNdx]);
                gl.bindBuffer(GL_ARRAY_BUFFER, bufferIDs[randomBufferNdx]);
                gl.bufferData(GL_ARRAY_BUFFER, randomSize, &zeroData[0], usage);

                if (rnd.getBool())
                {
                    gl.vertexAttribPointer(m_minimalProgramPosLoc, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
                    gl.drawArrays(GL_POINTS, 0, 1);
                    gl.drawArrays(GL_POINTS, randomSize / (int)sizeof(float[4]) - 1, 1);
                }
            }

            for (int randomBufferNdx = 0; randomBufferNdx < numRandomBuffers; ++randomBufferNdx)
                gl.deleteBuffers(1, &bufferIDs[randomBufferNdx]);

            waitGLResults();
            GLU_EXPECT_NO_ERROR(gl.getError(), "Buffer gen");

            m_testCtx.touchWatchdog();
        }

        buffersWarmedUp = true;
        return CONTINUE;
    }
    else if (m_useGL && m_bufferRandomizerTimer++ % 8 == 0)
    {
        // Do some random buffer operations to every now and then
        // to make sure the previous test iterations won't affect
        // following test runs.

        const int numRandomBuffers = 3;
        const int maxBufferSize    = 16777216;
        const std::vector<uint8_t> zeroData(maxBufferSize, 0x00);
        de::Random rnd(0x1234 + 0xabc * m_bufferRandomizerTimer);

        // BufferData
        {
            uint32_t bufferIDs[numRandomBuffers] = {0};

            for (int randomBufferNdx = 0; randomBufferNdx < numRandomBuffers; ++randomBufferNdx)
            {
                const int randomSize = deAlign32(rnd.getInt(1, maxBufferSize), 4 * 4);
                const uint32_t usage = usages[rnd.getUint32() % (uint32_t)DE_LENGTH_OF_ARRAY(usages)];

                gl.genBuffers(1, &bufferIDs[randomBufferNdx]);
                gl.bindBuffer(GL_ARRAY_BUFFER, bufferIDs[randomBufferNdx]);
                gl.bufferData(GL_ARRAY_BUFFER, randomSize, &zeroData[0], usage);
            }

            for (int randomBufferNdx = 0; randomBufferNdx < numRandomBuffers; ++randomBufferNdx)
                gl.deleteBuffers(1, &bufferIDs[randomBufferNdx]);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "buffer ops");

        // Do some memory mappings
        {
            uint32_t bufferIDs[numRandomBuffers] = {0};

            for (int randomBufferNdx = 0; randomBufferNdx < numRandomBuffers; ++randomBufferNdx)
            {
                const int randomSize = deAlign32(rnd.getInt(1, maxBufferSize), 4 * 4);
                const uint32_t usage = usages[rnd.getUint32() % (uint32_t)DE_LENGTH_OF_ARRAY(usages)];
                void *ptr;

                gl.genBuffers(1, &bufferIDs[randomBufferNdx]);
                gl.bindBuffer(GL_ARRAY_BUFFER, bufferIDs[randomBufferNdx]);
                gl.bufferData(GL_ARRAY_BUFFER, randomSize, &zeroData[0], usage);

                gl.vertexAttribPointer(m_minimalProgramPosLoc, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
                gl.drawArrays(GL_POINTS, 0, 1);
                gl.drawArrays(GL_POINTS, randomSize / (int)sizeof(float[4]) - 1, 1);

                if (rnd.getBool())
                    waitGLResults();

                ptr = gl.mapBufferRange(GL_ARRAY_BUFFER, 0, randomSize, GL_MAP_WRITE_BIT);
                if (ptr)
                {
                    medianTimeMemcpy(ptr, &zeroData[0], randomSize);
                    gl.unmapBuffer(GL_ARRAY_BUFFER);
                }
            }

            for (int randomBufferNdx = 0; randomBufferNdx < numRandomBuffers; ++randomBufferNdx)
                gl.deleteBuffers(1, &bufferIDs[randomBufferNdx]);

            waitGLResults();
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "buffer maps");
        return CONTINUE;
    }
    else
    {
        const int currentIteration     = m_iteration;
        const int sampleNdx            = m_iterationOrder[currentIteration];
        const bool sampleRunSuccessful = runSample(currentIteration, m_results[sampleNdx]);

        GLU_EXPECT_NO_ERROR(gl.getError(), "post runSample()");

        // Retry failed samples
        if (!sampleRunSuccessful)
            return CONTINUE;

        if (++m_iteration >= m_numSamples)
        {
            logAndSetTestResult(m_results);
            return STOP;
        }
        else
            return CONTINUE;
    }
}

template <typename SampleType>
void BasicBufferCase<SampleType>::disableGLWarmup(void)
{
    m_useGL = false;
}

template <typename SampleType>
void BasicBufferCase<SampleType>::waitGLResults(void)
{
    tcu::Surface unusedSurface(UNUSED_RENDER_AREA_SIZE, UNUSED_RENDER_AREA_SIZE);
    glu::readPixels(m_context.getRenderContext(), 0, 0, unusedSurface.getAccess());
}

template <typename SampleType>
class BasicUploadCase : public BasicBufferCase<SampleType>
{
public:
    enum CaseType
    {
        CASE_NO_BUFFERS = 0,
        CASE_NEW_BUFFER,
        CASE_UNSPECIFIED_BUFFER,
        CASE_SPECIFIED_BUFFER,
        CASE_USED_BUFFER,
        CASE_USED_LARGER_BUFFER,

        CASE_LAST
    };

    enum CaseFlags
    {
        FLAG_DONT_LOG_BUFFER_INFO              = 0x01,
        FLAG_RESULT_BUFFER_UNSPECIFIED_CONTENT = 0x02,
    };

    enum ResultType
    {
        RESULT_MEDIAN_TRANSFER_RATE = 0,
        RESULT_ASYMPTOTIC_TRANSFER_RATE,
    };

    BasicUploadCase(Context &context, const char *name, const char *desc, int bufferSizeMin, int bufferSizeMax,
                    int numSamples, uint32_t bufferUsage, CaseType caseType, ResultType resultType, int flags = 0);

    ~BasicUploadCase(void);

    virtual void init(void);
    virtual void deinit(void);

private:
    bool runSample(int iteration, UploadSampleResult<SampleType> &sample);
    void createBuffer(int bufferSize, int iteration);
    void deleteBuffer(int bufferSize);
    void useBuffer(int bufferSize);

    virtual void testBufferUpload(UploadSampleResult<SampleType> &result, int writeSize) = 0;
    void logAndSetTestResult(const std::vector<UploadSampleResult<SampleType>> &results);

    uint32_t m_unusedBufferID;

protected:
    const CaseType m_caseType;
    const ResultType m_resultType;
    const uint32_t m_bufferUsage;
    const bool m_logBufferInfo;
    const bool m_bufferUnspecifiedContent;
    std::vector<uint8_t> m_zeroData;

    using BasicBufferCase<SampleType>::m_testCtx;
    using BasicBufferCase<SampleType>::m_context;

    using BasicBufferCase<SampleType>::UNUSED_RENDER_AREA_SIZE;
    using BasicBufferCase<SampleType>::m_minimalProgram;
    using BasicBufferCase<SampleType>::m_minimalProgramPosLoc;
    using BasicBufferCase<SampleType>::m_bufferID;
    using BasicBufferCase<SampleType>::m_numSamples;
    using BasicBufferCase<SampleType>::m_bufferSizeMin;
    using BasicBufferCase<SampleType>::m_bufferSizeMax;
    using BasicBufferCase<SampleType>::m_allocateLargerBuffer;
};

template <typename SampleType>
BasicUploadCase<SampleType>::BasicUploadCase(Context &context, const char *name, const char *desc, int bufferSizeMin,
                                             int bufferSizeMax, int numSamples, uint32_t bufferUsage, CaseType caseType,
                                             ResultType resultType, int flags)
    : BasicBufferCase<SampleType>(
          context, name, desc, bufferSizeMin, bufferSizeMax, numSamples,
          (caseType == CASE_USED_LARGER_BUFFER) ? (BasicBufferCase<SampleType>::FLAG_ALLOCATE_LARGER_BUFFER) : (0))
    , m_unusedBufferID(0)
    , m_caseType(caseType)
    , m_resultType(resultType)
    , m_bufferUsage(bufferUsage)
    , m_logBufferInfo((flags & FLAG_DONT_LOG_BUFFER_INFO) == 0)
    , m_bufferUnspecifiedContent((flags & FLAG_RESULT_BUFFER_UNSPECIFIED_CONTENT) != 0)
    , m_zeroData()
{
    DE_ASSERT(m_caseType < CASE_LAST);
}

template <typename SampleType>
BasicUploadCase<SampleType>::~BasicUploadCase(void)
{
    deinit();
}

template <typename SampleType>
void BasicUploadCase<SampleType>::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    BasicBufferCase<SampleType>::init();

    // zero buffer as upload source
    m_zeroData.resize(m_bufferSizeMax, 0x00);

    // unused buffer

    gl.genBuffers(1, &m_unusedBufferID);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Gen buf");

    // log basic info

    m_testCtx.getLog() << tcu::TestLog::Message << "Testing performance with " << m_numSamples
                       << " test samples. Sample order is randomized. All samples at even positions (first = 0) are "
                          "tested before samples at odd positions.\n"
                       << "Buffer sizes are in range [" << getHumanReadableByteSize(m_bufferSizeMin) << ", "
                       << getHumanReadableByteSize(m_bufferSizeMax) << "]." << tcu::TestLog::EndMessage;

    if (m_logBufferInfo)
    {
        switch (m_caseType)
        {
        case CASE_NO_BUFFERS:
            break;

        case CASE_NEW_BUFFER:
            m_testCtx.getLog() << tcu::TestLog::Message
                               << "Target buffer is generated but not specified (i.e glBufferData() not called)."
                               << tcu::TestLog::EndMessage;
            break;

        case CASE_UNSPECIFIED_BUFFER:
            m_testCtx.getLog() << tcu::TestLog::Message << "Target buffer is allocated with glBufferData(NULL)."
                               << tcu::TestLog::EndMessage;
            break;

        case CASE_SPECIFIED_BUFFER:
            m_testCtx.getLog() << tcu::TestLog::Message
                               << "Target buffer contents are specified prior testing with glBufferData(data)."
                               << tcu::TestLog::EndMessage;
            break;

        case CASE_USED_BUFFER:
            m_testCtx.getLog() << tcu::TestLog::Message << "Target buffer has been used in drawing before testing."
                               << tcu::TestLog::EndMessage;
            break;

        case CASE_USED_LARGER_BUFFER:
            m_testCtx.getLog() << tcu::TestLog::Message
                               << "Target buffer is larger and has been used in drawing before testing."
                               << tcu::TestLog::EndMessage;
            break;

        default:
            DE_ASSERT(false);
            break;
        }
    }

    if (m_resultType == RESULT_MEDIAN_TRANSFER_RATE)
        m_testCtx.getLog() << tcu::TestLog::Message << "Test result is the median transfer rate of the test samples."
                           << tcu::TestLog::EndMessage;
    else if (m_resultType == RESULT_ASYMPTOTIC_TRANSFER_RATE)
        m_testCtx.getLog() << tcu::TestLog::Message
                           << "Test result is the asymptotic transfer rate as the buffer size approaches infinity."
                           << tcu::TestLog::EndMessage;
    else
        DE_ASSERT(false);
}

template <typename SampleType>
void BasicUploadCase<SampleType>::deinit(void)
{
    if (m_unusedBufferID)
    {
        m_context.getRenderContext().getFunctions().deleteBuffers(1, &m_unusedBufferID);
        m_unusedBufferID = 0;
    }

    m_zeroData = std::vector<uint8_t>();

    BasicBufferCase<SampleType>::deinit();
}

template <typename SampleType>
bool BasicUploadCase<SampleType>::runSample(int iteration, UploadSampleResult<SampleType> &sample)
{
    const glw::Functions &gl      = m_context.getRenderContext().getFunctions();
    const int allocatedBufferSize = sample.allocatedSize;
    const int bufferSize          = sample.bufferSize;

    if (m_caseType != CASE_NO_BUFFERS)
        createBuffer(iteration, allocatedBufferSize);

    // warmup CPU before the test to make sure the power management governor
    // keeps us in the "high performance" mode
    {
        deYield();
        tcu::warmupCPU();
        deYield();
    }

    testBufferUpload(sample, bufferSize);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Buffer upload sample");

    if (m_caseType != CASE_NO_BUFFERS)
        deleteBuffer(bufferSize);

    return true;
}

template <typename SampleType>
void BasicUploadCase<SampleType>::createBuffer(int iteration, int bufferSize)
{
    DE_ASSERT(!m_bufferID);
    DE_ASSERT(m_caseType != CASE_NO_BUFFERS);

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    // create buffer

    if (m_caseType == CASE_NO_BUFFERS)
        return;

    // create empty buffer

    gl.genBuffers(1, &m_bufferID);
    gl.bindBuffer(GL_ARRAY_BUFFER, m_bufferID);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Buffer gen");

    if (m_caseType == CASE_NEW_BUFFER)
    {
        // upload something else first, this should reduce noise in samples

        de::Random rng(0xbadc * iteration);
        const int sizeDelta = rng.getInt(0, 2097140);
        const int unusedUploadSize =
            deAlign32(1048576 + sizeDelta, 4 * 4); // Vary buffer size to make sure it is always reallocated
        const std::vector<uint8_t> unusedData(unusedUploadSize, 0x20);

        gl.bindBuffer(GL_ARRAY_BUFFER, m_unusedBufferID);
        gl.bufferData(GL_ARRAY_BUFFER, unusedUploadSize, &unusedData[0], m_bufferUsage);

        // make sure upload won't interfere with the test
        useBuffer(unusedUploadSize);

        // don't kill the buffer so that the following upload cannot potentially reuse the buffer

        return;
    }

    // specify it

    if (m_caseType == CASE_UNSPECIFIED_BUFFER)
        gl.bufferData(GL_ARRAY_BUFFER, bufferSize, nullptr, m_bufferUsage);
    else
    {
        const std::vector<uint8_t> unusedData(bufferSize, 0x20);
        gl.bufferData(GL_ARRAY_BUFFER, bufferSize, &unusedData[0], m_bufferUsage);
    }

    if (m_caseType == CASE_UNSPECIFIED_BUFFER || m_caseType == CASE_SPECIFIED_BUFFER)
        return;

    // use it and make sure it is uploaded

    useBuffer(bufferSize);
    DE_ASSERT(m_caseType == CASE_USED_BUFFER || m_caseType == CASE_USED_LARGER_BUFFER);
}

template <typename SampleType>
void BasicUploadCase<SampleType>::deleteBuffer(int bufferSize)
{
    DE_ASSERT(m_bufferID);
    DE_ASSERT(m_caseType != CASE_NO_BUFFERS);

    // render from the buffer to make sure it actually made it to the gpu. This is to
    // make sure that if the upload actually happens later or is happening right now in
    // the background, it will not interfere with further test runs

    // if buffer contains unspecified content, sourcing data from it results in undefined
    // results, possibly including program termination. Specify all data to prevent such
    // case from happening

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.bindBuffer(GL_ARRAY_BUFFER, m_bufferID);

    if (m_bufferUnspecifiedContent)
    {
        const std::vector<uint8_t> unusedData(bufferSize, 0x20);
        gl.bufferData(GL_ARRAY_BUFFER, bufferSize, &unusedData[0], m_bufferUsage);

        GLU_EXPECT_NO_ERROR(gl.getError(), "re-specify buffer");
    }

    useBuffer(bufferSize);

    gl.deleteBuffers(1, &m_bufferID);
    m_bufferID = 0;
}

template <typename SampleType>
void BasicUploadCase<SampleType>::useBuffer(int bufferSize)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.useProgram(m_minimalProgram->getProgram());

    gl.viewport(0, 0, UNUSED_RENDER_AREA_SIZE, UNUSED_RENDER_AREA_SIZE);
    gl.vertexAttribPointer(m_minimalProgramPosLoc, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    gl.enableVertexAttribArray(m_minimalProgramPosLoc);

    // use whole buffer to make sure buffer is uploaded by drawing first and last
    DE_ASSERT(bufferSize % (int)sizeof(float[4]) == 0);
    gl.drawArrays(GL_POINTS, 0, 1);
    gl.drawArrays(GL_POINTS, bufferSize / (int)sizeof(float[4]) - 1, 1);

    BasicBufferCase<SampleType>::waitGLResults();
}

template <typename SampleType>
void BasicUploadCase<SampleType>::logAndSetTestResult(const std::vector<UploadSampleResult<SampleType>> &results)
{
    const UploadSampleAnalyzeResult analysis = analyzeSampleResults(m_testCtx.getLog(), results, true);

    // with small buffers, report the median transfer rate of the samples
    // with large buffers, report the expected preformance of infinitely large buffers
    const float rate = (m_resultType == RESULT_ASYMPTOTIC_TRANSFER_RATE) ? (analysis.transferRateAtInfinity) :
                                                                           (analysis.transferRateMedian);

    if (rate == std::numeric_limits<float>::infinity())
    {
        // sample times are 1) invalid or 2) timer resolution too low
        // report speed 0 bytes / s since real value cannot be determined
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::floatToString(0.0f, 2).c_str());
    }
    else
    {
        // report transfer rate in MB / s
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::floatToString(rate / 1024.0f / 1024.0f, 2).c_str());
    }
}

class ReferenceMemcpyCase : public BasicUploadCase<SingleOperationDuration>
{
public:
    ReferenceMemcpyCase(Context &ctx, const char *name, const char *desc, int minBufferSize, int maxBufferSize,
                        int numSamples, bool largeBuffersCase);
    ~ReferenceMemcpyCase(void);

    void init(void);
    void deinit(void);

private:
    void testBufferUpload(UploadSampleResult<SingleOperationDuration> &result, int bufferSize);

    std::vector<uint8_t> m_dstBuf;
};

ReferenceMemcpyCase::ReferenceMemcpyCase(Context &ctx, const char *name, const char *desc, int minBufferSize,
                                         int maxBufferSize, int numSamples, bool largeBuffersCase)
    : BasicUploadCase<SingleOperationDuration>(
          ctx, name, desc, minBufferSize, maxBufferSize, numSamples, 0, CASE_NO_BUFFERS,
          (largeBuffersCase) ? (RESULT_ASYMPTOTIC_TRANSFER_RATE) : (RESULT_MEDIAN_TRANSFER_RATE))
    , m_dstBuf()
{
    disableGLWarmup();
}

ReferenceMemcpyCase::~ReferenceMemcpyCase(void)
{
}

void ReferenceMemcpyCase::init(void)
{
    // Describe what the test tries to do
    m_testCtx.getLog() << tcu::TestLog::Message << "Testing performance of memcpy()." << tcu::TestLog::EndMessage;

    m_dstBuf.resize(m_bufferSizeMax, 0x00);

    BasicUploadCase<SingleOperationDuration>::init();
}

void ReferenceMemcpyCase::deinit(void)
{
    m_dstBuf = std::vector<uint8_t>();
    BasicUploadCase<SingleOperationDuration>::deinit();
}

void ReferenceMemcpyCase::testBufferUpload(UploadSampleResult<SingleOperationDuration> &result, int bufferSize)
{
    // write
    result.duration.totalDuration       = medianTimeMemcpy(&m_dstBuf[0], &m_zeroData[0], bufferSize);
    result.duration.fitResponseDuration = result.duration.totalDuration;

    result.writtenSize = bufferSize;
}

class BufferDataUploadCase : public BasicUploadCase<SingleOperationDuration>
{
public:
    BufferDataUploadCase(Context &ctx, const char *name, const char *desc, int minBufferSize, int maxBufferSize,
                         int numSamples, uint32_t bufferUsage, CaseType caseType);
    ~BufferDataUploadCase(void);

    void init(void);

private:
    void testBufferUpload(UploadSampleResult<SingleOperationDuration> &result, int bufferSize);
};

BufferDataUploadCase::BufferDataUploadCase(Context &ctx, const char *name, const char *desc, int minBufferSize,
                                           int maxBufferSize, int numSamples, uint32_t bufferUsage, CaseType caseType)
    : BasicUploadCase<SingleOperationDuration>(ctx, name, desc, minBufferSize, maxBufferSize, numSamples, bufferUsage,
                                               caseType, RESULT_MEDIAN_TRANSFER_RATE)
{
}

BufferDataUploadCase::~BufferDataUploadCase(void)
{
}

void BufferDataUploadCase::init(void)
{
    // Describe what the test tries to do
    m_testCtx.getLog() << tcu::TestLog::Message << "Testing glBufferData() function." << tcu::TestLog::EndMessage;

    BasicUploadCase<SingleOperationDuration>::init();
}

void BufferDataUploadCase::testBufferUpload(UploadSampleResult<SingleOperationDuration> &result, int bufferSize)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.bindBuffer(GL_ARRAY_BUFFER, m_bufferID);

    // upload
    {
        uint64_t startTime;
        uint64_t endTime;

        startTime = deGetMicroseconds();
        gl.bufferData(GL_ARRAY_BUFFER, bufferSize, &m_zeroData[0], m_bufferUsage);
        endTime = deGetMicroseconds();

        result.duration.totalDuration       = endTime - startTime;
        result.duration.fitResponseDuration = result.duration.totalDuration;
        result.writtenSize                  = bufferSize;
    }
}

class BufferSubDataUploadCase : public BasicUploadCase<SingleOperationDuration>
{
public:
    enum Flags
    {
        FLAG_FULL_UPLOAD           = 0x01,
        FLAG_PARTIAL_UPLOAD        = 0x02,
        FLAG_INVALIDATE_BEFORE_USE = 0x04,
    };

    BufferSubDataUploadCase(Context &ctx, const char *name, const char *desc, int minBufferSize, int maxBufferSize,
                            int numSamples, uint32_t bufferUsage, CaseType parentCase, int flags);
    ~BufferSubDataUploadCase(void);

    void init(void);

private:
    void testBufferUpload(UploadSampleResult<SingleOperationDuration> &result, int bufferSize);

    const bool m_fullUpload;
    const bool m_invalidateBeforeUse;
};

BufferSubDataUploadCase::BufferSubDataUploadCase(Context &ctx, const char *name, const char *desc, int minBufferSize,
                                                 int maxBufferSize, int numSamples, uint32_t bufferUsage,
                                                 CaseType parentCase, int flags)
    : BasicUploadCase<SingleOperationDuration>(ctx, name, desc, minBufferSize, maxBufferSize, numSamples, bufferUsage,
                                               parentCase, RESULT_MEDIAN_TRANSFER_RATE)
    , m_fullUpload((flags & FLAG_FULL_UPLOAD) != 0)
    , m_invalidateBeforeUse((flags & FLAG_INVALIDATE_BEFORE_USE) != 0)
{
    DE_ASSERT((flags & (FLAG_FULL_UPLOAD | FLAG_PARTIAL_UPLOAD)) != 0);
    DE_ASSERT((flags & (FLAG_FULL_UPLOAD | FLAG_PARTIAL_UPLOAD)) != (FLAG_FULL_UPLOAD | FLAG_PARTIAL_UPLOAD));
}

BufferSubDataUploadCase::~BufferSubDataUploadCase(void)
{
}

void BufferSubDataUploadCase::init(void)
{
    // Describe what the test tries to do
    m_testCtx.getLog() << tcu::TestLog::Message << "Testing glBufferSubData() function call performance. "
                       << ((m_fullUpload) ? ("The whole buffer is updated with glBufferSubData. ") :
                                            ("Half of the buffer data is updated with glBufferSubData. "))
                       << ((m_invalidateBeforeUse) ?
                               ("The buffer is cleared with glBufferData(..., NULL) before glBufferSubData upload.") :
                               (""))
                       << "\n"
                       << tcu::TestLog::EndMessage;

    BasicUploadCase<SingleOperationDuration>::init();
}

void BufferSubDataUploadCase::testBufferUpload(UploadSampleResult<SingleOperationDuration> &result, int bufferSize)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.bindBuffer(GL_ARRAY_BUFFER, m_bufferID);

    // "invalidate", upload null
    if (m_invalidateBeforeUse)
        gl.bufferData(GL_ARRAY_BUFFER, bufferSize, nullptr, m_bufferUsage);

    // upload
    {
        uint64_t startTime;
        uint64_t endTime;

        startTime = deGetMicroseconds();

        if (m_fullUpload)
            gl.bufferSubData(GL_ARRAY_BUFFER, 0, bufferSize, &m_zeroData[0]);
        else
        {
            // upload to buffer center
            gl.bufferSubData(GL_ARRAY_BUFFER, bufferSize / 4, bufferSize / 2, &m_zeroData[0]);
        }

        endTime = deGetMicroseconds();

        result.duration.totalDuration       = endTime - startTime;
        result.duration.fitResponseDuration = result.duration.totalDuration;

        if (m_fullUpload)
            result.writtenSize = bufferSize;
        else
            result.writtenSize = bufferSize / 2;
    }
}

class MapBufferRangeCase : public BasicUploadCase<MapBufferRangeDuration>
{
public:
    enum Flags
    {
        FLAG_PARTIAL                       = 0x01,
        FLAG_MANUAL_INVALIDATION           = 0x02,
        FLAG_USE_UNUSED_UNSPECIFIED_BUFFER = 0x04,
        FLAG_USE_UNUSED_SPECIFIED_BUFFER   = 0x08,
    };

    MapBufferRangeCase(Context &ctx, const char *name, const char *desc, int minBufferSize, int maxBufferSize,
                       int numSamples, uint32_t bufferUsage, uint32_t mapFlags, int caseFlags);
    ~MapBufferRangeCase(void);

    void init(void);

private:
    static CaseType getBaseCaseType(int caseFlags);
    static int getBaseFlags(uint32_t mapFlags, int caseFlags);

    void testBufferUpload(UploadSampleResult<MapBufferRangeDuration> &result, int bufferSize);
    void attemptBufferMap(UploadSampleResult<MapBufferRangeDuration> &result, int bufferSize);

    const bool m_manualInvalidation;
    const bool m_fullUpload;
    const bool m_useUnusedUnspecifiedBuffer;
    const bool m_useUnusedSpecifiedBuffer;
    const uint32_t m_mapFlags;
    int m_unmapFailures;
};

MapBufferRangeCase::MapBufferRangeCase(Context &ctx, const char *name, const char *desc, int minBufferSize,
                                       int maxBufferSize, int numSamples, uint32_t bufferUsage, uint32_t mapFlags,
                                       int caseFlags)
    : BasicUploadCase<MapBufferRangeDuration>(ctx, name, desc, minBufferSize, maxBufferSize, numSamples, bufferUsage,
                                              getBaseCaseType(caseFlags), RESULT_MEDIAN_TRANSFER_RATE,
                                              getBaseFlags(mapFlags, caseFlags))
    , m_manualInvalidation((caseFlags & FLAG_MANUAL_INVALIDATION) != 0)
    , m_fullUpload((caseFlags & FLAG_PARTIAL) == 0)
    , m_useUnusedUnspecifiedBuffer((caseFlags & FLAG_USE_UNUSED_UNSPECIFIED_BUFFER) != 0)
    , m_useUnusedSpecifiedBuffer((caseFlags & FLAG_USE_UNUSED_SPECIFIED_BUFFER) != 0)
    , m_mapFlags(mapFlags)
    , m_unmapFailures(0)
{
    DE_ASSERT(!(m_useUnusedUnspecifiedBuffer && m_useUnusedSpecifiedBuffer));
    DE_ASSERT(!((m_useUnusedUnspecifiedBuffer || m_useUnusedSpecifiedBuffer) && m_manualInvalidation));
}

MapBufferRangeCase::~MapBufferRangeCase(void)
{
}

void MapBufferRangeCase::init(void)
{
    // Describe what the test tries to do
    m_testCtx.getLog()
        << tcu::TestLog::Message << "Testing glMapBufferRange() and glUnmapBuffer() function call performance.\n"
        << ((m_fullUpload) ? ("The whole buffer is mapped.") : ("Half of the buffer is mapped.")) << "\n"
        << ((m_useUnusedUnspecifiedBuffer) ?
                ("The buffer has not been used before mapping and is allocated with unspecified contents.\n") :
                (""))
        << ((m_useUnusedSpecifiedBuffer) ?
                ("The buffer has not been used before mapping and is allocated with specified contents.\n") :
                (""))
        << ((!m_useUnusedSpecifiedBuffer && !m_useUnusedUnspecifiedBuffer) ?
                ("The buffer has previously been used in a drawing operation.\n") :
                (""))
        << ((m_manualInvalidation) ? ("The buffer is cleared with glBufferData(..., NULL) before mapping.\n") : (""))
        << "Map bits:\n"
        << ((m_mapFlags & GL_MAP_WRITE_BIT) ? ("\tGL_MAP_WRITE_BIT\n") : (""))
        << ((m_mapFlags & GL_MAP_READ_BIT) ? ("\tGL_MAP_READ_BIT\n") : (""))
        << ((m_mapFlags & GL_MAP_INVALIDATE_RANGE_BIT) ? ("\tGL_MAP_INVALIDATE_RANGE_BIT\n") : (""))
        << ((m_mapFlags & GL_MAP_INVALIDATE_BUFFER_BIT) ? ("\tGL_MAP_INVALIDATE_BUFFER_BIT\n") : (""))
        << ((m_mapFlags & GL_MAP_UNSYNCHRONIZED_BIT) ? ("\tGL_MAP_UNSYNCHRONIZED_BIT\n") : (""))
        << tcu::TestLog::EndMessage;

    BasicUploadCase<MapBufferRangeDuration>::init();
}

MapBufferRangeCase::CaseType MapBufferRangeCase::getBaseCaseType(int caseFlags)
{
    if ((caseFlags & FLAG_USE_UNUSED_UNSPECIFIED_BUFFER) == 0 && (caseFlags & FLAG_USE_UNUSED_SPECIFIED_BUFFER) == 0)
        return CASE_USED_BUFFER;
    else
        return CASE_NEW_BUFFER;
}

int MapBufferRangeCase::getBaseFlags(uint32_t mapFlags, int caseFlags)
{
    int flags = FLAG_DONT_LOG_BUFFER_INFO;

    // If buffer contains unspecified data when it is sourced (i.e drawn)
    // results are undefined, and system errors may occur. Signal parent
    // class to take this into account
    if (caseFlags & FLAG_PARTIAL)
    {
        if ((mapFlags & GL_MAP_INVALIDATE_BUFFER_BIT) != 0 || (caseFlags & FLAG_MANUAL_INVALIDATION) != 0 ||
            (caseFlags & FLAG_USE_UNUSED_UNSPECIFIED_BUFFER) != 0)
        {
            flags |= FLAG_RESULT_BUFFER_UNSPECIFIED_CONTENT;
        }
    }

    return flags;
}

void MapBufferRangeCase::testBufferUpload(UploadSampleResult<MapBufferRangeDuration> &result, int bufferSize)
{
    const int unmapFailureThreshold = 4;

    for (; m_unmapFailures < unmapFailureThreshold; ++m_unmapFailures)
    {
        try
        {
            attemptBufferMap(result, bufferSize);
            return;
        }
        catch (UnmapFailureError &)
        {
        }
    }

    throw tcu::TestError("Unmapping failures exceeded limit");
}

void MapBufferRangeCase::attemptBufferMap(UploadSampleResult<MapBufferRangeDuration> &result, int bufferSize)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.bindBuffer(GL_ARRAY_BUFFER, m_bufferID);

    if (m_fullUpload)
        result.writtenSize = bufferSize;
    else
        result.writtenSize = bufferSize / 2;

    // Create unused buffer

    if (m_manualInvalidation || m_useUnusedUnspecifiedBuffer)
    {
        uint64_t startTime;
        uint64_t endTime;

        // "invalidate" or allocate, upload null
        startTime = deGetMicroseconds();
        gl.bufferData(GL_ARRAY_BUFFER, bufferSize, nullptr, m_bufferUsage);
        endTime = deGetMicroseconds();

        result.duration.allocDuration = endTime - startTime;
    }
    else if (m_useUnusedSpecifiedBuffer)
    {
        uint64_t startTime;
        uint64_t endTime;

        // Specify buffer contents
        startTime = deGetMicroseconds();
        gl.bufferData(GL_ARRAY_BUFFER, bufferSize, &m_zeroData[0], m_bufferUsage);
        endTime = deGetMicroseconds();

        result.duration.allocDuration = endTime - startTime;
    }
    else
    {
        // No alloc, no time
        result.duration.allocDuration = 0;
    }

    // upload
    {
        void *mapPtr;

        // Map
        {
            uint64_t startTime;
            uint64_t endTime;

            startTime = deGetMicroseconds();
            if (m_fullUpload)
                mapPtr = gl.mapBufferRange(GL_ARRAY_BUFFER, 0, result.writtenSize, m_mapFlags);
            else
            {
                // upload to buffer center
                mapPtr = gl.mapBufferRange(GL_ARRAY_BUFFER, bufferSize / 4, result.writtenSize, m_mapFlags);
            }
            endTime = deGetMicroseconds();

            if (!mapPtr)
                throw tcu::Exception("MapBufferRange returned NULL");

            result.duration.mapDuration = endTime - startTime;
        }

        // Write
        {
            result.duration.writeDuration = medianTimeMemcpy(mapPtr, &m_zeroData[0], result.writtenSize);
        }

        // Unmap
        {
            uint64_t startTime;
            uint64_t endTime;
            glw::GLboolean unmapSuccessful;

            startTime       = deGetMicroseconds();
            unmapSuccessful = gl.unmapBuffer(GL_ARRAY_BUFFER);
            endTime         = deGetMicroseconds();

            // if unmapping fails, just try again later
            if (!unmapSuccessful)
                throw UnmapFailureError();

            result.duration.unmapDuration = endTime - startTime;
        }

        result.duration.totalDuration = result.duration.mapDuration + result.duration.writeDuration +
                                        result.duration.unmapDuration + result.duration.allocDuration;
        result.duration.fitResponseDuration = result.duration.totalDuration;
    }
}

class MapBufferRangeFlushCase : public BasicUploadCase<MapBufferRangeFlushDuration>
{
public:
    enum Flags
    {
        FLAG_PARTIAL                       = 0x01,
        FLAG_FLUSH_IN_PARTS                = 0x02,
        FLAG_USE_UNUSED_UNSPECIFIED_BUFFER = 0x04,
        FLAG_USE_UNUSED_SPECIFIED_BUFFER   = 0x08,
        FLAG_FLUSH_PARTIAL                 = 0x10,
    };

    MapBufferRangeFlushCase(Context &ctx, const char *name, const char *desc, int minBufferSize, int maxBufferSize,
                            int numSamples, uint32_t bufferUsage, uint32_t mapFlags, int caseFlags);
    ~MapBufferRangeFlushCase(void);

    void init(void);

private:
    static CaseType getBaseCaseType(int caseFlags);
    static int getBaseFlags(uint32_t mapFlags, int caseFlags);

    void testBufferUpload(UploadSampleResult<MapBufferRangeFlushDuration> &result, int bufferSize);
    void attemptBufferMap(UploadSampleResult<MapBufferRangeFlushDuration> &result, int bufferSize);

    const bool m_fullUpload;
    const bool m_flushInParts;
    const bool m_flushPartial;
    const bool m_useUnusedUnspecifiedBuffer;
    const bool m_useUnusedSpecifiedBuffer;
    const uint32_t m_mapFlags;
    int m_unmapFailures;
};

MapBufferRangeFlushCase::MapBufferRangeFlushCase(Context &ctx, const char *name, const char *desc, int minBufferSize,
                                                 int maxBufferSize, int numSamples, uint32_t bufferUsage,
                                                 uint32_t mapFlags, int caseFlags)
    : BasicUploadCase<MapBufferRangeFlushDuration>(ctx, name, desc, minBufferSize, maxBufferSize, numSamples,
                                                   bufferUsage, getBaseCaseType(caseFlags), RESULT_MEDIAN_TRANSFER_RATE,
                                                   getBaseFlags(mapFlags, caseFlags))
    , m_fullUpload((caseFlags & FLAG_PARTIAL) == 0)
    , m_flushInParts((caseFlags & FLAG_FLUSH_IN_PARTS) != 0)
    , m_flushPartial((caseFlags & FLAG_FLUSH_PARTIAL) != 0)
    , m_useUnusedUnspecifiedBuffer((caseFlags & FLAG_USE_UNUSED_UNSPECIFIED_BUFFER) != 0)
    , m_useUnusedSpecifiedBuffer((caseFlags & FLAG_USE_UNUSED_SPECIFIED_BUFFER) != 0)
    , m_mapFlags(mapFlags)
    , m_unmapFailures(0)
{
    DE_ASSERT(!(m_flushPartial && m_flushInParts));
    DE_ASSERT(!(m_flushPartial && !m_fullUpload));
}

MapBufferRangeFlushCase::~MapBufferRangeFlushCase(void)
{
}

void MapBufferRangeFlushCase::init(void)
{
    // Describe what the test tries to do
    m_testCtx.getLog()
        << tcu::TestLog::Message
        << "Testing glMapBufferRange(), glFlushMappedBufferRange() and glUnmapBuffer() function call performance.\n"
        << ((m_fullUpload) ? ("The whole buffer is mapped.") : ("Half of the buffer is mapped.")) << "\n"
        << ((m_flushInParts) ?
                ("The mapped range is partitioned to 4 subranges and each partition is flushed separately.") :
            (m_flushPartial) ? ("Half of the buffer range is flushed.") :
                               ("The whole mapped range is flushed in one flush call."))
        << "\n"
        << ((m_useUnusedUnspecifiedBuffer) ?
                ("The buffer has not been used before mapping and is allocated with unspecified contents.\n") :
                (""))
        << ((m_useUnusedSpecifiedBuffer) ?
                ("The buffer has not been used before mapping and is allocated with specified contents.\n") :
                (""))
        << ((!m_useUnusedSpecifiedBuffer && !m_useUnusedUnspecifiedBuffer) ?
                ("The buffer has previously been used in a drawing operation.\n") :
                (""))
        << "Map bits:\n"
        << ((m_mapFlags & GL_MAP_WRITE_BIT) ? ("\tGL_MAP_WRITE_BIT\n") : (""))
        << ((m_mapFlags & GL_MAP_READ_BIT) ? ("\tGL_MAP_READ_BIT\n") : (""))
        << ((m_mapFlags & GL_MAP_INVALIDATE_RANGE_BIT) ? ("\tGL_MAP_INVALIDATE_RANGE_BIT\n") : (""))
        << ((m_mapFlags & GL_MAP_INVALIDATE_BUFFER_BIT) ? ("\tGL_MAP_INVALIDATE_BUFFER_BIT\n") : (""))
        << ((m_mapFlags & GL_MAP_UNSYNCHRONIZED_BIT) ? ("\tGL_MAP_UNSYNCHRONIZED_BIT\n") : (""))
        << ((m_mapFlags & GL_MAP_FLUSH_EXPLICIT_BIT) ? ("\tGL_MAP_FLUSH_EXPLICIT_BIT\n") : (""))
        << tcu::TestLog::EndMessage;

    BasicUploadCase<MapBufferRangeFlushDuration>::init();
}

MapBufferRangeFlushCase::CaseType MapBufferRangeFlushCase::getBaseCaseType(int caseFlags)
{
    if ((caseFlags & FLAG_USE_UNUSED_UNSPECIFIED_BUFFER) == 0 && (caseFlags & FLAG_USE_UNUSED_SPECIFIED_BUFFER) == 0)
        return CASE_USED_BUFFER;
    else
        return CASE_NEW_BUFFER;
}

int MapBufferRangeFlushCase::getBaseFlags(uint32_t mapFlags, int caseFlags)
{
    int flags = FLAG_DONT_LOG_BUFFER_INFO;

    // If buffer contains unspecified data when it is sourced (i.e drawn)
    // results are undefined, and system errors may occur. Signal parent
    // class to take this into account
    if (caseFlags & FLAG_PARTIAL)
    {
        if ((mapFlags & GL_MAP_INVALIDATE_BUFFER_BIT) != 0 || (caseFlags & FLAG_USE_UNUSED_UNSPECIFIED_BUFFER) != 0 ||
            (caseFlags & FLAG_FLUSH_PARTIAL) != 0)
        {
            flags |= FLAG_RESULT_BUFFER_UNSPECIFIED_CONTENT;
        }
    }

    return flags;
}

void MapBufferRangeFlushCase::testBufferUpload(UploadSampleResult<MapBufferRangeFlushDuration> &result, int bufferSize)
{
    const int unmapFailureThreshold = 4;

    for (; m_unmapFailures < unmapFailureThreshold; ++m_unmapFailures)
    {
        try
        {
            attemptBufferMap(result, bufferSize);
            return;
        }
        catch (UnmapFailureError &)
        {
        }
    }

    throw tcu::TestError("Unmapping failures exceeded limit");
}

void MapBufferRangeFlushCase::attemptBufferMap(UploadSampleResult<MapBufferRangeFlushDuration> &result, int bufferSize)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const int mappedSize     = (m_fullUpload) ? (bufferSize) : (bufferSize / 2);

    if (m_fullUpload && !m_flushPartial)
        result.writtenSize = bufferSize;
    else
        result.writtenSize = bufferSize / 2;

    gl.bindBuffer(GL_ARRAY_BUFFER, m_bufferID);

    // Create unused buffer

    if (m_useUnusedUnspecifiedBuffer)
    {
        uint64_t startTime;
        uint64_t endTime;

        // Don't specify contents
        startTime = deGetMicroseconds();
        gl.bufferData(GL_ARRAY_BUFFER, bufferSize, nullptr, m_bufferUsage);
        endTime = deGetMicroseconds();

        result.duration.allocDuration = endTime - startTime;
    }
    else if (m_useUnusedSpecifiedBuffer)
    {
        uint64_t startTime;
        uint64_t endTime;

        // Specify buffer contents
        startTime = deGetMicroseconds();
        gl.bufferData(GL_ARRAY_BUFFER, bufferSize, &m_zeroData[0], m_bufferUsage);
        endTime = deGetMicroseconds();

        result.duration.allocDuration = endTime - startTime;
    }
    else
    {
        // No alloc, no time
        result.duration.allocDuration = 0;
    }

    // upload
    {
        void *mapPtr;

        // Map
        {
            uint64_t startTime;
            uint64_t endTime;

            startTime = deGetMicroseconds();
            if (m_fullUpload)
                mapPtr = gl.mapBufferRange(GL_ARRAY_BUFFER, 0, mappedSize, m_mapFlags);
            else
            {
                // upload to buffer center
                mapPtr = gl.mapBufferRange(GL_ARRAY_BUFFER, bufferSize / 4, mappedSize, m_mapFlags);
            }
            endTime = deGetMicroseconds();

            if (!mapPtr)
                throw tcu::Exception("MapBufferRange returned NULL");

            result.duration.mapDuration = endTime - startTime;
        }

        // Write
        {
            if (!m_flushPartial)
                result.duration.writeDuration = medianTimeMemcpy(mapPtr, &m_zeroData[0], result.writtenSize);
            else
                result.duration.writeDuration =
                    medianTimeMemcpy((uint8_t *)mapPtr + bufferSize / 4, &m_zeroData[0], result.writtenSize);
        }

        // Flush
        {
            uint64_t startTime;
            uint64_t endTime;

            startTime = deGetMicroseconds();

            if (m_flushPartial)
                gl.flushMappedBufferRange(GL_ARRAY_BUFFER, mappedSize / 4, mappedSize / 2);
            else if (!m_flushInParts)
                gl.flushMappedBufferRange(GL_ARRAY_BUFFER, 0, mappedSize);
            else
            {
                const int p1 = 0;
                const int p2 = mappedSize / 3;
                const int p3 = mappedSize / 2;
                const int p4 = mappedSize * 2 / 4;
                const int p5 = mappedSize;

                // flush in mixed order
                gl.flushMappedBufferRange(GL_ARRAY_BUFFER, p2, p3 - p2);
                gl.flushMappedBufferRange(GL_ARRAY_BUFFER, p1, p2 - p1);
                gl.flushMappedBufferRange(GL_ARRAY_BUFFER, p4, p5 - p4);
                gl.flushMappedBufferRange(GL_ARRAY_BUFFER, p3, p4 - p3);
            }

            endTime = deGetMicroseconds();

            result.duration.flushDuration = endTime - startTime;
        }

        // Unmap
        {
            uint64_t startTime;
            uint64_t endTime;
            glw::GLboolean unmapSuccessful;

            startTime       = deGetMicroseconds();
            unmapSuccessful = gl.unmapBuffer(GL_ARRAY_BUFFER);
            endTime         = deGetMicroseconds();

            // if unmapping fails, just try again later
            if (!unmapSuccessful)
                throw UnmapFailureError();

            result.duration.unmapDuration = endTime - startTime;
        }

        result.duration.totalDuration = result.duration.mapDuration + result.duration.writeDuration +
                                        result.duration.flushDuration + result.duration.unmapDuration +
                                        result.duration.allocDuration;
        result.duration.fitResponseDuration = result.duration.totalDuration;
    }
}

template <typename SampleType>
class ModifyAfterBasicCase : public BasicBufferCase<SampleType>
{
public:
    ModifyAfterBasicCase(Context &context, const char *name, const char *description, int bufferSizeMin,
                         int bufferSizeMax, uint32_t usage, bool bufferUnspecifiedAfterTest);
    ~ModifyAfterBasicCase(void);

    void init(void);
    void deinit(void);

protected:
    void drawBufferRange(int begin, int end);

private:
    enum
    {
        NUM_SAMPLES = 20,
    };

    bool runSample(int iteration, UploadSampleResult<SampleType> &sample);
    bool prepareAndRunTest(int iteration, UploadSampleResult<SampleType> &result, int bufferSize);
    void logAndSetTestResult(const std::vector<UploadSampleResult<SampleType>> &results);

    virtual void testWithBufferSize(UploadSampleResult<SampleType> &result, int bufferSize) = 0;

    int m_unmappingErrors;

protected:
    const bool m_bufferUnspecifiedAfterTest;
    const uint32_t m_bufferUsage;
    std::vector<uint8_t> m_zeroData;

    using BasicBufferCase<SampleType>::m_testCtx;
    using BasicBufferCase<SampleType>::m_context;

    using BasicBufferCase<SampleType>::UNUSED_RENDER_AREA_SIZE;
    using BasicBufferCase<SampleType>::m_minimalProgram;
    using BasicBufferCase<SampleType>::m_minimalProgramPosLoc;
    using BasicBufferCase<SampleType>::m_bufferID;
    using BasicBufferCase<SampleType>::m_numSamples;
    using BasicBufferCase<SampleType>::m_bufferSizeMin;
    using BasicBufferCase<SampleType>::m_bufferSizeMax;
    using BasicBufferCase<SampleType>::m_allocateLargerBuffer;
};

template <typename SampleType>
ModifyAfterBasicCase<SampleType>::ModifyAfterBasicCase(Context &context, const char *name, const char *description,
                                                       int bufferSizeMin, int bufferSizeMax, uint32_t usage,
                                                       bool bufferUnspecifiedAfterTest)
    : BasicBufferCase<SampleType>(context, name, description, bufferSizeMin, bufferSizeMax, NUM_SAMPLES, 0)
    , m_unmappingErrors(0)
    , m_bufferUnspecifiedAfterTest(bufferUnspecifiedAfterTest)
    , m_bufferUsage(usage)
    , m_zeroData()
{
}

template <typename SampleType>
ModifyAfterBasicCase<SampleType>::~ModifyAfterBasicCase(void)
{
    BasicBufferCase<SampleType>::deinit();
}

template <typename SampleType>
void ModifyAfterBasicCase<SampleType>::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    // init parent

    BasicBufferCase<SampleType>::init();

    // upload source
    m_zeroData.resize(m_bufferSizeMax, 0x00);

    // log basic info

    m_testCtx.getLog() << tcu::TestLog::Message << "Testing performance with " << (int)NUM_SAMPLES
                       << " test samples. Sample order is randomized. All samples at even positions (first = 0) are "
                          "tested before samples at odd positions.\n"
                       << "Buffer sizes are in range [" << getHumanReadableByteSize(m_bufferSizeMin) << ", "
                       << getHumanReadableByteSize(m_bufferSizeMax) << "]." << tcu::TestLog::EndMessage;

    // log which transfer rate is the test result and buffer info

    m_testCtx.getLog() << tcu::TestLog::Message << "Test result is the median transfer rate of the test samples.\n"
                       << "Buffer usage = " << glu::getUsageName(m_bufferUsage) << tcu::TestLog::EndMessage;

    // Set state for drawing so that we don't have to change these during the iteration
    {
        gl.useProgram(m_minimalProgram->getProgram());
        gl.viewport(0, 0, UNUSED_RENDER_AREA_SIZE, UNUSED_RENDER_AREA_SIZE);
        gl.enableVertexAttribArray(m_minimalProgramPosLoc);
    }
}

template <typename SampleType>
void ModifyAfterBasicCase<SampleType>::deinit(void)
{
    m_zeroData = std::vector<uint8_t>();

    BasicBufferCase<SampleType>::deinit();
}

template <typename SampleType>
void ModifyAfterBasicCase<SampleType>::drawBufferRange(int begin, int end)
{
    DE_ASSERT(begin % (int)sizeof(float[4]) == 0);
    DE_ASSERT(end % (int)sizeof(float[4]) == 0);

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    // use given range
    gl.drawArrays(GL_POINTS, begin / (int)sizeof(float[4]), 1);
    gl.drawArrays(GL_POINTS, end / (int)sizeof(float[4]) - 1, 1);
}

template <typename SampleType>
bool ModifyAfterBasicCase<SampleType>::runSample(int iteration, UploadSampleResult<SampleType> &sample)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const int bufferSize     = sample.bufferSize;
    bool testOk;

    testOk = prepareAndRunTest(iteration, sample, bufferSize);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Buffer upload sample");

    if (!testOk)
    {
        const int unmapFailureThreshold = 4;

        // only unmapping error can cause iteration failure
        if (++m_unmappingErrors >= unmapFailureThreshold)
            throw tcu::TestError("Too many unmapping errors, cannot continue.");

        // just try again
        return false;
    }

    return true;
}

template <typename SampleType>
bool ModifyAfterBasicCase<SampleType>::prepareAndRunTest(int iteration, UploadSampleResult<SampleType> &result,
                                                         int bufferSize)
{
    DE_UNREF(iteration);

    DE_ASSERT(!m_bufferID);
    DE_ASSERT(deIsAligned32(bufferSize, 4 * 4)); // aligned to vec4

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool testRunOk           = true;
    bool unmappingFailed     = false;

    // Upload initial buffer to the GPU...
    gl.genBuffers(1, &m_bufferID);
    gl.bindBuffer(GL_ARRAY_BUFFER, m_bufferID);
    gl.bufferData(GL_ARRAY_BUFFER, bufferSize, &m_zeroData[0], m_bufferUsage);

    // ...use it...
    gl.vertexAttribPointer(m_minimalProgramPosLoc, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    drawBufferRange(0, bufferSize);

    // ..and make sure it is uploaded
    BasicBufferCase<SampleType>::waitGLResults();

    // warmup CPU before the test to make sure the power management governor
    // keeps us in the "high performance" mode
    {
        deYield();
        tcu::warmupCPU();
        deYield();
    }

    // test
    try
    {
        // buffer is uploaded to the GPU. Draw from it.
        drawBufferRange(0, bufferSize);

        // and test upload
        testWithBufferSize(result, bufferSize);
    }
    catch (UnmapFailureError &)
    {
        testRunOk       = false;
        unmappingFailed = true;
    }

    // clean up: make sure buffer is not in upload queue and delete it

    // sourcing unspecified data causes undefined results, possibly program termination
    if (m_bufferUnspecifiedAfterTest || unmappingFailed)
        gl.bufferData(GL_ARRAY_BUFFER, bufferSize, &m_zeroData[0], m_bufferUsage);

    drawBufferRange(0, bufferSize);
    BasicBufferCase<SampleType>::waitGLResults();

    gl.deleteBuffers(1, &m_bufferID);
    m_bufferID = 0;

    return testRunOk;
}

template <typename SampleType>
void ModifyAfterBasicCase<SampleType>::logAndSetTestResult(const std::vector<UploadSampleResult<SampleType>> &results)
{
    const UploadSampleAnalyzeResult analysis = analyzeSampleResults(m_testCtx.getLog(), results, false);

    // Return median transfer rate of the samples

    if (analysis.transferRateMedian == std::numeric_limits<float>::infinity())
    {
        // sample times are 1) invalid or 2) timer resolution too low
        // report speed 0 bytes / s since real value cannot be determined
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::floatToString(0.0f, 2).c_str());
    }
    else
    {
        // report transfer rate in MB / s
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS,
                                de::floatToString(analysis.transferRateMedian / 1024.0f / 1024.0f, 2).c_str());
    }
}

class ModifyAfterWithBufferDataCase : public ModifyAfterBasicCase<SingleOperationDuration>
{
public:
    enum CaseFlags
    {
        FLAG_RESPECIFY_SIZE  = 0x1,
        FLAG_UPLOAD_REPEATED = 0x2,
    };

    ModifyAfterWithBufferDataCase(Context &context, const char *name, const char *desc, int bufferSizeMin,
                                  int bufferSizeMax, uint32_t usage, int flags);
    ~ModifyAfterWithBufferDataCase(void);

    void init(void);
    void deinit(void);

private:
    void testWithBufferSize(UploadSampleResult<SingleOperationDuration> &result, int bufferSize);

    enum
    {
        NUM_REPEATS = 2
    };

    const bool m_respecifySize;
    const bool m_repeatedUpload;
    const float m_sizeDifferenceFactor;
};

ModifyAfterWithBufferDataCase::ModifyAfterWithBufferDataCase(Context &context, const char *name, const char *desc,
                                                             int bufferSizeMin, int bufferSizeMax, uint32_t usage,
                                                             int flags)
    : ModifyAfterBasicCase<SingleOperationDuration>(context, name, desc, bufferSizeMin, bufferSizeMax, usage, false)
    , m_respecifySize((flags & FLAG_RESPECIFY_SIZE) != 0)
    , m_repeatedUpload((flags & FLAG_UPLOAD_REPEATED) != 0)
    , m_sizeDifferenceFactor(1.3f)
{
    DE_ASSERT(!(m_repeatedUpload && m_respecifySize));
}

ModifyAfterWithBufferDataCase::~ModifyAfterWithBufferDataCase(void)
{
    deinit();
}

void ModifyAfterWithBufferDataCase::init(void)
{
    // Log the purpose of the test

    if (m_repeatedUpload)
        m_testCtx.getLog() << tcu::TestLog::Message
                           << "Testing performance of BufferData() command after \"specify buffer contents - draw "
                              "buffer\" command pair is repeated "
                           << (int)NUM_REPEATS << " times." << tcu::TestLog::EndMessage;
    else
        m_testCtx.getLog() << tcu::TestLog::Message
                           << "Testing performance of BufferData() command after a draw command that sources data from "
                              "the target buffer."
                           << tcu::TestLog::EndMessage;

    m_testCtx.getLog() << tcu::TestLog::Message
                       << ((m_respecifySize) ?
                               ("Buffer size is increased and contents are modified with BufferData().\n") :
                               ("Buffer contents are modified with BufferData().\n"))
                       << tcu::TestLog::EndMessage;

    // init parent
    ModifyAfterBasicCase<SingleOperationDuration>::init();

    // make sure our zeroBuffer is large enough
    if (m_respecifySize)
    {
        const int largerBufferSize = deAlign32((int)((float)m_bufferSizeMax * m_sizeDifferenceFactor), 4 * 4);
        m_zeroData.resize(largerBufferSize, 0x00);
    }
}

void ModifyAfterWithBufferDataCase::deinit(void)
{
    ModifyAfterBasicCase<SingleOperationDuration>::deinit();
}

void ModifyAfterWithBufferDataCase::testWithBufferSize(UploadSampleResult<SingleOperationDuration> &result,
                                                       int bufferSize)
{
    // always draw the same amount to make compares between cases sensible
    const int drawStart = deAlign32(bufferSize / 4, 4 * 4);
    const int drawEnd   = deAlign32(bufferSize * 3 / 4, 4 * 4);

    const glw::Functions &gl   = m_context.getRenderContext().getFunctions();
    const int largerBufferSize = deAlign32((int)((float)bufferSize * m_sizeDifferenceFactor), 4 * 4);
    const int newBufferSize    = (m_respecifySize) ? (largerBufferSize) : (bufferSize);
    uint64_t startTime;
    uint64_t endTime;

    // repeat upload-draw
    if (m_repeatedUpload)
    {
        for (int repeatNdx = 0; repeatNdx < NUM_REPEATS; ++repeatNdx)
        {
            gl.bufferData(GL_ARRAY_BUFFER, newBufferSize, &m_zeroData[0], m_bufferUsage);
            drawBufferRange(drawStart, drawEnd);
        }
    }

    // test upload
    startTime = deGetMicroseconds();
    gl.bufferData(GL_ARRAY_BUFFER, newBufferSize, &m_zeroData[0], m_bufferUsage);
    endTime = deGetMicroseconds();

    result.duration.totalDuration       = endTime - startTime;
    result.duration.fitResponseDuration = result.duration.totalDuration;
    result.writtenSize                  = newBufferSize;
}

class ModifyAfterWithBufferSubDataCase : public ModifyAfterBasicCase<SingleOperationDuration>
{
public:
    enum CaseFlags
    {
        FLAG_PARTIAL         = 0x1,
        FLAG_UPLOAD_REPEATED = 0x2,
    };

    ModifyAfterWithBufferSubDataCase(Context &context, const char *name, const char *desc, int bufferSizeMin,
                                     int bufferSizeMax, uint32_t usage, int flags);
    ~ModifyAfterWithBufferSubDataCase(void);

    void init(void);
    void deinit(void);

private:
    void testWithBufferSize(UploadSampleResult<SingleOperationDuration> &result, int bufferSize);

    enum
    {
        NUM_REPEATS = 2
    };

    const bool m_partialUpload;
    const bool m_repeatedUpload;
};

ModifyAfterWithBufferSubDataCase::ModifyAfterWithBufferSubDataCase(Context &context, const char *name, const char *desc,
                                                                   int bufferSizeMin, int bufferSizeMax, uint32_t usage,
                                                                   int flags)
    : ModifyAfterBasicCase<SingleOperationDuration>(context, name, desc, bufferSizeMin, bufferSizeMax, usage, false)
    , m_partialUpload((flags & FLAG_PARTIAL) != 0)
    , m_repeatedUpload((flags & FLAG_UPLOAD_REPEATED) != 0)
{
}

ModifyAfterWithBufferSubDataCase::~ModifyAfterWithBufferSubDataCase(void)
{
    deinit();
}

void ModifyAfterWithBufferSubDataCase::init(void)
{
    // Log the purpose of the test

    if (m_repeatedUpload)
        m_testCtx.getLog() << tcu::TestLog::Message
                           << "Testing performance of BufferSubData() command after \"specify buffer contents - draw "
                              "buffer\" command pair is repeated "
                           << (int)NUM_REPEATS << " times." << tcu::TestLog::EndMessage;
    else
        m_testCtx.getLog() << tcu::TestLog::Message
                           << "Testing performance of BufferSubData() command after a draw command that sources data "
                              "from the target buffer."
                           << tcu::TestLog::EndMessage;

    m_testCtx.getLog() << tcu::TestLog::Message
                       << ((m_partialUpload) ? ("Half of the buffer contents are modified.\n") :
                                               ("Buffer contents are fully respecified.\n"))
                       << tcu::TestLog::EndMessage;

    ModifyAfterBasicCase<SingleOperationDuration>::init();
}

void ModifyAfterWithBufferSubDataCase::deinit(void)
{
    ModifyAfterBasicCase<SingleOperationDuration>::deinit();
}

void ModifyAfterWithBufferSubDataCase::testWithBufferSize(UploadSampleResult<SingleOperationDuration> &result,
                                                          int bufferSize)
{
    // always draw the same amount to make compares between cases sensible
    const int drawStart = deAlign32(bufferSize / 4, 4 * 4);
    const int drawEnd   = deAlign32(bufferSize * 3 / 4, 4 * 4);

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const int subdataOffset  = deAlign32((m_partialUpload) ? (bufferSize / 4) : (0), 4 * 4);
    const int subdataSize    = deAlign32((m_partialUpload) ? (bufferSize / 2) : (bufferSize), 4 * 4);
    uint64_t startTime;
    uint64_t endTime;

    // make upload-draw stream
    if (m_repeatedUpload)
    {
        for (int repeatNdx = 0; repeatNdx < NUM_REPEATS; ++repeatNdx)
        {
            gl.bufferSubData(GL_ARRAY_BUFFER, subdataOffset, subdataSize, &m_zeroData[0]);
            drawBufferRange(drawStart, drawEnd);
        }
    }

    // test upload
    startTime = deGetMicroseconds();
    gl.bufferSubData(GL_ARRAY_BUFFER, subdataOffset, subdataSize, &m_zeroData[0]);
    endTime = deGetMicroseconds();

    result.duration.totalDuration       = endTime - startTime;
    result.duration.fitResponseDuration = result.duration.totalDuration;
    result.writtenSize                  = subdataSize;
}

class ModifyAfterWithMapBufferRangeCase : public ModifyAfterBasicCase<MapBufferRangeDurationNoAlloc>
{
public:
    enum CaseFlags
    {
        FLAG_PARTIAL = 0x1,
    };

    ModifyAfterWithMapBufferRangeCase(Context &context, const char *name, const char *desc, int bufferSizeMin,
                                      int bufferSizeMax, uint32_t usage, int flags, uint32_t glMapFlags);
    ~ModifyAfterWithMapBufferRangeCase(void);

    void init(void);
    void deinit(void);

private:
    static bool isBufferUnspecifiedAfterUpload(int flags, uint32_t mapFlags);
    void testWithBufferSize(UploadSampleResult<MapBufferRangeDurationNoAlloc> &result, int bufferSize);

    const bool m_partialUpload;
    const uint32_t m_mapFlags;
};

ModifyAfterWithMapBufferRangeCase::ModifyAfterWithMapBufferRangeCase(Context &context, const char *name,
                                                                     const char *desc, int bufferSizeMin,
                                                                     int bufferSizeMax, uint32_t usage, int flags,
                                                                     uint32_t glMapFlags)
    : ModifyAfterBasicCase<MapBufferRangeDurationNoAlloc>(context, name, desc, bufferSizeMin, bufferSizeMax, usage,
                                                          isBufferUnspecifiedAfterUpload(flags, glMapFlags))
    , m_partialUpload((flags & FLAG_PARTIAL) != 0)
    , m_mapFlags(glMapFlags)
{
}

ModifyAfterWithMapBufferRangeCase::~ModifyAfterWithMapBufferRangeCase(void)
{
    deinit();
}

void ModifyAfterWithMapBufferRangeCase::init(void)
{
    // Log the purpose of the test

    m_testCtx.getLog() << tcu::TestLog::Message
                       << "Testing performance of MapBufferRange() command after a draw command that sources data from "
                          "the target buffer.\n"
                       << ((m_partialUpload) ? ("Half of the buffer is mapped.\n") : ("Whole buffer is mapped.\n"))
                       << "Map bits:\n"
                       << ((m_mapFlags & GL_MAP_WRITE_BIT) ? ("\tGL_MAP_WRITE_BIT\n") : (""))
                       << ((m_mapFlags & GL_MAP_READ_BIT) ? ("\tGL_MAP_READ_BIT\n") : (""))
                       << ((m_mapFlags & GL_MAP_INVALIDATE_RANGE_BIT) ? ("\tGL_MAP_INVALIDATE_RANGE_BIT\n") : (""))
                       << ((m_mapFlags & GL_MAP_INVALIDATE_BUFFER_BIT) ? ("\tGL_MAP_INVALIDATE_BUFFER_BIT\n") : (""))
                       << ((m_mapFlags & GL_MAP_UNSYNCHRONIZED_BIT) ? ("\tGL_MAP_UNSYNCHRONIZED_BIT\n") : (""))
                       << ((m_mapFlags & GL_MAP_FLUSH_EXPLICIT_BIT) ? ("\tGL_MAP_FLUSH_EXPLICIT_BIT\n") : (""))
                       << tcu::TestLog::EndMessage;

    ModifyAfterBasicCase<MapBufferRangeDurationNoAlloc>::init();
}

void ModifyAfterWithMapBufferRangeCase::deinit(void)
{
    ModifyAfterBasicCase<MapBufferRangeDurationNoAlloc>::deinit();
}

bool ModifyAfterWithMapBufferRangeCase::isBufferUnspecifiedAfterUpload(int flags, uint32_t mapFlags)
{
    if ((flags & FLAG_PARTIAL) != 0 && ((mapFlags & GL_MAP_INVALIDATE_BUFFER_BIT) != 0))
        return true;

    return false;
}

void ModifyAfterWithMapBufferRangeCase::testWithBufferSize(UploadSampleResult<MapBufferRangeDurationNoAlloc> &result,
                                                           int bufferSize)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const int subdataOffset  = deAlign32((m_partialUpload) ? (bufferSize / 4) : (0), 4 * 4);
    const int subdataSize    = deAlign32((m_partialUpload) ? (bufferSize / 2) : (bufferSize), 4 * 4);
    void *mapPtr;

    // map
    {
        uint64_t startTime;
        uint64_t endTime;

        startTime = deGetMicroseconds();
        mapPtr    = gl.mapBufferRange(GL_ARRAY_BUFFER, subdataOffset, subdataSize, m_mapFlags);
        endTime   = deGetMicroseconds();

        if (!mapPtr)
            throw tcu::TestError("mapBufferRange returned null");

        result.duration.mapDuration = endTime - startTime;
    }

    // write
    {
        result.duration.writeDuration = medianTimeMemcpy(mapPtr, &m_zeroData[0], subdataSize);
    }

    // unmap
    {
        uint64_t startTime;
        uint64_t endTime;
        glw::GLboolean unmapSucceeded;

        startTime      = deGetMicroseconds();
        unmapSucceeded = gl.unmapBuffer(GL_ARRAY_BUFFER);
        endTime        = deGetMicroseconds();

        if (unmapSucceeded != GL_TRUE)
            throw UnmapFailureError();

        result.duration.unmapDuration = endTime - startTime;
    }

    result.duration.totalDuration =
        result.duration.mapDuration + result.duration.writeDuration + result.duration.unmapDuration;
    result.duration.fitResponseDuration = result.duration.totalDuration;
    result.writtenSize                  = subdataSize;
}

class ModifyAfterWithMapBufferFlushCase : public ModifyAfterBasicCase<MapBufferRangeFlushDurationNoAlloc>
{
public:
    enum CaseFlags
    {
        FLAG_PARTIAL = 0x1,
    };

    ModifyAfterWithMapBufferFlushCase(Context &context, const char *name, const char *desc, int bufferSizeMin,
                                      int bufferSizeMax, uint32_t usage, int flags, uint32_t glMapFlags);
    ~ModifyAfterWithMapBufferFlushCase(void);

    void init(void);
    void deinit(void);

private:
    static bool isBufferUnspecifiedAfterUpload(int flags, uint32_t mapFlags);
    void testWithBufferSize(UploadSampleResult<MapBufferRangeFlushDurationNoAlloc> &result, int bufferSize);

    const bool m_partialUpload;
    const uint32_t m_mapFlags;
};

ModifyAfterWithMapBufferFlushCase::ModifyAfterWithMapBufferFlushCase(Context &context, const char *name,
                                                                     const char *desc, int bufferSizeMin,
                                                                     int bufferSizeMax, uint32_t usage, int flags,
                                                                     uint32_t glMapFlags)
    : ModifyAfterBasicCase<MapBufferRangeFlushDurationNoAlloc>(context, name, desc, bufferSizeMin, bufferSizeMax, usage,
                                                               isBufferUnspecifiedAfterUpload(flags, glMapFlags))
    , m_partialUpload((flags & FLAG_PARTIAL) != 0)
    , m_mapFlags(glMapFlags)
{
}

ModifyAfterWithMapBufferFlushCase::~ModifyAfterWithMapBufferFlushCase(void)
{
    deinit();
}

void ModifyAfterWithMapBufferFlushCase::init(void)
{
    // Log the purpose of the test

    m_testCtx.getLog() << tcu::TestLog::Message
                       << "Testing performance of MapBufferRange() command after a draw command that sources data from "
                          "the target buffer.\n"
                       << ((m_partialUpload) ? ("Half of the buffer is mapped.\n") : ("Whole buffer is mapped.\n"))
                       << "Map bits:\n"
                       << ((m_mapFlags & GL_MAP_WRITE_BIT) ? ("\tGL_MAP_WRITE_BIT\n") : (""))
                       << ((m_mapFlags & GL_MAP_READ_BIT) ? ("\tGL_MAP_READ_BIT\n") : (""))
                       << ((m_mapFlags & GL_MAP_INVALIDATE_RANGE_BIT) ? ("\tGL_MAP_INVALIDATE_RANGE_BIT\n") : (""))
                       << ((m_mapFlags & GL_MAP_INVALIDATE_BUFFER_BIT) ? ("\tGL_MAP_INVALIDATE_BUFFER_BIT\n") : (""))
                       << ((m_mapFlags & GL_MAP_UNSYNCHRONIZED_BIT) ? ("\tGL_MAP_UNSYNCHRONIZED_BIT\n") : (""))
                       << ((m_mapFlags & GL_MAP_FLUSH_EXPLICIT_BIT) ? ("\tGL_MAP_FLUSH_EXPLICIT_BIT\n") : (""))
                       << tcu::TestLog::EndMessage;

    ModifyAfterBasicCase<MapBufferRangeFlushDurationNoAlloc>::init();
}

void ModifyAfterWithMapBufferFlushCase::deinit(void)
{
    ModifyAfterBasicCase<MapBufferRangeFlushDurationNoAlloc>::deinit();
}

bool ModifyAfterWithMapBufferFlushCase::isBufferUnspecifiedAfterUpload(int flags, uint32_t mapFlags)
{
    if ((flags & FLAG_PARTIAL) != 0 && ((mapFlags & GL_MAP_INVALIDATE_BUFFER_BIT) != 0))
        return true;

    return false;
}

void ModifyAfterWithMapBufferFlushCase::testWithBufferSize(
    UploadSampleResult<MapBufferRangeFlushDurationNoAlloc> &result, int bufferSize)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const int subdataOffset  = deAlign32((m_partialUpload) ? (bufferSize / 4) : (0), 4 * 4);
    const int subdataSize    = deAlign32((m_partialUpload) ? (bufferSize / 2) : (bufferSize), 4 * 4);
    void *mapPtr;

    // map
    {
        uint64_t startTime;
        uint64_t endTime;

        startTime = deGetMicroseconds();
        mapPtr    = gl.mapBufferRange(GL_ARRAY_BUFFER, subdataOffset, subdataSize, m_mapFlags);
        endTime   = deGetMicroseconds();

        if (!mapPtr)
            throw tcu::TestError("mapBufferRange returned null");

        result.duration.mapDuration = endTime - startTime;
    }

    // write
    {
        result.duration.writeDuration = medianTimeMemcpy(mapPtr, &m_zeroData[0], subdataSize);
    }

    // flush
    {
        uint64_t startTime;
        uint64_t endTime;

        startTime = deGetMicroseconds();
        gl.flushMappedBufferRange(GL_ARRAY_BUFFER, 0, subdataSize);
        endTime = deGetMicroseconds();

        result.duration.flushDuration = endTime - startTime;
    }

    // unmap
    {
        uint64_t startTime;
        uint64_t endTime;
        glw::GLboolean unmapSucceeded;

        startTime      = deGetMicroseconds();
        unmapSucceeded = gl.unmapBuffer(GL_ARRAY_BUFFER);
        endTime        = deGetMicroseconds();

        if (unmapSucceeded != GL_TRUE)
            throw UnmapFailureError();

        result.duration.unmapDuration = endTime - startTime;
    }

    result.duration.totalDuration = result.duration.mapDuration + result.duration.writeDuration +
                                    result.duration.unmapDuration + result.duration.flushDuration;
    result.duration.fitResponseDuration = result.duration.totalDuration;
    result.writtenSize                  = subdataSize;
}

enum DrawMethod
{
    DRAWMETHOD_DRAW_ARRAYS = 0,
    DRAWMETHOD_DRAW_ELEMENTS,

    DRAWMETHOD_LAST
};

enum TargetBuffer
{
    TARGETBUFFER_VERTEX = 0,
    TARGETBUFFER_INDEX,

    TARGETBUFFER_LAST
};

enum BufferState
{
    BUFFERSTATE_NEW = 0,
    BUFFERSTATE_EXISTING,

    BUFFERSTATE_LAST
};

enum UploadMethod
{
    UPLOADMETHOD_BUFFER_DATA = 0,
    UPLOADMETHOD_BUFFER_SUB_DATA,
    UPLOADMETHOD_MAP_BUFFER_RANGE,

    UPLOADMETHOD_LAST
};

enum UnrelatedBufferType
{
    UNRELATEDBUFFERTYPE_NONE = 0,
    UNRELATEDBUFFERTYPE_VERTEX,

    UNRELATEDBUFFERTYPE_LAST
};

enum UploadRange
{
    UPLOADRANGE_FULL = 0,
    UPLOADRANGE_PARTIAL,

    UPLOADRANGE_LAST
};

struct LayeredGridSpec
{
    int gridWidth;
    int gridHeight;
    int gridLayers;
};

static int getLayeredGridNumVertices(const LayeredGridSpec &scene)
{
    return scene.gridWidth * scene.gridHeight * scene.gridLayers * 6;
}

static void generateLayeredGridVertexAttribData4C4V(std::vector<tcu::Vec4> &vertexData, const LayeredGridSpec &scene)
{
    // interleave color & vertex data
    const tcu::Vec4 green(0.0f, 1.0f, 0.0f, 0.7f);
    const tcu::Vec4 yellow(1.0f, 1.0f, 0.0f, 0.8f);

    vertexData.resize(getLayeredGridNumVertices(scene) * 2);

    for (int cellY = 0; cellY < scene.gridHeight; ++cellY)
        for (int cellX = 0; cellX < scene.gridWidth; ++cellX)
            for (int cellZ = 0; cellZ < scene.gridLayers; ++cellZ)
            {
                const tcu::Vec4 color  = (((cellX + cellY + cellZ) % 2) == 0) ? (green) : (yellow);
                const float cellLeft   = (float(cellX) / (float)scene.gridWidth - 0.5f) * 2.0f;
                const float cellRight  = (float(cellX + 1) / (float)scene.gridWidth - 0.5f) * 2.0f;
                const float cellTop    = (float(cellY + 1) / (float)scene.gridHeight - 0.5f) * 2.0f;
                const float cellBottom = (float(cellY) / (float)scene.gridHeight - 0.5f) * 2.0f;

                vertexData[(cellY * scene.gridWidth * scene.gridLayers + cellX * scene.gridLayers + cellZ) * 12 + 0] =
                    color;
                vertexData[(cellY * scene.gridWidth * scene.gridLayers + cellX * scene.gridLayers + cellZ) * 12 + 1] =
                    tcu::Vec4(cellLeft, cellTop, 0.0f, 1.0f);

                vertexData[(cellY * scene.gridWidth * scene.gridLayers + cellX * scene.gridLayers + cellZ) * 12 + 2] =
                    color;
                vertexData[(cellY * scene.gridWidth * scene.gridLayers + cellX * scene.gridLayers + cellZ) * 12 + 3] =
                    tcu::Vec4(cellLeft, cellBottom, 0.0f, 1.0f);

                vertexData[(cellY * scene.gridWidth * scene.gridLayers + cellX * scene.gridLayers + cellZ) * 12 + 4] =
                    color;
                vertexData[(cellY * scene.gridWidth * scene.gridLayers + cellX * scene.gridLayers + cellZ) * 12 + 5] =
                    tcu::Vec4(cellRight, cellBottom, 0.0f, 1.0f);

                vertexData[(cellY * scene.gridWidth * scene.gridLayers + cellX * scene.gridLayers + cellZ) * 12 + 6] =
                    color;
                vertexData[(cellY * scene.gridWidth * scene.gridLayers + cellX * scene.gridLayers + cellZ) * 12 + 7] =
                    tcu::Vec4(cellLeft, cellTop, 0.0f, 1.0f);

                vertexData[(cellY * scene.gridWidth * scene.gridLayers + cellX * scene.gridLayers + cellZ) * 12 + 8] =
                    color;
                vertexData[(cellY * scene.gridWidth * scene.gridLayers + cellX * scene.gridLayers + cellZ) * 12 + 9] =
                    tcu::Vec4(cellRight, cellBottom, 0.0f, 1.0f);

                vertexData[(cellY * scene.gridWidth * scene.gridLayers + cellX * scene.gridLayers + cellZ) * 12 + 10] =
                    color;
                vertexData[(cellY * scene.gridWidth * scene.gridLayers + cellX * scene.gridLayers + cellZ) * 12 + 11] =
                    tcu::Vec4(cellRight, cellTop, 0.0f, 1.0f);
            }
}

static void generateLayeredGridIndexData(std::vector<uint32_t> &indexData, const LayeredGridSpec &scene)
{
    indexData.resize(getLayeredGridNumVertices(scene) * 2);

    for (int ndx = 0; ndx < scene.gridLayers * scene.gridHeight * scene.gridWidth * 6; ++ndx)
        indexData[ndx] = ndx;
}

class RenderPerformanceTestBase : public TestCase
{
public:
    RenderPerformanceTestBase(Context &context, const char *name, const char *description);
    ~RenderPerformanceTestBase(void);

protected:
    void init(void);
    void deinit(void);

    void waitGLResults(void) const;
    void setupVertexAttribs(void) const;

    enum
    {
        RENDER_AREA_SIZE = 128
    };

private:
    glu::ShaderProgram *m_renderProgram;
    int m_colorLoc;
    int m_positionLoc;
};

RenderPerformanceTestBase::RenderPerformanceTestBase(Context &context, const char *name, const char *description)
    : TestCase(context, tcu::NODETYPE_PERFORMANCE, name, description)
    , m_renderProgram(nullptr)
    , m_colorLoc(0)
    , m_positionLoc(0)
{
}

RenderPerformanceTestBase::~RenderPerformanceTestBase(void)
{
    deinit();
}

void RenderPerformanceTestBase::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    m_renderProgram = new glu::ShaderProgram(m_context.getRenderContext(),
                                             glu::ProgramSources() << glu::VertexSource(s_colorVertexShader)
                                                                   << glu::FragmentSource(s_colorFragmentShader));
    if (!m_renderProgram->isOk())
    {
        m_testCtx.getLog() << *m_renderProgram;
        throw tcu::TestError("could not build program");
    }

    m_colorLoc    = gl.getAttribLocation(m_renderProgram->getProgram(), "a_color");
    m_positionLoc = gl.getAttribLocation(m_renderProgram->getProgram(), "a_position");

    if (m_colorLoc == -1)
        throw tcu::TestError("Location of attribute a_color was -1");
    if (m_positionLoc == -1)
        throw tcu::TestError("Location of attribute a_position was -1");
}

void RenderPerformanceTestBase::deinit(void)
{
    delete m_renderProgram;
    m_renderProgram = nullptr;
}

void RenderPerformanceTestBase::setupVertexAttribs(void) const
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    // buffers are bound

    gl.enableVertexAttribArray(m_colorLoc);
    gl.enableVertexAttribArray(m_positionLoc);

    gl.vertexAttribPointer(m_colorLoc, 4, GL_FLOAT, GL_FALSE, (glw::GLsizei)(8 * sizeof(float)),
                           glu::BufferOffsetAsPointer(0 * sizeof(tcu::Vec4)));
    gl.vertexAttribPointer(m_positionLoc, 4, GL_FLOAT, GL_FALSE, (glw::GLsizei)(8 * sizeof(float)),
                           glu::BufferOffsetAsPointer(1 * sizeof(tcu::Vec4)));

    gl.useProgram(m_renderProgram->getProgram());

    GLU_EXPECT_NO_ERROR(gl.getError(), "set up rendering");
}

void RenderPerformanceTestBase::waitGLResults(void) const
{
    tcu::Surface unusedSurface(RENDER_AREA_SIZE, RENDER_AREA_SIZE);
    glu::readPixels(m_context.getRenderContext(), 0, 0, unusedSurface.getAccess());
}

template <typename SampleType>
class RenderCase : public RenderPerformanceTestBase
{
public:
    RenderCase(Context &context, const char *name, const char *description, DrawMethod drawMethod);
    ~RenderCase(void);

protected:
    void init(void);
    void deinit(void);

private:
    IterateResult iterate(void);

protected:
    struct SampleResult
    {
        LayeredGridSpec scene;
        RenderSampleResult<SampleType> result;
    };

    int getMinWorkloadSize(void) const;
    int getMaxWorkloadSize(void) const;
    int getMinWorkloadDataSize(void) const;
    int getMaxWorkloadDataSize(void) const;
    int getVertexDataSize(void) const;
    int getNumSamples(void) const;
    void uploadScene(const LayeredGridSpec &scene);

    virtual void runSample(SampleResult &sample) = 0;
    virtual void logAndSetTestResult(const std::vector<SampleResult> &results);

    void mapResultsToRenderRateFormat(std::vector<RenderSampleResult<SampleType>> &dst,
                                      const std::vector<SampleResult> &src) const;

    const DrawMethod m_drawMethod;

private:
    glw::GLuint m_attributeBufferID;
    glw::GLuint m_indexBufferID;
    int m_iterationNdx;
    std::vector<int> m_iterationOrder;
    std::vector<SampleResult> m_results;
    int m_numUnmapFailures;
};

template <typename SampleType>
RenderCase<SampleType>::RenderCase(Context &context, const char *name, const char *description, DrawMethod drawMethod)
    : RenderPerformanceTestBase(context, name, description)
    , m_drawMethod(drawMethod)
    , m_attributeBufferID(0)
    , m_indexBufferID(0)
    , m_iterationNdx(0)
    , m_numUnmapFailures(0)
{
    DE_ASSERT(drawMethod < DRAWMETHOD_LAST);
}

template <typename SampleType>
RenderCase<SampleType>::~RenderCase(void)
{
    deinit();
}

template <typename SampleType>
void RenderCase<SampleType>::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    RenderPerformanceTestBase::init();

    // requirements

    if (m_context.getRenderTarget().getWidth() < RENDER_AREA_SIZE ||
        m_context.getRenderTarget().getHeight() < RENDER_AREA_SIZE)
        throw tcu::NotSupportedError("Test case requires " + de::toString<int>(RENDER_AREA_SIZE) + "x" +
                                     de::toString<int>(RENDER_AREA_SIZE) + " render target");

    // gl state

    gl.viewport(0, 0, RENDER_AREA_SIZE, RENDER_AREA_SIZE);

    // enable bleding to prevent grid layers from being discarded
    gl.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl.blendEquation(GL_FUNC_ADD);
    gl.enable(GL_BLEND);

    // generate iterations

    {
        const int gridSizes[] = {20, 26, 32, 38, 44, 50, 56, 62, 68, 74, 80, 86, 92, 98, 104, 110, 116, 122, 128};

        for (int gridNdx = 0; gridNdx < DE_LENGTH_OF_ARRAY(gridSizes); ++gridNdx)
        {
            m_results.push_back(SampleResult());

            m_results.back().scene.gridHeight = gridSizes[gridNdx];
            m_results.back().scene.gridWidth  = gridSizes[gridNdx];
            m_results.back().scene.gridLayers = 5;

            m_results.back().result.numVertices = getLayeredGridNumVertices(m_results.back().scene);

            // test cases set these, initialize to unused values
            m_results.back().result.renderDataSize    = -1;
            m_results.back().result.uploadedDataSize  = -1;
            m_results.back().result.unrelatedDataSize = -1;
        }
    }

    // randomize iteration order
    {
        m_iterationOrder.resize(m_results.size());
        generateTwoPassRandomIterationOrder(m_iterationOrder, (int)m_iterationOrder.size());
    }
}

template <typename SampleType>
void RenderCase<SampleType>::deinit(void)
{
    RenderPerformanceTestBase::deinit();

    if (m_attributeBufferID)
    {
        m_context.getRenderContext().getFunctions().deleteBuffers(1, &m_attributeBufferID);
        m_attributeBufferID = 0;
    }

    if (m_indexBufferID)
    {
        m_context.getRenderContext().getFunctions().deleteBuffers(1, &m_indexBufferID);
        m_indexBufferID = 0;
    }
}

template <typename SampleType>
typename RenderCase<SampleType>::IterateResult RenderCase<SampleType>::iterate(void)
{
    const int unmapFailureThreshold = 3;
    const int currentIteration      = m_iterationNdx;
    const int currentConfigNdx      = m_iterationOrder[currentIteration];
    SampleResult &currentSample     = m_results[currentConfigNdx];

    try
    {
        runSample(currentSample);
        ++m_iterationNdx;
    }
    catch (const UnmapFailureError &ex)
    {
        DE_UNREF(ex);
        ++m_numUnmapFailures;
    }

    if (m_numUnmapFailures > unmapFailureThreshold)
        throw tcu::TestError("Got too many unmap errors");

    if (m_iterationNdx < (int)m_iterationOrder.size())
        return CONTINUE;

    logAndSetTestResult(m_results);
    return STOP;
}

template <typename SampleType>
int RenderCase<SampleType>::getMinWorkloadSize(void) const
{
    int result = getLayeredGridNumVertices(m_results[0].scene);

    for (int ndx = 1; ndx < (int)m_results.size(); ++ndx)
    {
        const int workloadSize = getLayeredGridNumVertices(m_results[ndx].scene);
        result                 = de::min(result, workloadSize);
    }

    return result;
}

template <typename SampleType>
int RenderCase<SampleType>::getMaxWorkloadSize(void) const
{
    int result = getLayeredGridNumVertices(m_results[0].scene);

    for (int ndx = 1; ndx < (int)m_results.size(); ++ndx)
    {
        const int workloadSize = getLayeredGridNumVertices(m_results[ndx].scene);
        result                 = de::max(result, workloadSize);
    }

    return result;
}

template <typename SampleType>
int RenderCase<SampleType>::getMinWorkloadDataSize(void) const
{
    return getMinWorkloadSize() * getVertexDataSize();
}

template <typename SampleType>
int RenderCase<SampleType>::getMaxWorkloadDataSize(void) const
{
    return getMaxWorkloadSize() * getVertexDataSize();
}

template <typename SampleType>
int RenderCase<SampleType>::getVertexDataSize(void) const
{
    const int numVectors = 2;
    const int vec4Size   = 4 * sizeof(float);

    return numVectors * vec4Size;
}

template <typename SampleType>
int RenderCase<SampleType>::getNumSamples(void) const
{
    return (int)m_results.size();
}

template <typename SampleType>
void RenderCase<SampleType>::uploadScene(const LayeredGridSpec &scene)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    // vertex buffer
    {
        std::vector<tcu::Vec4> vertexData;

        generateLayeredGridVertexAttribData4C4V(vertexData, scene);

        if (m_attributeBufferID == 0)
            gl.genBuffers(1, &m_attributeBufferID);
        gl.bindBuffer(GL_ARRAY_BUFFER, m_attributeBufferID);
        gl.bufferData(GL_ARRAY_BUFFER, (int)(vertexData.size() * sizeof(tcu::Vec4)), &vertexData[0], GL_STATIC_DRAW);
    }

    // index buffer
    if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
    {
        std::vector<uint32_t> indexData;

        generateLayeredGridIndexData(indexData, scene);

        if (m_indexBufferID == 0)
            gl.genBuffers(1, &m_indexBufferID);
        gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBufferID);
        gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, (int)(indexData.size() * sizeof(uint32_t)), &indexData[0],
                      GL_STATIC_DRAW);
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "create buffers");
}

template <typename SampleType>
void RenderCase<SampleType>::logAndSetTestResult(const std::vector<SampleResult> &results)
{
    std::vector<RenderSampleResult<SampleType>> mappedResults;

    mapResultsToRenderRateFormat(mappedResults, results);

    {
        const RenderSampleAnalyzeResult analysis = analyzeSampleResults(m_testCtx.getLog(), mappedResults);
        const float rate                         = analysis.renderRateAtRange;

        if (rate == std::numeric_limits<float>::infinity())
        {
            // sample times are 1) invalid or 2) timer resolution too low
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::floatToString(0.0f, 2).c_str());
        }
        else
        {
            // report transfer rate in millions of MiB/s
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::floatToString(rate / 1024.0f / 1024.0f, 2).c_str());
        }
    }
}

template <typename SampleType>
void RenderCase<SampleType>::mapResultsToRenderRateFormat(std::vector<RenderSampleResult<SampleType>> &dst,
                                                          const std::vector<SampleResult> &src) const
{
    dst.resize(src.size());

    for (int ndx = 0; ndx < (int)src.size(); ++ndx)
        dst[ndx] = src[ndx].result;
}

class ReferenceRenderTimeCase : public RenderCase<RenderReadDuration>
{
public:
    ReferenceRenderTimeCase(Context &context, const char *name, const char *description, DrawMethod drawMethod);

private:
    void init(void);
    void runSample(SampleResult &sample);
};

ReferenceRenderTimeCase::ReferenceRenderTimeCase(Context &context, const char *name, const char *description,
                                                 DrawMethod drawMethod)
    : RenderCase<RenderReadDuration>(context, name, description, drawMethod)
{
}

void ReferenceRenderTimeCase::init(void)
{
    const char *const targetFunctionName = (m_drawMethod == DRAWMETHOD_DRAW_ARRAYS) ? ("drawArrays") : ("drawElements");

    // init parent
    RenderCase<RenderReadDuration>::init();

    // log
    m_testCtx.getLog() << tcu::TestLog::Message << "Measuring the time used in " << targetFunctionName
                       << " and readPixels call with different rendering workloads.\n"
                       << getNumSamples() << " test samples. Sample order is randomized.\n"
                       << "All samples at even positions (first = 0) are tested before samples at odd positions.\n"
                       << "Generated workload is multiple viewport-covering grids with varying number of cells, each "
                          "cell is two separate triangles.\n"
                       << "Workload sizes are in the range [" << getMinWorkloadSize() << ",  " << getMaxWorkloadSize()
                       << "] vertices ([" << getHumanReadableByteSize(getMinWorkloadDataSize()) << ","
                       << getHumanReadableByteSize(getMaxWorkloadDataSize()) << "] to be processed).\n"
                       << "Test result is the approximated total processing rate in MiB / s.\n"
                       << ((m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS) ?
                               ("Note that index array size is not included in the processed size.\n") :
                               (""))
                       << "Note! Test result should only be used as a baseline reference result for "
                          "buffer.data_upload.* test group results."
                       << tcu::TestLog::EndMessage;
}

void ReferenceRenderTimeCase::runSample(SampleResult &sample)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    tcu::Surface resultSurface(RENDER_AREA_SIZE, RENDER_AREA_SIZE);
    const int numVertices = getLayeredGridNumVertices(sample.scene);
    const glu::Buffer arrayBuffer(m_context.getRenderContext());
    const glu::Buffer indexBuffer(m_context.getRenderContext());
    std::vector<tcu::Vec4> vertexData;
    std::vector<uint32_t> indexData;
    uint64_t startTime;
    uint64_t endTime;

    // generate and upload buffers

    generateLayeredGridVertexAttribData4C4V(vertexData, sample.scene);
    gl.bindBuffer(GL_ARRAY_BUFFER, *arrayBuffer);
    gl.bufferData(GL_ARRAY_BUFFER, (int)(vertexData.size() * sizeof(tcu::Vec4)), &vertexData[0], GL_STATIC_DRAW);

    if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
    {
        generateLayeredGridIndexData(indexData, sample.scene);
        gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, *indexBuffer);
        gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, (int)(indexData.size() * sizeof(uint32_t)), &indexData[0],
                      GL_STATIC_DRAW);
    }

    setupVertexAttribs();

    // make sure data is uploaded

    if (m_drawMethod == DRAWMETHOD_DRAW_ARRAYS)
        gl.drawArrays(GL_TRIANGLES, 0, numVertices);
    else if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
        gl.drawElements(GL_TRIANGLES, numVertices, GL_UNSIGNED_INT, nullptr);
    else
        DE_ASSERT(false);
    waitGLResults();

    gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl.clear(GL_COLOR_BUFFER_BIT);
    waitGLResults();

    tcu::warmupCPU();

    // Measure both draw and associated readpixels
    {
        startTime = deGetMicroseconds();

        if (m_drawMethod == DRAWMETHOD_DRAW_ARRAYS)
            gl.drawArrays(GL_TRIANGLES, 0, numVertices);
        else if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
            gl.drawElements(GL_TRIANGLES, numVertices, GL_UNSIGNED_INT, nullptr);
        else
            DE_ASSERT(false);

        endTime = deGetMicroseconds();

        sample.result.duration.renderDuration = endTime - startTime;
    }

    {
        startTime = deGetMicroseconds();
        glu::readPixels(m_context.getRenderContext(), 0, 0, resultSurface.getAccess());
        endTime = deGetMicroseconds();

        sample.result.duration.readDuration = endTime - startTime;
    }

    sample.result.renderDataSize    = getVertexDataSize() * sample.result.numVertices;
    sample.result.uploadedDataSize  = 0;
    sample.result.unrelatedDataSize = 0;
    sample.result.duration.renderReadDuration =
        sample.result.duration.renderDuration + sample.result.duration.readDuration;
    sample.result.duration.totalDuration = sample.result.duration.renderDuration + sample.result.duration.readDuration;
    sample.result.duration.fitResponseDuration = sample.result.duration.renderReadDuration;
}

class UnrelatedUploadRenderTimeCase : public RenderCase<UnrelatedUploadRenderReadDuration>
{
public:
    UnrelatedUploadRenderTimeCase(Context &context, const char *name, const char *description, DrawMethod drawMethod,
                                  UploadMethod unrelatedUploadMethod);

private:
    void init(void);
    void runSample(SampleResult &sample);

    const UploadMethod m_unrelatedUploadMethod;
};

UnrelatedUploadRenderTimeCase::UnrelatedUploadRenderTimeCase(Context &context, const char *name,
                                                             const char *description, DrawMethod drawMethod,
                                                             UploadMethod unrelatedUploadMethod)
    : RenderCase<UnrelatedUploadRenderReadDuration>(context, name, description, drawMethod)
    , m_unrelatedUploadMethod(unrelatedUploadMethod)
{
    DE_ASSERT(m_unrelatedUploadMethod < UPLOADMETHOD_LAST);
}

void UnrelatedUploadRenderTimeCase::init(void)
{
    const char *const targetFunctionName = (m_drawMethod == DRAWMETHOD_DRAW_ARRAYS) ? ("drawArrays") : ("drawElements");
    tcu::MessageBuilder message(&m_testCtx.getLog());

    // init parent
    RenderCase<UnrelatedUploadRenderReadDuration>::init();

    // log

    message << "Measuring the time used in " << targetFunctionName
            << " and readPixels call with different rendering workloads.\n"
            << "Uploading an unrelated buffer just before issuing the rendering command with "
            << ((m_unrelatedUploadMethod != UPLOADMETHOD_BUFFER_DATA)      ? ("bufferData") :
                (m_unrelatedUploadMethod != UPLOADMETHOD_BUFFER_SUB_DATA)  ? ("bufferSubData") :
                (m_unrelatedUploadMethod != UPLOADMETHOD_MAP_BUFFER_RANGE) ? ("mapBufferRange") :
                                                                             (nullptr))
            << ".\n"
            << getNumSamples() << " test samples. Sample order is randomized.\n"
            << "All samples at even positions (first = 0) are tested before samples at odd positions.\n"
            << "Generated workload is multiple viewport-covering grids with varying number of cells, each cell is two "
               "separate triangles.\n"
            << "Workload sizes are in the range [" << getMinWorkloadSize() << ",  " << getMaxWorkloadSize()
            << "] vertices ([" << getHumanReadableByteSize(getMinWorkloadDataSize()) << ","
            << getHumanReadableByteSize(getMaxWorkloadDataSize()) << "] to be processed).\n"
            << "Unrelated upload sizes are in the range [" << getHumanReadableByteSize(getMinWorkloadDataSize()) << ", "
            << getHumanReadableByteSize(getMaxWorkloadDataSize()) << "]\n"
            << "Test result is the approximated total processing rate in MiB / s.\n"
            << ((m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS) ?
                    ("Note that index array size is not included in the processed size.\n") :
                    (""))
            << "Note that the data size and the time used in the unrelated upload is not included in the results.\n"
            << "Note! Test result may not be useful as is but instead should be compared against the reference.* group "
               "and upload_and_draw.*_and_unrelated_upload group results.\n"
            << tcu::TestLog::EndMessage;
}

void UnrelatedUploadRenderTimeCase::runSample(SampleResult &sample)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    tcu::Surface resultSurface(RENDER_AREA_SIZE, RENDER_AREA_SIZE);
    const int numVertices = getLayeredGridNumVertices(sample.scene);
    const glu::Buffer arrayBuffer(m_context.getRenderContext());
    const glu::Buffer indexBuffer(m_context.getRenderContext());
    const glu::Buffer unrelatedBuffer(m_context.getRenderContext());
    int unrelatedUploadSize = -1;
    int renderUploadSize;
    std::vector<tcu::Vec4> vertexData;
    std::vector<uint32_t> indexData;
    uint64_t startTime;
    uint64_t endTime;

    // generate and upload buffers

    generateLayeredGridVertexAttribData4C4V(vertexData, sample.scene);
    renderUploadSize = (int)(vertexData.size() * sizeof(tcu::Vec4));

    gl.bindBuffer(GL_ARRAY_BUFFER, *arrayBuffer);
    gl.bufferData(GL_ARRAY_BUFFER, renderUploadSize, &vertexData[0], GL_STATIC_DRAW);

    if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
    {
        generateLayeredGridIndexData(indexData, sample.scene);
        gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, *indexBuffer);
        gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, (int)(indexData.size() * sizeof(uint32_t)), &indexData[0],
                      GL_STATIC_DRAW);
    }

    setupVertexAttribs();

    // make sure data is uploaded

    if (m_drawMethod == DRAWMETHOD_DRAW_ARRAYS)
        gl.drawArrays(GL_TRIANGLES, 0, numVertices);
    else if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
        gl.drawElements(GL_TRIANGLES, numVertices, GL_UNSIGNED_INT, nullptr);
    else
        DE_ASSERT(false);
    waitGLResults();

    gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl.clear(GL_COLOR_BUFFER_BIT);
    waitGLResults();

    tcu::warmupCPU();

    // Unrelated upload
    if (m_unrelatedUploadMethod == UPLOADMETHOD_BUFFER_DATA)
    {
        unrelatedUploadSize = (int)(vertexData.size() * sizeof(tcu::Vec4));

        gl.bindBuffer(GL_ARRAY_BUFFER, *unrelatedBuffer);
        gl.bufferData(GL_ARRAY_BUFFER, unrelatedUploadSize, &vertexData[0], GL_STATIC_DRAW);
    }
    else if (m_unrelatedUploadMethod == UPLOADMETHOD_BUFFER_SUB_DATA)
    {
        unrelatedUploadSize = (int)(vertexData.size() * sizeof(tcu::Vec4));

        gl.bindBuffer(GL_ARRAY_BUFFER, *unrelatedBuffer);
        gl.bufferData(GL_ARRAY_BUFFER, unrelatedUploadSize, nullptr, GL_STATIC_DRAW);
        gl.bufferSubData(GL_ARRAY_BUFFER, 0, unrelatedUploadSize, &vertexData[0]);
    }
    else if (m_unrelatedUploadMethod == UPLOADMETHOD_MAP_BUFFER_RANGE)
    {
        void *mapPtr;
        glw::GLboolean unmapSuccessful;

        unrelatedUploadSize = (int)(vertexData.size() * sizeof(tcu::Vec4));

        gl.bindBuffer(GL_ARRAY_BUFFER, *unrelatedBuffer);
        gl.bufferData(GL_ARRAY_BUFFER, unrelatedUploadSize, nullptr, GL_STATIC_DRAW);

        mapPtr = gl.mapBufferRange(GL_ARRAY_BUFFER, 0, unrelatedUploadSize,
                                   GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT |
                                       GL_MAP_UNSYNCHRONIZED_BIT);
        if (!mapPtr)
            throw tcu::Exception("MapBufferRange returned NULL");

        deMemcpy(mapPtr, &vertexData[0], unrelatedUploadSize);

        // if unmapping fails, just try again later
        unmapSuccessful = gl.unmapBuffer(GL_ARRAY_BUFFER);
        if (!unmapSuccessful)
            throw UnmapFailureError();
    }
    else
        DE_ASSERT(false);

    DE_ASSERT(unrelatedUploadSize != -1);

    // Measure both draw and associated readpixels
    {
        startTime = deGetMicroseconds();

        if (m_drawMethod == DRAWMETHOD_DRAW_ARRAYS)
            gl.drawArrays(GL_TRIANGLES, 0, numVertices);
        else if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
            gl.drawElements(GL_TRIANGLES, numVertices, GL_UNSIGNED_INT, nullptr);
        else
            DE_ASSERT(false);

        endTime = deGetMicroseconds();

        sample.result.duration.renderDuration = endTime - startTime;
    }

    {
        startTime = deGetMicroseconds();
        glu::readPixels(m_context.getRenderContext(), 0, 0, resultSurface.getAccess());
        endTime = deGetMicroseconds();

        sample.result.duration.readDuration = endTime - startTime;
    }

    sample.result.renderDataSize    = getVertexDataSize() * sample.result.numVertices;
    sample.result.uploadedDataSize  = renderUploadSize;
    sample.result.unrelatedDataSize = unrelatedUploadSize;
    sample.result.duration.renderReadDuration =
        sample.result.duration.renderDuration + sample.result.duration.readDuration;
    sample.result.duration.totalDuration = sample.result.duration.renderDuration + sample.result.duration.readDuration;
    sample.result.duration.fitResponseDuration = sample.result.duration.renderReadDuration;
}

class ReferenceReadPixelsTimeCase : public TestCase
{
public:
    ReferenceReadPixelsTimeCase(Context &context, const char *name, const char *description);

private:
    void init(void);
    IterateResult iterate(void);
    void logAndSetTestResult(void);

    enum
    {
        RENDER_AREA_SIZE = 128
    };

    const int m_numSamples;
    int m_sampleNdx;
    std::vector<int> m_samples;
};

ReferenceReadPixelsTimeCase::ReferenceReadPixelsTimeCase(Context &context, const char *name, const char *description)
    : TestCase(context, tcu::NODETYPE_PERFORMANCE, name, description)
    , m_numSamples(20)
    , m_sampleNdx(0)
    , m_samples(m_numSamples)
{
}

void ReferenceReadPixelsTimeCase::init(void)
{
    m_testCtx.getLog() << tcu::TestLog::Message << "Measuring the time used in a single readPixels call with "
                       << m_numSamples << " test samples.\n"
                       << "Test result is the median of the samples in microseconds.\n"
                       << "Note! Test result should only be used as a baseline reference result for "
                          "buffer.data_upload.* test group results."
                       << tcu::TestLog::EndMessage;
}

ReferenceReadPixelsTimeCase::IterateResult ReferenceReadPixelsTimeCase::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    tcu::Surface resultSurface(RENDER_AREA_SIZE, RENDER_AREA_SIZE);
    uint64_t startTime;
    uint64_t endTime;

    deYield();
    tcu::warmupCPU();
    deYield();

    // "Render" something and wait for it
    gl.clearColor(0.0f, 1.0f, float(m_sampleNdx) / float(m_numSamples), 1.0f);
    gl.clear(GL_COLOR_BUFFER_BIT);

    // wait for results
    glu::readPixels(m_context.getRenderContext(), 0, 0, resultSurface.getAccess());

    // measure time used in readPixels
    startTime = deGetMicroseconds();
    glu::readPixels(m_context.getRenderContext(), 0, 0, resultSurface.getAccess());
    endTime = deGetMicroseconds();

    m_samples[m_sampleNdx] = (int)(endTime - startTime);

    if (++m_sampleNdx < m_numSamples)
        return CONTINUE;

    logAndSetTestResult();
    return STOP;
}

void ReferenceReadPixelsTimeCase::logAndSetTestResult(void)
{
    // Log sample list
    {
        m_testCtx.getLog() << tcu::TestLog::SampleList("Samples", "Samples") << tcu::TestLog::SampleInfo
                           << tcu::TestLog::ValueInfo("ReadTime", "ReadPixels time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
                           << tcu::TestLog::EndSampleInfo;

        for (int sampleNdx = 0; sampleNdx < (int)m_samples.size(); ++sampleNdx)
            m_testCtx.getLog() << tcu::TestLog::Sample << m_samples[sampleNdx] << tcu::TestLog::EndSample;

        m_testCtx.getLog() << tcu::TestLog::EndSampleList;
    }

    // Log median
    {
        float median;
        float limit60Low;
        float limit60Up;

        std::sort(m_samples.begin(), m_samples.end());
        median     = linearSample(m_samples, 0.5f);
        limit60Low = linearSample(m_samples, 0.2f);
        limit60Up  = linearSample(m_samples, 0.8f);

        m_testCtx.getLog() << tcu::TestLog::Float("Median", "Median", "us", QP_KEY_TAG_TIME, median)
                           << tcu::TestLog::Message << "60 % of samples within range:\n"
                           << tcu::TestLog::EndMessage
                           << tcu::TestLog::Float("Low60Range", "Lower", "us", QP_KEY_TAG_TIME, limit60Low)
                           << tcu::TestLog::Float("High60Range", "Upper", "us", QP_KEY_TAG_TIME, limit60Up);

        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::floatToString(median, 2).c_str());
    }
}

template <typename SampleType>
class GenericUploadRenderTimeCase : public RenderCase<SampleType>
{
public:
    typedef typename RenderCase<SampleType>::SampleResult SampleResult;

    GenericUploadRenderTimeCase(Context &context, const char *name, const char *description, DrawMethod method,
                                TargetBuffer targetBuffer, UploadMethod uploadMethod, BufferState bufferState,
                                UploadRange uploadRange, UnrelatedBufferType unrelatedBufferType);

private:
    void init(void);
    void runSample(SampleResult &sample);

    using RenderCase<SampleType>::RENDER_AREA_SIZE;

    const TargetBuffer m_targetBuffer;
    const BufferState m_bufferState;
    const UploadMethod m_uploadMethod;
    const UnrelatedBufferType m_unrelatedBufferType;
    const UploadRange m_uploadRange;

    using RenderCase<SampleType>::m_context;
    using RenderCase<SampleType>::m_testCtx;
    using RenderCase<SampleType>::m_drawMethod;
};

template <typename SampleType>
GenericUploadRenderTimeCase<SampleType>::GenericUploadRenderTimeCase(Context &context, const char *name,
                                                                     const char *description, DrawMethod method,
                                                                     TargetBuffer targetBuffer,
                                                                     UploadMethod uploadMethod, BufferState bufferState,
                                                                     UploadRange uploadRange,
                                                                     UnrelatedBufferType unrelatedBufferType)
    : RenderCase<SampleType>(context, name, description, method)
    , m_targetBuffer(targetBuffer)
    , m_bufferState(bufferState)
    , m_uploadMethod(uploadMethod)
    , m_unrelatedBufferType(unrelatedBufferType)
    , m_uploadRange(uploadRange)
{
    DE_ASSERT(m_targetBuffer < TARGETBUFFER_LAST);
    DE_ASSERT(m_bufferState < BUFFERSTATE_LAST);
    DE_ASSERT(m_uploadMethod < UPLOADMETHOD_LAST);
    DE_ASSERT(m_unrelatedBufferType < UNRELATEDBUFFERTYPE_LAST);
    DE_ASSERT(m_uploadRange < UPLOADRANGE_LAST);
}

template <typename SampleType>
void GenericUploadRenderTimeCase<SampleType>::init(void)
{
    // init parent
    RenderCase<SampleType>::init();

    // log
    {
        const char *const targetFunctionName =
            (m_drawMethod == DRAWMETHOD_DRAW_ARRAYS) ? ("drawArrays") : ("drawElements");
        const int perVertexSize =
            (m_targetBuffer == TARGETBUFFER_INDEX) ? ((int)sizeof(uint32_t)) : ((int)sizeof(tcu::Vec4[2]));
        const int fullMinUploadSize = RenderCase<SampleType>::getMinWorkloadSize() * perVertexSize;
        const int fullMaxUploadSize = RenderCase<SampleType>::getMaxWorkloadSize() * perVertexSize;
        const int minUploadSize =
            (m_uploadRange == UPLOADRANGE_FULL) ? (fullMinUploadSize) : (deAlign32(fullMinUploadSize / 2, 4));
        const int maxUploadSize =
            (m_uploadRange == UPLOADRANGE_FULL) ? (fullMaxUploadSize) : (deAlign32(fullMaxUploadSize / 2, 4));
        const int minUnrelatedUploadSize = RenderCase<SampleType>::getMinWorkloadSize() * (int)sizeof(tcu::Vec4[2]);
        const int maxUnrelatedUploadSize = RenderCase<SampleType>::getMaxWorkloadSize() * (int)sizeof(tcu::Vec4[2]);

        m_testCtx.getLog()
            << tcu::TestLog::Message << "Measuring the time used in " << targetFunctionName
            << " and readPixels call with different rendering workloads.\n"
            << "The " << ((m_targetBuffer == TARGETBUFFER_INDEX) ? ("index") : ("vertex attrib")) << " buffer "
            << ((m_bufferState == BUFFERSTATE_NEW) ? ("") : ("contents ")) << "sourced by the rendering command "
            << ((m_bufferState == BUFFERSTATE_NEW)     ? ("is uploaded ") :
                (m_uploadRange == UPLOADRANGE_FULL)    ? ("are specified ") :
                (m_uploadRange == UPLOADRANGE_PARTIAL) ? ("are updated (partial upload) ") :
                                                         (nullptr))
            << "just before issuing the rendering command.\n"
            << ((m_bufferState == BUFFERSTATE_EXISTING) ? ("The buffer has been used in rendering.\n") :
                                                          ("The buffer is generated just before uploading.\n"))
            << "Buffer "
            << ((m_bufferState == BUFFERSTATE_NEW)     ? ("is uploaded") :
                (m_uploadRange == UPLOADRANGE_FULL)    ? ("contents are specified") :
                (m_uploadRange == UPLOADRANGE_PARTIAL) ? ("contents are partially updated") :
                                                         (nullptr))
            << " with "
            << ((m_uploadMethod == UPLOADMETHOD_BUFFER_DATA)     ? ("bufferData") :
                (m_uploadMethod == UPLOADMETHOD_BUFFER_SUB_DATA) ? ("bufferSubData") :
                                                                   ("mapBufferRange"))
            << " command. Usage of the target buffer is DYNAMIC_DRAW.\n"
            << ((m_uploadMethod == UPLOADMETHOD_MAP_BUFFER_RANGE) ?
                    ("Mapping buffer with bits MAP_WRITE_BIT | MAP_INVALIDATE_RANGE_BIT | MAP_INVALIDATE_BUFFER_BIT | "
                     "MAP_UNSYNCHRONIZED_BIT\n") :
                    (""))
            << ((m_unrelatedBufferType == UNRELATEDBUFFERTYPE_VERTEX) ?
                    ("Uploading an unrelated buffer just before issuing the rendering command with bufferData.\n") :
                    (""))
            << RenderCase<SampleType>::getNumSamples() << " test samples. Sample order is randomized.\n"
            << "All samples at even positions (first = 0) are tested before samples at odd positions.\n"
            << "Generated workload is multiple viewport-covering grids with varying number of cells, each cell is two "
               "separate triangles.\n"
            << "Workload sizes are in the range [" << RenderCase<SampleType>::getMinWorkloadSize() << ",  "
            << RenderCase<SampleType>::getMaxWorkloadSize() << "] vertices "
            << "([" << getHumanReadableByteSize(RenderCase<SampleType>::getMinWorkloadDataSize()) << ","
            << getHumanReadableByteSize(RenderCase<SampleType>::getMaxWorkloadDataSize()) << "] to be processed).\n"
            << "Upload sizes are in the range [" << getHumanReadableByteSize(minUploadSize) << ","
            << getHumanReadableByteSize(maxUploadSize) << "].\n"
            << ((m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS) ?
                    ("Unrelated upload sizes are in the range [" + getHumanReadableByteSize(minUnrelatedUploadSize) +
                     ", " + getHumanReadableByteSize(maxUnrelatedUploadSize) + "]\n") :
                    (""))
            << "Test result is the approximated processing rate in MiB / s.\n"
            << "Note that while upload time is measured, the time used is not included in the results.\n"
            << ((m_unrelatedBufferType == UNRELATEDBUFFERTYPE_VERTEX) ?
                    ("Note that the data size and the time used in the unrelated upload is not included in the "
                     "results.\n") :
                    (""))
            << ((m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS) ?
                    ("Note that index array size is not included in the processed size.\n") :
                    (""))
            << "Note! Test result may not be useful as is but instead should be compared against the reference.* group "
               "and other upload_and_draw.* group results.\n"
            << tcu::TestLog::EndMessage;
    }
}

template <typename SampleType>
void GenericUploadRenderTimeCase<SampleType>::runSample(SampleResult &sample)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const glu::Buffer arrayBuffer(m_context.getRenderContext());
    const glu::Buffer indexBuffer(m_context.getRenderContext());
    const glu::Buffer unrelatedBuffer(m_context.getRenderContext());
    const int numVertices = getLayeredGridNumVertices(sample.scene);
    tcu::Surface resultSurface(RENDER_AREA_SIZE, RENDER_AREA_SIZE);
    uint64_t startTime;
    uint64_t endTime;
    std::vector<tcu::Vec4> vertexData;
    std::vector<uint32_t> indexData;

    // create data

    generateLayeredGridVertexAttribData4C4V(vertexData, sample.scene);
    if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
        generateLayeredGridIndexData(indexData, sample.scene);

    gl.bindBuffer(GL_ARRAY_BUFFER, *arrayBuffer);
    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, *indexBuffer);
    RenderCase<SampleType>::setupVertexAttribs();

    // target should be an exisiting buffer? Draw from it once to make sure it exists on the gpu

    if (m_drawMethod == DRAWMETHOD_DRAW_ARRAYS && m_bufferState == BUFFERSTATE_EXISTING)
    {
        gl.bufferData(GL_ARRAY_BUFFER, (glw::GLsizeiptr)(vertexData.size() * sizeof(tcu::Vec4)), &vertexData[0],
                      GL_DYNAMIC_DRAW);
        gl.drawArrays(GL_TRIANGLES, 0, numVertices);
    }
    else if (m_drawMethod == DRAWMETHOD_DRAW_ARRAYS && m_bufferState == BUFFERSTATE_NEW)
    {
        // do not touch the vertex buffer
    }
    else if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS && m_bufferState == BUFFERSTATE_EXISTING)
    {
        // hint that the target buffer will be modified soon
        const glw::GLenum vertexDataUsage =
            (m_targetBuffer == TARGETBUFFER_VERTEX) ? (GL_DYNAMIC_DRAW) : (GL_STATIC_DRAW);
        const glw::GLenum indexDataUsage =
            (m_targetBuffer == TARGETBUFFER_INDEX) ? (GL_DYNAMIC_DRAW) : (GL_STATIC_DRAW);

        gl.bufferData(GL_ARRAY_BUFFER, (glw::GLsizeiptr)(vertexData.size() * sizeof(tcu::Vec4)), &vertexData[0],
                      vertexDataUsage);
        gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, (glw::GLsizeiptr)(indexData.size() * sizeof(uint32_t)), &indexData[0],
                      indexDataUsage);
        gl.drawElements(GL_TRIANGLES, numVertices, GL_UNSIGNED_INT, nullptr);
    }
    else if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS && m_bufferState == BUFFERSTATE_NEW)
    {
        if (m_targetBuffer == TARGETBUFFER_VERTEX)
        {
            // make the index buffer present on the gpu
            // use another vertex buffer to keep original buffer in unused state
            const glu::Buffer vertexCopyBuffer(m_context.getRenderContext());

            gl.bindBuffer(GL_ARRAY_BUFFER, *vertexCopyBuffer);
            RenderCase<SampleType>::setupVertexAttribs();

            gl.bufferData(GL_ARRAY_BUFFER, (glw::GLsizeiptr)(vertexData.size() * sizeof(tcu::Vec4)), &vertexData[0],
                          GL_STATIC_DRAW);
            gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, (glw::GLsizeiptr)(indexData.size() * sizeof(uint32_t)),
                          &indexData[0], GL_STATIC_DRAW);
            gl.drawElements(GL_TRIANGLES, numVertices, GL_UNSIGNED_INT, nullptr);

            // restore original state
            gl.bindBuffer(GL_ARRAY_BUFFER, *arrayBuffer);
            RenderCase<SampleType>::setupVertexAttribs();
        }
        else if (m_targetBuffer == TARGETBUFFER_INDEX)
        {
            // make the vertex buffer present on the gpu
            gl.bufferData(GL_ARRAY_BUFFER, (glw::GLsizeiptr)(vertexData.size() * sizeof(tcu::Vec4)), &vertexData[0],
                          GL_STATIC_DRAW);
            gl.drawArrays(GL_TRIANGLES, 0, numVertices);
        }
        else
            DE_ASSERT(false);
    }
    else
        DE_ASSERT(false);

    RenderCase<SampleType>::waitGLResults();
    GLU_EXPECT_NO_ERROR(gl.getError(), "post buffer prepare");

    gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl.clear(GL_COLOR_BUFFER_BIT);
    RenderCase<SampleType>::waitGLResults();

    tcu::warmupCPU();

    // upload

    {
        glw::GLenum target;
        glw::GLsizeiptr size;
        glw::GLintptr offset = 0;
        const void *source;

        if (m_targetBuffer == TARGETBUFFER_VERTEX && m_uploadRange == UPLOADRANGE_FULL)
        {
            target = GL_ARRAY_BUFFER;
            size   = (glw::GLsizeiptr)(vertexData.size() * sizeof(tcu::Vec4));
            source = &vertexData[0];
        }
        else if (m_targetBuffer == TARGETBUFFER_INDEX && m_uploadRange == UPLOADRANGE_FULL)
        {
            target = GL_ELEMENT_ARRAY_BUFFER;
            size   = (glw::GLsizeiptr)(indexData.size() * sizeof(uint32_t));
            source = &indexData[0];
        }
        else if (m_targetBuffer == TARGETBUFFER_VERTEX && m_uploadRange == UPLOADRANGE_PARTIAL)
        {
            DE_ASSERT(m_bufferState == BUFFERSTATE_EXISTING);

            target = GL_ARRAY_BUFFER;
            size   = (glw::GLsizeiptr)deAlign32((int)(vertexData.size() * sizeof(tcu::Vec4)) / 2, 4);
            offset = (glw::GLintptr)deAlign32((int)size / 2, 4);
            source = (const uint8_t *)&vertexData[0] + offset;
        }
        else if (m_targetBuffer == TARGETBUFFER_INDEX && m_uploadRange == UPLOADRANGE_PARTIAL)
        {
            DE_ASSERT(m_bufferState == BUFFERSTATE_EXISTING);

            // upload to 25% - 75% range
            target = GL_ELEMENT_ARRAY_BUFFER;
            size   = (glw::GLsizeiptr)deAlign32((int32_t)(indexData.size() * sizeof(uint32_t)) / 2, 4);
            offset = (glw::GLintptr)deAlign32((int)size / 2, 4);
            source = (const uint8_t *)&indexData[0] + offset;
        }
        else
        {
            DE_ASSERT(false);
            return;
        }

        startTime = deGetMicroseconds();

        if (m_uploadMethod == UPLOADMETHOD_BUFFER_DATA)
            gl.bufferData(target, size, source, GL_DYNAMIC_DRAW);
        else if (m_uploadMethod == UPLOADMETHOD_BUFFER_SUB_DATA)
        {
            // create buffer storage
            if (m_bufferState == BUFFERSTATE_NEW)
                gl.bufferData(target, size, nullptr, GL_DYNAMIC_DRAW);
            gl.bufferSubData(target, offset, size, source);
        }
        else if (m_uploadMethod == UPLOADMETHOD_MAP_BUFFER_RANGE)
        {
            void *mapPtr;
            glw::GLboolean unmapSuccessful;

            // create buffer storage
            if (m_bufferState == BUFFERSTATE_NEW)
                gl.bufferData(target, size, nullptr, GL_DYNAMIC_DRAW);

            mapPtr = gl.mapBufferRange(target, offset, size,
                                       GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT |
                                           GL_MAP_UNSYNCHRONIZED_BIT);
            if (!mapPtr)
                throw tcu::Exception("MapBufferRange returned NULL");

            deMemcpy(mapPtr, source, (int)size);

            // if unmapping fails, just try again later
            unmapSuccessful = gl.unmapBuffer(target);
            if (!unmapSuccessful)
                throw UnmapFailureError();
        }
        else
            DE_ASSERT(false);

        endTime = deGetMicroseconds();

        sample.result.uploadedDataSize        = (int)size;
        sample.result.duration.uploadDuration = endTime - startTime;
    }

    // unrelated
    if (m_unrelatedBufferType == UNRELATEDBUFFERTYPE_VERTEX)
    {
        const int unrelatedUploadSize = (int)(vertexData.size() * sizeof(tcu::Vec4));

        gl.bindBuffer(GL_ARRAY_BUFFER, *unrelatedBuffer);
        gl.bufferData(GL_ARRAY_BUFFER, unrelatedUploadSize, &vertexData[0], GL_STATIC_DRAW);
        // Attibute pointers are not modified, no need restore state

        sample.result.unrelatedDataSize = unrelatedUploadSize;
    }

    // draw
    {
        startTime = deGetMicroseconds();

        if (m_drawMethod == DRAWMETHOD_DRAW_ARRAYS)
            gl.drawArrays(GL_TRIANGLES, 0, numVertices);
        else if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
            gl.drawElements(GL_TRIANGLES, numVertices, GL_UNSIGNED_INT, nullptr);
        else
            DE_ASSERT(false);

        endTime = deGetMicroseconds();

        sample.result.duration.renderDuration = endTime - startTime;
    }

    // read
    {
        startTime = deGetMicroseconds();
        glu::readPixels(m_context.getRenderContext(), 0, 0, resultSurface.getAccess());
        endTime = deGetMicroseconds();

        sample.result.duration.readDuration = endTime - startTime;
    }

    // set results

    sample.result.renderDataSize = RenderCase<SampleType>::getVertexDataSize() * sample.result.numVertices;

    sample.result.duration.renderReadDuration =
        sample.result.duration.renderDuration + sample.result.duration.readDuration;
    sample.result.duration.totalDuration = sample.result.duration.uploadDuration +
                                           sample.result.duration.renderDuration + sample.result.duration.readDuration;
    sample.result.duration.fitResponseDuration = sample.result.duration.renderReadDuration;
}

class BufferInUseRenderTimeCase : public RenderCase<RenderUploadRenderReadDuration>
{
public:
    enum MapFlags
    {
        MAPFLAG_NONE = 0,
        MAPFLAG_INVALIDATE_BUFFER,
        MAPFLAG_INVALIDATE_RANGE,

        MAPFLAG_LAST
    };
    enum UploadBufferTarget
    {
        UPLOADBUFFERTARGET_DIFFERENT_BUFFER = 0,
        UPLOADBUFFERTARGET_SAME_BUFFER,

        UPLOADBUFFERTARGET_LAST
    };
    BufferInUseRenderTimeCase(Context &context, const char *name, const char *description, DrawMethod method,
                              MapFlags mapFlags, TargetBuffer targetBuffer, UploadMethod uploadMethod,
                              UploadRange uploadRange, UploadBufferTarget uploadTarget);

private:
    void init(void);
    void runSample(SampleResult &sample);

    const TargetBuffer m_targetBuffer;
    const UploadMethod m_uploadMethod;
    const UploadRange m_uploadRange;
    const MapFlags m_mapFlags;
    const UploadBufferTarget m_uploadBufferTarget;
};

BufferInUseRenderTimeCase::BufferInUseRenderTimeCase(Context &context, const char *name, const char *description,
                                                     DrawMethod method, MapFlags mapFlags, TargetBuffer targetBuffer,
                                                     UploadMethod uploadMethod, UploadRange uploadRange,
                                                     UploadBufferTarget uploadTarget)
    : RenderCase<RenderUploadRenderReadDuration>(context, name, description, method)
    , m_targetBuffer(targetBuffer)
    , m_uploadMethod(uploadMethod)
    , m_uploadRange(uploadRange)
    , m_mapFlags(mapFlags)
    , m_uploadBufferTarget(uploadTarget)
{
    DE_ASSERT(m_targetBuffer < TARGETBUFFER_LAST);
    DE_ASSERT(m_uploadMethod < UPLOADMETHOD_LAST);
    DE_ASSERT(m_uploadRange < UPLOADRANGE_LAST);
    DE_ASSERT(m_mapFlags < MAPFLAG_LAST);
    DE_ASSERT(m_uploadBufferTarget < UPLOADBUFFERTARGET_LAST);
}

void BufferInUseRenderTimeCase::init(void)
{
    RenderCase<RenderUploadRenderReadDuration>::init();

    // log
    {
        const char *const targetFunctionName =
            (m_drawMethod == DRAWMETHOD_DRAW_ARRAYS) ? ("drawArrays") : ("drawElements");
        const char *const uploadFunctionName = (m_uploadMethod == UPLOADMETHOD_BUFFER_DATA)     ? ("bufferData") :
                                               (m_uploadMethod == UPLOADMETHOD_BUFFER_SUB_DATA) ? ("bufferSubData") :
                                                                                                  ("mapBufferRange");
        const bool isReferenceCase           = (m_uploadBufferTarget == UPLOADBUFFERTARGET_DIFFERENT_BUFFER);
        tcu::MessageBuilder message(&m_testCtx.getLog());

        message << "Measuring the time used in " << targetFunctionName << " call, a buffer upload, "
                << targetFunctionName
                << " call using the uploaded buffer and readPixels call with different upload sizes.\n";

        if (isReferenceCase)
            message << "Rendering:\n"
                    << "    before test: create and use buffers B and C\n"
                    << "    first draw: render using buffer B\n"
                    << ((m_uploadRange == UPLOADRANGE_FULL)    ? ("    upload: respecify buffer C contents\n") :
                        (m_uploadRange == UPLOADRANGE_PARTIAL) ? ("    upload: modify buffer C contents\n") :
                                                                 (nullptr))
                    << "    second draw: render using buffer C\n"
                    << "    read: readPixels\n";
        else
            message << "Rendering:\n"
                    << "    before test: create and use buffer B\n"
                    << "    first draw: render using buffer B\n"
                    << ((m_uploadRange == UPLOADRANGE_FULL)    ? ("    upload: respecify buffer B contents\n") :
                        (m_uploadRange == UPLOADRANGE_PARTIAL) ? ("    upload: modify buffer B contents\n") :
                                                                 (nullptr))
                    << "    second draw: render using buffer B\n"
                    << "    read: readPixels\n";

        message << "Uploading using " << uploadFunctionName
                << ((m_mapFlags == MAPFLAG_INVALIDATE_RANGE) ? (", flags = MAP_WRITE_BIT | MAP_INVALIDATE_RANGE_BIT") :
                    (m_mapFlags == MAPFLAG_INVALIDATE_BUFFER) ?
                                                               (", flags = MAP_WRITE_BIT | MAP_INVALIDATE_BUFFER_BIT") :
                    (m_mapFlags == MAPFLAG_NONE) ? ("") :
                                                   (nullptr))
                << "\n"
                << getNumSamples() << " test samples. Sample order is randomized.\n"
                << "All samples at even positions (first = 0) are tested before samples at odd positions.\n"
                << "Workload sizes are in the range [" << getMinWorkloadSize() << ",  " << getMaxWorkloadSize()
                << "] vertices "
                << "([" << getHumanReadableByteSize(getMinWorkloadDataSize()) << ","
                << getHumanReadableByteSize(getMaxWorkloadDataSize()) << "] to be processed).\n"
                << "Test result is the approximated processing rate in MiB / s of the second draw call and the "
                   "readPixels call.\n";

        if (isReferenceCase)
            message << "Note! Test result should only be used as a baseline reference result for "
                       "buffer.render_after_upload.draw_modify_draw test group results.";
        else
            message << "Note! Test result may not be useful as is but instead should be compared against the "
                       "buffer.render_after_upload.reference.draw_upload_draw group results.\n";

        message << tcu::TestLog::EndMessage;
    }
}

void BufferInUseRenderTimeCase::runSample(SampleResult &sample)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const glu::Buffer arrayBuffer(m_context.getRenderContext());
    const glu::Buffer indexBuffer(m_context.getRenderContext());
    const glu::Buffer alternativeUploadBuffer(m_context.getRenderContext());
    const int numVertices = getLayeredGridNumVertices(sample.scene);
    tcu::Surface resultSurface(RENDER_AREA_SIZE, RENDER_AREA_SIZE);
    uint64_t startTime;
    uint64_t endTime;
    std::vector<tcu::Vec4> vertexData;
    std::vector<uint32_t> indexData;

    // create data

    generateLayeredGridVertexAttribData4C4V(vertexData, sample.scene);
    if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
        generateLayeredGridIndexData(indexData, sample.scene);

    // make buffers used

    gl.bindBuffer(GL_ARRAY_BUFFER, *arrayBuffer);
    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, *indexBuffer);
    setupVertexAttribs();

    if (m_drawMethod == DRAWMETHOD_DRAW_ARRAYS)
    {
        gl.bufferData(GL_ARRAY_BUFFER, (glw::GLsizeiptr)(vertexData.size() * sizeof(tcu::Vec4)), &vertexData[0],
                      GL_STREAM_DRAW);
        gl.drawArrays(GL_TRIANGLES, 0, numVertices);
    }
    else if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
    {
        gl.bufferData(GL_ARRAY_BUFFER, (glw::GLsizeiptr)(vertexData.size() * sizeof(tcu::Vec4)), &vertexData[0],
                      GL_STREAM_DRAW);
        gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, (glw::GLsizeiptr)(indexData.size() * sizeof(uint32_t)), &indexData[0],
                      GL_STREAM_DRAW);
        gl.drawElements(GL_TRIANGLES, numVertices, GL_UNSIGNED_INT, nullptr);
    }
    else
        DE_ASSERT(false);

    // another pair of buffers for reference case
    if (m_uploadBufferTarget == UPLOADBUFFERTARGET_DIFFERENT_BUFFER)
    {
        if (m_targetBuffer == TARGETBUFFER_VERTEX)
        {
            gl.bindBuffer(GL_ARRAY_BUFFER, *alternativeUploadBuffer);
            gl.bufferData(GL_ARRAY_BUFFER, (glw::GLsizeiptr)(vertexData.size() * sizeof(tcu::Vec4)), &vertexData[0],
                          GL_STREAM_DRAW);

            setupVertexAttribs();
            gl.drawArrays(GL_TRIANGLES, 0, numVertices);
        }
        else if (m_targetBuffer == TARGETBUFFER_INDEX)
        {
            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, *alternativeUploadBuffer);
            gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, (glw::GLsizeiptr)(indexData.size() * sizeof(uint32_t)),
                          &indexData[0], GL_STREAM_DRAW);
            gl.drawElements(GL_TRIANGLES, numVertices, GL_UNSIGNED_INT, nullptr);
        }
        else
            DE_ASSERT(false);

        // restore state
        gl.bindBuffer(GL_ARRAY_BUFFER, *arrayBuffer);
        gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, *indexBuffer);
        setupVertexAttribs();
    }

    waitGLResults();
    GLU_EXPECT_NO_ERROR(gl.getError(), "post buffer prepare");

    gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl.clear(GL_COLOR_BUFFER_BIT);
    waitGLResults();

    tcu::warmupCPU();

    // first draw
    {
        startTime = deGetMicroseconds();

        if (m_drawMethod == DRAWMETHOD_DRAW_ARRAYS)
            gl.drawArrays(GL_TRIANGLES, 0, numVertices);
        else if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
            gl.drawElements(GL_TRIANGLES, numVertices, GL_UNSIGNED_INT, nullptr);
        else
            DE_ASSERT(false);

        endTime = deGetMicroseconds();

        sample.result.duration.firstRenderDuration = endTime - startTime;
    }

    // upload
    {
        glw::GLenum target;
        glw::GLsizeiptr size;
        glw::GLintptr offset = 0;
        const void *source;

        if (m_targetBuffer == TARGETBUFFER_VERTEX && m_uploadRange == UPLOADRANGE_FULL)
        {
            target = GL_ARRAY_BUFFER;
            size   = (glw::GLsizeiptr)(vertexData.size() * sizeof(tcu::Vec4));
            source = &vertexData[0];
        }
        else if (m_targetBuffer == TARGETBUFFER_INDEX && m_uploadRange == UPLOADRANGE_FULL)
        {
            target = GL_ELEMENT_ARRAY_BUFFER;
            size   = (glw::GLsizeiptr)(indexData.size() * sizeof(uint32_t));
            source = &indexData[0];
        }
        else if (m_targetBuffer == TARGETBUFFER_VERTEX && m_uploadRange == UPLOADRANGE_PARTIAL)
        {
            target = GL_ARRAY_BUFFER;
            size   = (glw::GLsizeiptr)deAlign32((int)(vertexData.size() * sizeof(tcu::Vec4)) / 2, 4);
            offset = (glw::GLintptr)deAlign32((int)size / 2, 4);
            source = (const uint8_t *)&vertexData[0] + offset;
        }
        else if (m_targetBuffer == TARGETBUFFER_INDEX && m_uploadRange == UPLOADRANGE_PARTIAL)
        {
            // upload to 25% - 75% range
            target = GL_ELEMENT_ARRAY_BUFFER;
            size   = (glw::GLsizeiptr)deAlign32((int32_t)(indexData.size() * sizeof(uint32_t)) / 2, 4);
            offset = (glw::GLintptr)deAlign32((int)size / 2, 4);
            source = (const uint8_t *)&indexData[0] + offset;
        }
        else
        {
            DE_ASSERT(false);
            return;
        }

        // reference case? don't modify the buffer in use
        if (m_uploadBufferTarget == UPLOADBUFFERTARGET_DIFFERENT_BUFFER)
            gl.bindBuffer(target, *alternativeUploadBuffer);

        startTime = deGetMicroseconds();

        if (m_uploadMethod == UPLOADMETHOD_BUFFER_DATA)
            gl.bufferData(target, size, source, GL_STREAM_DRAW);
        else if (m_uploadMethod == UPLOADMETHOD_BUFFER_SUB_DATA)
            gl.bufferSubData(target, offset, size, source);
        else if (m_uploadMethod == UPLOADMETHOD_MAP_BUFFER_RANGE)
        {
            const int mapFlags =
                (m_mapFlags == MAPFLAG_INVALIDATE_BUFFER) ? (GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT) :
                (m_mapFlags == MAPFLAG_INVALIDATE_RANGE)  ? (GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT) :
                                                            (-1);
            void *mapPtr;
            glw::GLboolean unmapSuccessful;

            mapPtr = gl.mapBufferRange(target, offset, size, mapFlags);
            if (!mapPtr)
                throw tcu::Exception("MapBufferRange returned NULL");

            deMemcpy(mapPtr, source, (int)size);

            // if unmapping fails, just try again later
            unmapSuccessful = gl.unmapBuffer(target);
            if (!unmapSuccessful)
                throw UnmapFailureError();
        }
        else
            DE_ASSERT(false);

        endTime = deGetMicroseconds();

        sample.result.uploadedDataSize        = (int)size;
        sample.result.duration.uploadDuration = endTime - startTime;
    }

    // second draw
    {
        // Source vertex data from alternative buffer in refernce case
        if (m_uploadBufferTarget == UPLOADBUFFERTARGET_DIFFERENT_BUFFER && m_targetBuffer == TARGETBUFFER_VERTEX)
            setupVertexAttribs();

        startTime = deGetMicroseconds();

        if (m_drawMethod == DRAWMETHOD_DRAW_ARRAYS)
            gl.drawArrays(GL_TRIANGLES, 0, numVertices);
        else if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
            gl.drawElements(GL_TRIANGLES, numVertices, GL_UNSIGNED_INT, nullptr);
        else
            DE_ASSERT(false);

        endTime = deGetMicroseconds();

        sample.result.duration.secondRenderDuration = endTime - startTime;
    }

    // read
    {
        startTime = deGetMicroseconds();
        glu::readPixels(m_context.getRenderContext(), 0, 0, resultSurface.getAccess());
        endTime = deGetMicroseconds();

        sample.result.duration.readDuration = endTime - startTime;
    }

    // set results

    sample.result.renderDataSize = getVertexDataSize() * sample.result.numVertices;

    sample.result.duration.renderReadDuration =
        sample.result.duration.secondRenderDuration + sample.result.duration.readDuration;
    sample.result.duration.totalDuration =
        sample.result.duration.firstRenderDuration + sample.result.duration.uploadDuration +
        sample.result.duration.secondRenderDuration + sample.result.duration.readDuration;
    sample.result.duration.fitResponseDuration = sample.result.duration.renderReadDuration;
}

class UploadWaitDrawCase : public RenderPerformanceTestBase
{
public:
    struct Sample
    {
        int numFrames;
        uint64_t uploadCallEndTime;
    };
    struct Result
    {
        uint64_t uploadDuration;
        uint64_t renderDuration;
        uint64_t readDuration;
        uint64_t renderReadDuration;

        uint64_t timeBeforeUse;
    };

    UploadWaitDrawCase(Context &context, const char *name, const char *description, DrawMethod drawMethod,
                       TargetBuffer targetBuffer, UploadMethod uploadMethod, BufferState bufferState);
    ~UploadWaitDrawCase(void);

private:
    void init(void);
    void deinit(void);
    IterateResult iterate(void);

    void uploadBuffer(Sample &sample, Result &result);
    void drawFromBuffer(Sample &sample, Result &result);
    void reuseAndDeleteBuffer(void);
    void logAndSetTestResult(void);
    void logSamples(void);
    void drawMisc(void);
    int findStabilizationSample(uint64_t Result::*target, const char *description);
    bool checkSampleTemporalStability(uint64_t Result::*target, const char *description);

    const DrawMethod m_drawMethod;
    const TargetBuffer m_targetBuffer;
    const UploadMethod m_uploadMethod;
    const BufferState m_bufferState;

    const int m_numSamplesPerSwap;
    const int m_numMaxSwaps;

    int m_frameNdx;
    int m_sampleNdx;
    int m_numVertices;

    std::vector<tcu::Vec4> m_vertexData;
    std::vector<uint32_t> m_indexData;
    std::vector<Sample> m_samples;
    std::vector<Result> m_results;
    std::vector<int> m_iterationOrder;

    uint32_t m_vertexBuffer;
    uint32_t m_indexBuffer;
    uint32_t m_miscBuffer;
    int m_numMiscVertices;
};

UploadWaitDrawCase::UploadWaitDrawCase(Context &context, const char *name, const char *description,
                                       DrawMethod drawMethod, TargetBuffer targetBuffer, UploadMethod uploadMethod,
                                       BufferState bufferState)
    : RenderPerformanceTestBase(context, name, description)
    , m_drawMethod(drawMethod)
    , m_targetBuffer(targetBuffer)
    , m_uploadMethod(uploadMethod)
    , m_bufferState(bufferState)
    , m_numSamplesPerSwap(10)
    , m_numMaxSwaps(4)
    , m_frameNdx(0)
    , m_sampleNdx(0)
    , m_numVertices(-1)
    , m_vertexBuffer(0)
    , m_indexBuffer(0)
    , m_miscBuffer(0)
    , m_numMiscVertices(-1)
{
}

UploadWaitDrawCase::~UploadWaitDrawCase(void)
{
    deinit();
}

void UploadWaitDrawCase::init(void)
{
    const glw::Functions &gl       = m_context.getRenderContext().getFunctions();
    const int vertexAttribSize     = (int)sizeof(tcu::Vec4) * 2; // color4, position4
    const int vertexIndexSize      = (int)sizeof(uint32_t);
    const int vertexUploadDataSize = (m_targetBuffer == TARGETBUFFER_VERTEX) ? (vertexAttribSize) : (vertexIndexSize);

    RenderPerformanceTestBase::init();

    // requirements

    if (m_context.getRenderTarget().getWidth() < RENDER_AREA_SIZE ||
        m_context.getRenderTarget().getHeight() < RENDER_AREA_SIZE)
        throw tcu::NotSupportedError("Test case requires " + de::toString<int>(RENDER_AREA_SIZE) + "x" +
                                     de::toString<int>(RENDER_AREA_SIZE) + " render target");

    // gl state

    gl.viewport(0, 0, RENDER_AREA_SIZE, RENDER_AREA_SIZE);

    // enable bleding to prevent grid layers from being discarded

    gl.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl.blendEquation(GL_FUNC_ADD);
    gl.enable(GL_BLEND);

    // scene

    {
        LayeredGridSpec scene;

        // create ~8MB workload with similar characteristics as in the other test
        // => makes comparison to other results more straightforward
        scene.gridWidth  = 93;
        scene.gridHeight = 93;
        scene.gridLayers = 5;

        generateLayeredGridVertexAttribData4C4V(m_vertexData, scene);
        generateLayeredGridIndexData(m_indexData, scene);
        m_numVertices = getLayeredGridNumVertices(scene);
    }

    // buffers

    if (m_bufferState == BUFFERSTATE_NEW)
    {
        if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
        {
            // reads from two buffers, prepare the static buffer

            if (m_targetBuffer == TARGETBUFFER_VERTEX)
            {
                // index buffer is static, use another vertex buffer to keep original buffer in unused state
                const glu::Buffer vertexCopyBuffer(m_context.getRenderContext());

                gl.genBuffers(1, &m_indexBuffer);
                gl.bindBuffer(GL_ARRAY_BUFFER, *vertexCopyBuffer);
                gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
                gl.bufferData(GL_ARRAY_BUFFER, (glw::GLsizeiptr)(m_vertexData.size() * sizeof(tcu::Vec4)),
                              &m_vertexData[0], GL_STATIC_DRAW);
                gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, (glw::GLsizeiptr)(m_indexData.size() * sizeof(uint32_t)),
                              &m_indexData[0], GL_STATIC_DRAW);

                setupVertexAttribs();
                gl.drawElements(GL_TRIANGLES, m_numVertices, GL_UNSIGNED_INT, nullptr);
            }
            else if (m_targetBuffer == TARGETBUFFER_INDEX)
            {
                // vertex buffer is static
                gl.genBuffers(1, &m_vertexBuffer);
                gl.bindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
                gl.bufferData(GL_ARRAY_BUFFER, (glw::GLsizeiptr)(m_vertexData.size() * sizeof(tcu::Vec4)),
                              &m_vertexData[0], GL_STATIC_DRAW);

                setupVertexAttribs();
                gl.drawArrays(GL_TRIANGLES, 0, m_numVertices);
            }
            else
                DE_ASSERT(false);
        }
    }
    else if (m_bufferState == BUFFERSTATE_EXISTING)
    {
        const glw::GLenum vertexUsage = (m_targetBuffer == TARGETBUFFER_VERTEX) ? (GL_STATIC_DRAW) : (GL_STATIC_DRAW);
        const glw::GLenum indexUsage  = (m_targetBuffer == TARGETBUFFER_INDEX) ? (GL_STATIC_DRAW) : (GL_STATIC_DRAW);

        gl.genBuffers(1, &m_vertexBuffer);
        gl.bindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
        gl.bufferData(GL_ARRAY_BUFFER, (glw::GLsizeiptr)(m_vertexData.size() * sizeof(tcu::Vec4)), &m_vertexData[0],
                      vertexUsage);

        if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
        {
            gl.genBuffers(1, &m_indexBuffer);
            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
            gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, (glw::GLsizeiptr)(m_indexData.size() * sizeof(uint32_t)),
                          &m_indexData[0], indexUsage);
        }

        setupVertexAttribs();

        if (m_drawMethod == DRAWMETHOD_DRAW_ARRAYS)
            gl.drawArrays(GL_TRIANGLES, 0, m_numVertices);
        else if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
            gl.drawElements(GL_TRIANGLES, m_numVertices, GL_UNSIGNED_INT, nullptr);
        else
            DE_ASSERT(false);
    }
    else
        DE_ASSERT(false);

    // misc draw buffer
    {
        std::vector<tcu::Vec4> vertexData;
        LayeredGridSpec scene;

        // create ~1.5MB workload with similar characteristics
        scene.gridWidth  = 40;
        scene.gridHeight = 40;
        scene.gridLayers = 5;

        generateLayeredGridVertexAttribData4C4V(vertexData, scene);

        gl.genBuffers(1, &m_miscBuffer);
        gl.bindBuffer(GL_ARRAY_BUFFER, m_miscBuffer);
        gl.bufferData(GL_ARRAY_BUFFER, (glw::GLsizeiptr)(sizeof(tcu::Vec4) * vertexData.size()), &vertexData[0],
                      GL_STATIC_DRAW);

        m_numMiscVertices = getLayeredGridNumVertices(scene);
    }

    // iterations
    {
        m_samples.resize((m_numMaxSwaps + 1) * m_numSamplesPerSwap);
        m_results.resize((m_numMaxSwaps + 1) * m_numSamplesPerSwap);

        for (int numSwaps = 0; numSwaps <= m_numMaxSwaps; ++numSwaps)
            for (int sampleNdx = 0; sampleNdx < m_numSamplesPerSwap; ++sampleNdx)
            {
                const int index = numSwaps * m_numSamplesPerSwap + sampleNdx;

                m_samples[index].numFrames = numSwaps;
            }

        m_iterationOrder.resize(m_samples.size());
        generateTwoPassRandomIterationOrder(m_iterationOrder, (int)m_samples.size());
    }

    // log
    m_testCtx.getLog()
        << tcu::TestLog::Message << "Measuring time used in "
        << ((m_drawMethod == DRAWMETHOD_DRAW_ARRAYS) ? ("drawArrays") : ("drawElements")) << " and readPixels call.\n"
        << "Drawing using a buffer that has been uploaded N frames ago. Testing with N within range [0, "
        << m_numMaxSwaps << "].\n"
        << "Uploaded buffer is a " << ((m_targetBuffer == TARGETBUFFER_VERTEX) ? ("vertex attribute") : ("index"))
        << " buffer.\n"
        << "Uploading using "
        << ((m_uploadMethod == UPLOADMETHOD_BUFFER_DATA) ?
                ("bufferData") :
            (m_uploadMethod == UPLOADMETHOD_BUFFER_SUB_DATA) ?
                ("bufferSubData") :
            (m_uploadMethod == UPLOADMETHOD_MAP_BUFFER_RANGE) ?
                ("mapBufferRange, flags = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | "
                 "GL_MAP_UNSYNCHRONIZED_BIT") :
                (nullptr))
        << "\n"
        << "Upload size is " << getHumanReadableByteSize(m_numVertices * vertexUploadDataSize) << ".\n"
        << ((m_bufferState == BUFFERSTATE_EXISTING) ? ("All test samples use the same buffer object.\n") : (""))
        << "Test result is the number of frames (swaps) required for the render time to stabilize.\n"
        << "Assuming combined time used in the draw call and readPixels call is stabilizes to a constant value.\n"
        << tcu::TestLog::EndMessage;
}

void UploadWaitDrawCase::deinit(void)
{
    RenderPerformanceTestBase::deinit();

    if (m_vertexBuffer)
    {
        m_context.getRenderContext().getFunctions().deleteBuffers(1, &m_vertexBuffer);
        m_vertexBuffer = 0;
    }
    if (m_indexBuffer)
    {
        m_context.getRenderContext().getFunctions().deleteBuffers(1, &m_indexBuffer);
        m_indexBuffer = 0;
    }
    if (m_miscBuffer)
    {
        m_context.getRenderContext().getFunctions().deleteBuffers(1, &m_miscBuffer);
        m_miscBuffer = 0;
    }
}

UploadWaitDrawCase::IterateResult UploadWaitDrawCase::iterate(void)
{
    const glw::Functions &gl             = m_context.getRenderContext().getFunctions();
    const int betweenIterationFrameCount = 5; // draw misc between test samples
    const int frameNdx                   = m_frameNdx++;
    const int currentSampleNdx           = m_iterationOrder[m_sampleNdx];

    // Simulate work for about 8ms
    busyWait(8000);

    // Busywork rendering during unused frames
    if (frameNdx != m_samples[currentSampleNdx].numFrames)
    {
        // draw similar from another buffer
        drawMisc();
    }

    if (frameNdx == 0)
    {
        // upload and start the clock
        uploadBuffer(m_samples[currentSampleNdx], m_results[currentSampleNdx]);
    }

    if (frameNdx ==
        m_samples[currentSampleNdx].numFrames) // \note: not else if, m_samples[currentSampleNdx].numFrames can be 0
    {
        // draw using the uploaded buffer
        drawFromBuffer(m_samples[currentSampleNdx], m_results[currentSampleNdx]);

        // re-use buffer for something else to make sure test iteration do not affect each other
        if (m_bufferState == BUFFERSTATE_NEW)
            reuseAndDeleteBuffer();
    }
    else if (frameNdx == m_samples[currentSampleNdx].numFrames + betweenIterationFrameCount)
    {
        // next sample
        ++m_sampleNdx;
        m_frameNdx = 0;
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "post-iterate");

    if (m_sampleNdx < (int)m_samples.size())
        return CONTINUE;

    logAndSetTestResult();
    return STOP;
}

void UploadWaitDrawCase::uploadBuffer(Sample &sample, Result &result)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    uint64_t startTime;
    uint64_t endTime;
    glw::GLenum target;
    glw::GLsizeiptr size;
    const void *source;

    // data source

    if (m_targetBuffer == TARGETBUFFER_VERTEX)
    {
        DE_ASSERT((m_vertexBuffer == 0) == (m_bufferState == BUFFERSTATE_NEW));

        target = GL_ARRAY_BUFFER;
        size   = (glw::GLsizeiptr)(m_vertexData.size() * sizeof(tcu::Vec4));
        source = &m_vertexData[0];
    }
    else if (m_targetBuffer == TARGETBUFFER_INDEX)
    {
        DE_ASSERT((m_indexBuffer == 0) == (m_bufferState == BUFFERSTATE_NEW));

        target = GL_ELEMENT_ARRAY_BUFFER;
        size   = (glw::GLsizeiptr)(m_indexData.size() * sizeof(uint32_t));
        source = &m_indexData[0];
    }
    else
    {
        DE_ASSERT(false);
        return;
    }

    // gen buffer

    if (m_bufferState == BUFFERSTATE_NEW)
    {
        if (m_targetBuffer == TARGETBUFFER_VERTEX)
        {
            gl.genBuffers(1, &m_vertexBuffer);
            gl.bindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
        }
        else if (m_targetBuffer == TARGETBUFFER_INDEX)
        {
            gl.genBuffers(1, &m_indexBuffer);
            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
        }
        else
            DE_ASSERT(false);

        if (m_uploadMethod == UPLOADMETHOD_BUFFER_SUB_DATA || m_uploadMethod == UPLOADMETHOD_MAP_BUFFER_RANGE)
        {
            gl.bufferData(target, size, nullptr, GL_STATIC_DRAW);
        }
    }
    else if (m_bufferState == BUFFERSTATE_EXISTING)
    {
        if (m_targetBuffer == TARGETBUFFER_VERTEX)
            gl.bindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
        else if (m_targetBuffer == TARGETBUFFER_INDEX)
            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
        else
            DE_ASSERT(false);
    }
    else
        DE_ASSERT(false);

    // upload

    startTime = deGetMicroseconds();

    if (m_uploadMethod == UPLOADMETHOD_BUFFER_DATA)
        gl.bufferData(target, size, source, GL_STATIC_DRAW);
    else if (m_uploadMethod == UPLOADMETHOD_BUFFER_SUB_DATA)
        gl.bufferSubData(target, 0, size, source);
    else if (m_uploadMethod == UPLOADMETHOD_MAP_BUFFER_RANGE)
    {
        void *mapPtr;
        glw::GLboolean unmapSuccessful;

        mapPtr = gl.mapBufferRange(target, 0, size,
                                   GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
        if (!mapPtr)
            throw tcu::Exception("MapBufferRange returned NULL");

        deMemcpy(mapPtr, source, (int)size);

        // if unmapping fails, just try again later
        unmapSuccessful = gl.unmapBuffer(target);
        if (!unmapSuccessful)
            throw UnmapFailureError();
    }
    else
        DE_ASSERT(false);

    endTime = deGetMicroseconds();

    sample.uploadCallEndTime = endTime;
    result.uploadDuration    = endTime - startTime;
}

void UploadWaitDrawCase::drawFromBuffer(Sample &sample, Result &result)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    tcu::Surface resultSurface(RENDER_AREA_SIZE, RENDER_AREA_SIZE);
    uint64_t startTime;
    uint64_t endTime;

    DE_ASSERT(m_vertexBuffer != 0);
    if (m_drawMethod == DRAWMETHOD_DRAW_ARRAYS)
        DE_ASSERT(m_indexBuffer == 0);
    else if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
        DE_ASSERT(m_indexBuffer != 0);
    else
        DE_ASSERT(false);

    // draw
    {
        gl.bindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
        if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);

        setupVertexAttribs();

        // microseconds passed since return from upload call
        result.timeBeforeUse = deGetMicroseconds() - sample.uploadCallEndTime;

        startTime = deGetMicroseconds();

        if (m_drawMethod == DRAWMETHOD_DRAW_ARRAYS)
            gl.drawArrays(GL_TRIANGLES, 0, m_numVertices);
        else if (m_drawMethod == DRAWMETHOD_DRAW_ELEMENTS)
            gl.drawElements(GL_TRIANGLES, m_numVertices, GL_UNSIGNED_INT, nullptr);
        else
            DE_ASSERT(false);

        endTime = deGetMicroseconds();

        result.renderDuration = endTime - startTime;
    }

    // read
    {
        startTime = deGetMicroseconds();
        glu::readPixels(m_context.getRenderContext(), 0, 0, resultSurface.getAccess());
        endTime = deGetMicroseconds();

        result.readDuration = endTime - startTime;
    }

    result.renderReadDuration = result.renderDuration + result.readDuration;
}

void UploadWaitDrawCase::reuseAndDeleteBuffer(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    if (m_targetBuffer == TARGETBUFFER_INDEX)
    {
        // respecify and delete index buffer
        static const uint32_t indices[3] = {1, 3, 8};

        DE_ASSERT(m_indexBuffer != 0);

        gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        gl.drawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);
        gl.deleteBuffers(1, &m_indexBuffer);
        m_indexBuffer = 0;
    }
    else if (m_targetBuffer == TARGETBUFFER_VERTEX)
    {
        // respecify and delete vertex buffer
        static const tcu::Vec4 coloredTriangle[6] = {
            tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),  tcu::Vec4(-0.4f, -0.4f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
            tcu::Vec4(-0.2f, 0.4f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f),   tcu::Vec4(0.8f, -0.1f, 0.0f, 1.0f),
        };

        DE_ASSERT(m_vertexBuffer != 0);

        gl.bufferData(GL_ARRAY_BUFFER, sizeof(coloredTriangle), coloredTriangle, GL_STATIC_DRAW);
        gl.drawArrays(GL_TRIANGLES, 0, 3);
        gl.deleteBuffers(1, &m_vertexBuffer);
        m_vertexBuffer = 0;
    }

    waitGLResults();
}

void UploadWaitDrawCase::logAndSetTestResult(void)
{
    int uploadStabilization;
    int renderReadStabilization;
    int renderStabilization;
    int readStabilization;
    bool temporallyStable;

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Samples", "Result samples");
        logSamples();
    }

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Stabilization", "Sample stability");

        // log stabilization points
        renderReadStabilization = findStabilizationSample(&Result::renderReadDuration, "Combined draw and read");
        uploadStabilization     = findStabilizationSample(&Result::uploadDuration, "Upload time");
        renderStabilization     = findStabilizationSample(&Result::renderDuration, "Draw call time");
        readStabilization       = findStabilizationSample(&Result::readDuration, "ReadPixels time");

        temporallyStable = true;
        temporallyStable &= checkSampleTemporalStability(&Result::renderReadDuration, "Combined draw and read");
        temporallyStable &= checkSampleTemporalStability(&Result::uploadDuration, "Upload time");
        temporallyStable &= checkSampleTemporalStability(&Result::renderDuration, "Draw call time");
        temporallyStable &= checkSampleTemporalStability(&Result::readDuration, "ReadPixels time");
    }

    {
        const tcu::ScopedLogSection section(m_testCtx.getLog(), "Results", "Results");

        // Check result sanily
        if (uploadStabilization != 0)
            m_testCtx.getLog() << tcu::TestLog::Message
                               << "Warning! Upload times are not stable, test result may not be accurate."
                               << tcu::TestLog::EndMessage;
        if (!temporallyStable)
            m_testCtx.getLog() << tcu::TestLog::Message
                               << "Warning! Time samples do not seem to be temporally stable, sample times seem to "
                                  "drift to one direction during test execution."
                               << tcu::TestLog::EndMessage;

        // render & read
        if (renderReadStabilization == -1)
            m_testCtx.getLog() << tcu::TestLog::Message
                               << "Combined time used in draw call and ReadPixels did not stabilize."
                               << tcu::TestLog::EndMessage;
        else
            m_testCtx.getLog() << tcu::TestLog::Integer(
                "RenderReadStabilizationPoint", "Combined draw call and ReadPixels call time stabilization time",
                "frames", QP_KEY_TAG_TIME, renderReadStabilization);

        // draw call
        if (renderStabilization == -1)
            m_testCtx.getLog() << tcu::TestLog::Message << "Time used in draw call did not stabilize."
                               << tcu::TestLog::EndMessage;
        else
            m_testCtx.getLog() << tcu::TestLog::Integer("DrawCallStabilizationPoint",
                                                        "Draw call time stabilization time", "frames", QP_KEY_TAG_TIME,
                                                        renderStabilization);

        // readpixels
        if (readStabilization == -1)
            m_testCtx.getLog() << tcu::TestLog::Message << "Time used in ReadPixels did not stabilize."
                               << tcu::TestLog::EndMessage;
        else
            m_testCtx.getLog() << tcu::TestLog::Integer("ReadPixelsStabilizationPoint",
                                                        "ReadPixels call time stabilization time", "frames",
                                                        QP_KEY_TAG_TIME, readStabilization);

        // Report renderReadStabilization
        if (renderReadStabilization != -1)
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::toString(renderReadStabilization).c_str());
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::toString(m_numMaxSwaps).c_str()); // don't report -1
    }
}

void UploadWaitDrawCase::logSamples(void)
{
    // Inverse m_iterationOrder

    std::vector<int> runOrder(m_iterationOrder.size());
    for (int ndx = 0; ndx < (int)m_iterationOrder.size(); ++ndx)
        runOrder[m_iterationOrder[ndx]] = ndx;

    // Log samples

    m_testCtx.getLog() << tcu::TestLog::SampleList("Samples", "Samples") << tcu::TestLog::SampleInfo
                       << tcu::TestLog::ValueInfo("NumSwaps", "SwapBuffers before use", "",
                                                  QP_SAMPLE_VALUE_TAG_PREDICTOR)
                       << tcu::TestLog::ValueInfo("Delay", "Time before use", "us", QP_SAMPLE_VALUE_TAG_PREDICTOR)
                       << tcu::TestLog::ValueInfo("RunOrder", "Sample run order", "", QP_SAMPLE_VALUE_TAG_PREDICTOR)
                       << tcu::TestLog::ValueInfo("DrawReadTime", "Draw call and ReadPixels time", "us",
                                                  QP_SAMPLE_VALUE_TAG_RESPONSE)
                       << tcu::TestLog::ValueInfo("TotalTime", "Total time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
                       << tcu::TestLog::ValueInfo("Upload time", "Upload time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
                       << tcu::TestLog::ValueInfo("DrawCallTime", "Draw call time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
                       << tcu::TestLog::ValueInfo("ReadTime", "ReadPixels time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
                       << tcu::TestLog::EndSampleInfo;

    for (int sampleNdx = 0; sampleNdx < (int)m_samples.size(); ++sampleNdx)
        m_testCtx.getLog() << tcu::TestLog::Sample << m_samples[sampleNdx].numFrames
                           << (int)m_results[sampleNdx].timeBeforeUse << runOrder[sampleNdx]
                           << (int)m_results[sampleNdx].renderReadDuration
                           << (int)(m_results[sampleNdx].renderReadDuration + m_results[sampleNdx].uploadDuration)
                           << (int)m_results[sampleNdx].uploadDuration << (int)m_results[sampleNdx].renderDuration
                           << (int)m_results[sampleNdx].readDuration << tcu::TestLog::EndSample;

    m_testCtx.getLog() << tcu::TestLog::EndSampleList;
}

void UploadWaitDrawCase::drawMisc(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.bindBuffer(GL_ARRAY_BUFFER, m_miscBuffer);
    setupVertexAttribs();
    gl.drawArrays(GL_TRIANGLES, 0, m_numMiscVertices);
}

struct DistributionCompareResult
{
    bool equal;
    float standardDeviations;
};

template <typename Comparer>
static float sumOfRanks(const std::vector<uint64_t> &testSamples, const std::vector<uint64_t> &allSamples,
                        const Comparer &comparer)
{
    float sum = 0;

    for (int sampleNdx = 0; sampleNdx < (int)testSamples.size(); ++sampleNdx)
    {
        const uint64_t testSample = testSamples[sampleNdx];
        const int lowerIndex =
            (int)(std::lower_bound(allSamples.begin(), allSamples.end(), testSample, comparer) - allSamples.begin());
        const int upperIndex =
            (int)(std::upper_bound(allSamples.begin(), allSamples.end(), testSample, comparer) - allSamples.begin());
        const int lowerRank      = lowerIndex + 1; // convert zero-indexed to rank
        const int upperRank      = upperIndex;     // convert zero-indexed to rank, upperIndex is last equal + 1
        const float rankMidpoint = (float)(lowerRank + upperRank) / 2.0f;

        sum += rankMidpoint;
    }

    return sum;
}

template <typename Comparer>
static DistributionCompareResult distributionCompare(const std::vector<uint64_t> &orderedObservationsA,
                                                     const std::vector<uint64_t> &orderedObservationsB,
                                                     const Comparer &comparer)
{
    // Mann-Whitney U test

    const int n1 = (int)orderedObservationsA.size();
    const int n2 = (int)orderedObservationsB.size();
    std::vector<uint64_t> allSamples(n1 + n2);

    std::copy(orderedObservationsA.begin(), orderedObservationsA.end(), allSamples.begin());
    std::copy(orderedObservationsB.begin(), orderedObservationsB.end(), allSamples.begin() + n1);
    std::sort(allSamples.begin(), allSamples.end());

    {
        const float R1 = sumOfRanks(orderedObservationsA, allSamples, comparer);

        const float U1 = (float)(n1 * n2 + n1 * (n1 + 1) / 2) - R1;
        const float U2 = (float)(n1 * n2) - U1;
        const float U  = de::min(U1, U2);

        // \note: sample sizes might not be large enough to expect normal distribution but we do it anyway

        const float mU     = (float)(n1 * n2) / 2.0f;
        const float sigmaU = deFloatSqrt((float)(n1 * n2 * (n1 + n2 + 1)) / 12.0f);
        const float z      = (U - mU) / sigmaU;

        DistributionCompareResult result;

        result.equal              = (de::abs(z) <= 1.96f); // accept within 95% confidence interval
        result.standardDeviations = z;

        return result;
    }
}

template <typename T>
struct ThresholdComparer
{
    float relativeThreshold;
    T absoluteThreshold;

    bool operator()(const T &a, const T &b) const
    {
        const float diff = de::abs((float)a - (float)b);

        // thresholds
        if (diff <= (float)absoluteThreshold)
            return false;
        if (diff <= float(a) * relativeThreshold || diff <= float(b) * relativeThreshold)
            return false;

        // cmp
        return a < b;
    }
};

int UploadWaitDrawCase::findStabilizationSample(uint64_t UploadWaitDrawCase::Result::*target, const char *description)
{
    std::vector<std::vector<uint64_t>> sampleObservations(m_numMaxSwaps + 1);
    ThresholdComparer<uint64_t> comparer;

    comparer.relativeThreshold = 0.15f; // 15%
    comparer.absoluteThreshold = 100;   // (us), assumed sampling precision

    // get observations and order them

    for (int swapNdx = 0; swapNdx <= m_numMaxSwaps; ++swapNdx)
    {
        int insertNdx = 0;

        sampleObservations[swapNdx].resize(m_numSamplesPerSwap);

        for (int ndx = 0; ndx < (int)m_samples.size(); ++ndx)
            if (m_samples[ndx].numFrames == swapNdx)
                sampleObservations[swapNdx][insertNdx++] = m_results[ndx].*target;

        DE_ASSERT(insertNdx == m_numSamplesPerSwap);

        std::sort(sampleObservations[swapNdx].begin(), sampleObservations[swapNdx].end());
    }

    // find stabilization point

    for (int sampleNdx = m_numMaxSwaps - 1; sampleNdx != -1; --sampleNdx)
    {
        // Distribution is equal to all following distributions
        for (int cmpTargetDistribution = sampleNdx + 1; cmpTargetDistribution <= m_numMaxSwaps; ++cmpTargetDistribution)
        {
            // Stable section ends here?
            const DistributionCompareResult result =
                distributionCompare(sampleObservations[sampleNdx], sampleObservations[cmpTargetDistribution], comparer);
            if (!result.equal)
            {
                // Last two samples are not equal? Samples never stabilized
                if (sampleNdx == m_numMaxSwaps - 1)
                {
                    m_testCtx.getLog() << tcu::TestLog::Message << description << ": Samples with swap count "
                                       << sampleNdx << " and " << cmpTargetDistribution
                                       << " do not seem to have the same distribution:\n"
                                       << "\tDifference in standard deviations: " << result.standardDeviations << "\n"
                                       << "\tSwap count " << sampleNdx
                                       << " median: " << linearSample(sampleObservations[sampleNdx], 0.5f) << "\n"
                                       << "\tSwap count " << cmpTargetDistribution
                                       << " median: " << linearSample(sampleObservations[cmpTargetDistribution], 0.5f)
                                       << "\n"
                                       << tcu::TestLog::EndMessage;
                    return -1;
                }
                else
                {
                    m_testCtx.getLog() << tcu::TestLog::Message << description << ": Samples with swap count "
                                       << sampleNdx << " and " << cmpTargetDistribution
                                       << " do not seem to have the same distribution:\n"
                                       << "\tSamples with swap count " << sampleNdx
                                       << " are not part of the tail of stable results.\n"
                                       << "\tDifference in standard deviations: " << result.standardDeviations << "\n"
                                       << "\tSwap count " << sampleNdx
                                       << " median: " << linearSample(sampleObservations[sampleNdx], 0.5f) << "\n"
                                       << "\tSwap count " << cmpTargetDistribution
                                       << " median: " << linearSample(sampleObservations[cmpTargetDistribution], 0.5f)
                                       << "\n"
                                       << tcu::TestLog::EndMessage;

                    return sampleNdx + 1;
                }
            }
        }
    }

    m_testCtx.getLog() << tcu::TestLog::Message << description << ": All samples seem to have the same distribution"
                       << tcu::TestLog::EndMessage;

    // all distributions equal
    return 0;
}

bool UploadWaitDrawCase::checkSampleTemporalStability(uint64_t UploadWaitDrawCase::Result::*target,
                                                      const char *description)
{
    // Try to find correlation with sample order and sample times

    const int numDataPoints = (int)m_iterationOrder.size();
    std::vector<tcu::Vec2> dataPoints(m_iterationOrder.size());
    LineParametersWithConfidence lineFit;

    for (int ndx = 0; ndx < (int)m_iterationOrder.size(); ++ndx)
    {
        dataPoints[m_iterationOrder[ndx]].x() = (float)ndx;
        dataPoints[m_iterationOrder[ndx]].y() = (float)(m_results[m_iterationOrder[ndx]].*target);
    }

    lineFit = theilSenSiegelLinearRegression(dataPoints, 0.6f);

    // Difference of more than 25% of the offset along the whole sample range
    if (de::abs(lineFit.coefficient) * (float)numDataPoints > de::abs(lineFit.offset) * 0.25f)
    {
        m_testCtx.getLog() << tcu::TestLog::Message << description
                           << ": Correlation with data point observation order and result time. Results are not "
                              "temporally stable, observations are not independent.\n"
                           << "\tCoefficient: " << lineFit.coefficient << " (us / observation)\n"
                           << tcu::TestLog::EndMessage;

        return false;
    }
    else
        return true;
}

} // namespace

BufferDataUploadTests::BufferDataUploadTests(Context &context)
    : TestCaseGroup(context, "data_upload", "Buffer data upload performance tests")
{
}

BufferDataUploadTests::~BufferDataUploadTests(void)
{
}

void BufferDataUploadTests::init(void)
{
    static const struct BufferUsage
    {
        const char *name;
        uint32_t usage;
        bool primaryUsage;
    } bufferUsages[] = {
        {"stream_draw", GL_STREAM_DRAW, true},    {"stream_read", GL_STREAM_READ, false},
        {"stream_copy", GL_STREAM_COPY, false},   {"static_draw", GL_STATIC_DRAW, true},
        {"static_read", GL_STATIC_READ, false},   {"static_copy", GL_STATIC_COPY, false},
        {"dynamic_draw", GL_DYNAMIC_DRAW, true},  {"dynamic_read", GL_DYNAMIC_READ, false},
        {"dynamic_copy", GL_DYNAMIC_COPY, false},
    };

    tcu::TestCaseGroup *const referenceGroup = new tcu::TestCaseGroup(m_testCtx, "reference", "Reference functions");
    tcu::TestCaseGroup *const functionCallGroup =
        new tcu::TestCaseGroup(m_testCtx, "function_call", "Function call timing");
    tcu::TestCaseGroup *const modifyAfterUseGroup =
        new tcu::TestCaseGroup(m_testCtx, "modify_after_use", "Function call time after buffer has been used");
    tcu::TestCaseGroup *const renderAfterUploadGroup = new tcu::TestCaseGroup(
        m_testCtx, "render_after_upload", "Function call time of draw commands after buffer has been modified");

    addChild(referenceGroup);
    addChild(functionCallGroup);
    addChild(modifyAfterUseGroup);
    addChild(renderAfterUploadGroup);

    // .reference
    {
        static const struct BufferSizeRange
        {
            const char *name;
            int minBufferSize;
            int maxBufferSize;
            int numSamples;
            bool largeBuffersCase;
        } sizeRanges[] = {
            {"small_buffers", 0, 1 << 18, 64, false},      // !< 0kB - 256kB
            {"large_buffers", 1 << 18, 1 << 24, 32, true}, // !< 256kB - 16MB
        };

        for (int bufferSizeRangeNdx = 0; bufferSizeRangeNdx < DE_LENGTH_OF_ARRAY(sizeRanges); ++bufferSizeRangeNdx)
        {
            referenceGroup->addChild(new ReferenceMemcpyCase(
                m_context, std::string("memcpy_").append(sizeRanges[bufferSizeRangeNdx].name).c_str(),
                "Test memcpy performance", sizeRanges[bufferSizeRangeNdx].minBufferSize,
                sizeRanges[bufferSizeRangeNdx].maxBufferSize, sizeRanges[bufferSizeRangeNdx].numSamples,
                sizeRanges[bufferSizeRangeNdx].largeBuffersCase));
        }
    }

    // .function_call
    {
        const int minBufferSize  = 0;       // !< 0kiB
        const int maxBufferSize  = 1 << 24; // !< 16MiB
        const int numDataSamples = 25;
        const int numMapSamples  = 25;

        tcu::TestCaseGroup *const bufferDataMethodGroup =
            new tcu::TestCaseGroup(m_testCtx, "buffer_data", "Use glBufferData");
        tcu::TestCaseGroup *const bufferSubDataMethodGroup =
            new tcu::TestCaseGroup(m_testCtx, "buffer_sub_data", "Use glBufferSubData");
        tcu::TestCaseGroup *const mapBufferRangeMethodGroup =
            new tcu::TestCaseGroup(m_testCtx, "map_buffer_range", "Use glMapBufferRange");

        functionCallGroup->addChild(bufferDataMethodGroup);
        functionCallGroup->addChild(bufferSubDataMethodGroup);
        functionCallGroup->addChild(mapBufferRangeMethodGroup);

        // .buffer_data
        {
            static const struct TargetCase
            {
                tcu::TestCaseGroup *group;
                BufferDataUploadCase::CaseType caseType;
                bool allUsages;
            } targetCases[] = {
                {new tcu::TestCaseGroup(m_testCtx, "new_buffer", "Target new buffer"),
                 BufferDataUploadCase::CASE_NEW_BUFFER, true},
                {new tcu::TestCaseGroup(m_testCtx, "unspecified_buffer", "Target new unspecified buffer"),
                 BufferDataUploadCase::CASE_UNSPECIFIED_BUFFER, true},
                {new tcu::TestCaseGroup(m_testCtx, "specified_buffer", "Target new specified buffer"),
                 BufferDataUploadCase::CASE_SPECIFIED_BUFFER, true},
                {new tcu::TestCaseGroup(m_testCtx, "used_buffer", "Target buffer that was used in draw"),
                 BufferDataUploadCase::CASE_USED_BUFFER, true},
                {new tcu::TestCaseGroup(m_testCtx, "larger_used_buffer", "Target larger buffer that was used in draw"),
                 BufferDataUploadCase::CASE_USED_LARGER_BUFFER, false},
            };

            for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(targetCases); ++targetNdx)
            {
                bufferDataMethodGroup->addChild(targetCases[targetNdx].group);

                for (int usageNdx = 0; usageNdx < DE_LENGTH_OF_ARRAY(bufferUsages); ++usageNdx)
                    if (bufferUsages[usageNdx].primaryUsage || targetCases[targetNdx].allUsages)
                        targetCases[targetNdx].group->addChild(new BufferDataUploadCase(
                            m_context, std::string("usage_").append(bufferUsages[usageNdx].name).c_str(),
                            std::string("Test with usage = ").append(bufferUsages[usageNdx].name).c_str(),
                            minBufferSize, maxBufferSize, numDataSamples, bufferUsages[usageNdx].usage,
                            targetCases[targetNdx].caseType));
            }
        }

        // .buffer_sub_data
        {
            static const struct FlagCase
            {
                tcu::TestCaseGroup *group;
                BufferSubDataUploadCase::CaseType parentCase;
                bool allUsages;
                int flags;
            } flagCases[] = {
                {new tcu::TestCaseGroup(m_testCtx, "used_buffer_full_upload", ""),
                 BufferSubDataUploadCase::CASE_USED_BUFFER, true, BufferSubDataUploadCase::FLAG_FULL_UPLOAD},
                {new tcu::TestCaseGroup(m_testCtx, "used_buffer_invalidate_before_full_upload",
                                        "Clear buffer with bufferData(...,NULL) before sub data call"),
                 BufferSubDataUploadCase::CASE_USED_BUFFER, false,
                 BufferSubDataUploadCase::FLAG_FULL_UPLOAD | BufferSubDataUploadCase::FLAG_INVALIDATE_BEFORE_USE},
                {new tcu::TestCaseGroup(m_testCtx, "used_buffer_partial_upload", ""),
                 BufferSubDataUploadCase::CASE_USED_BUFFER, true, BufferSubDataUploadCase::FLAG_PARTIAL_UPLOAD},
                {new tcu::TestCaseGroup(m_testCtx, "used_buffer_invalidate_before_partial_upload",
                                        "Clear buffer with bufferData(...,NULL) before sub data call"),
                 BufferSubDataUploadCase::CASE_USED_BUFFER, false,
                 BufferSubDataUploadCase::FLAG_PARTIAL_UPLOAD | BufferSubDataUploadCase::FLAG_INVALIDATE_BEFORE_USE},
            };

            for (int flagNdx = 0; flagNdx < DE_LENGTH_OF_ARRAY(flagCases); ++flagNdx)
            {
                bufferSubDataMethodGroup->addChild(flagCases[flagNdx].group);

                for (int usageNdx = 0; usageNdx < DE_LENGTH_OF_ARRAY(bufferUsages); ++usageNdx)
                    if (bufferUsages[usageNdx].primaryUsage || flagCases[flagNdx].allUsages)
                        flagCases[flagNdx].group->addChild(new BufferSubDataUploadCase(
                            m_context, std::string("usage_").append(bufferUsages[usageNdx].name).c_str(),
                            std::string("Test with usage = ").append(bufferUsages[usageNdx].name).c_str(),
                            minBufferSize, maxBufferSize, numDataSamples, bufferUsages[usageNdx].usage,
                            flagCases[flagNdx].parentCase, flagCases[flagNdx].flags));
            }
        }

        // .map_buffer_range
        {
            static const struct FlagCase
            {
                const char *name;
                bool usefulForUnusedBuffers;
                bool allUsages;
                int glFlags;
                int caseFlags;
            } flagCases[] = {
                {"flag_write_full", true, true, GL_MAP_WRITE_BIT, 0},
                {"flag_write_partial", true, true, GL_MAP_WRITE_BIT, MapBufferRangeCase::FLAG_PARTIAL},
                {"flag_read_write_full", true, true, GL_MAP_WRITE_BIT | GL_MAP_READ_BIT, 0},
                {"flag_read_write_partial", true, true, GL_MAP_WRITE_BIT | GL_MAP_READ_BIT,
                 MapBufferRangeCase::FLAG_PARTIAL},
                {"flag_invalidate_range_full", true, false, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT, 0},
                {"flag_invalidate_range_partial", true, false, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT,
                 MapBufferRangeCase::FLAG_PARTIAL},
                {"flag_invalidate_buffer_full", true, false, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT, 0},
                {"flag_invalidate_buffer_partial", true, false, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT,
                 MapBufferRangeCase::FLAG_PARTIAL},
                {"flag_write_full_manual_invalidate_buffer", false, false,
                 GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT, MapBufferRangeCase::FLAG_MANUAL_INVALIDATION},
                {"flag_write_partial_manual_invalidate_buffer", false, false,
                 GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT,
                 MapBufferRangeCase::FLAG_PARTIAL | MapBufferRangeCase::FLAG_MANUAL_INVALIDATION},
                {"flag_unsynchronized_full", true, false, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT, 0},
                {"flag_unsynchronized_partial", true, false, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT,
                 MapBufferRangeCase::FLAG_PARTIAL},
                {"flag_unsynchronized_and_invalidate_buffer_full", true, false,
                 GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_BUFFER_BIT, 0},
                {"flag_unsynchronized_and_invalidate_buffer_partial", true, false,
                 GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_BUFFER_BIT,
                 MapBufferRangeCase::FLAG_PARTIAL},
            };
            static const struct FlushCases
            {
                const char *name;
                int glFlags;
                int caseFlags;
            } flushCases[] = {
                {"flag_flush_explicit_map_full", GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT, 0},
                {"flag_flush_explicit_map_partial", GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT,
                 MapBufferRangeFlushCase::FLAG_PARTIAL},
                {"flag_flush_explicit_map_full_flush_in_parts", GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT,
                 MapBufferRangeFlushCase::FLAG_FLUSH_IN_PARTS},
                {"flag_flush_explicit_map_full_flush_partial", GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT,
                 MapBufferRangeFlushCase::FLAG_FLUSH_PARTIAL},
            };
            static const struct MapTestGroup
            {
                int flags;
                bool unusedBufferCase;
                tcu::TestCaseGroup *group;
            } groups[] = {
                {
                    MapBufferRangeCase::FLAG_USE_UNUSED_UNSPECIFIED_BUFFER,
                    true,
                    new tcu::TestCaseGroup(m_testCtx, "new_unspecified_buffer",
                                           "Test with unused, unspecified buffers"),
                },
                {
                    MapBufferRangeCase::FLAG_USE_UNUSED_SPECIFIED_BUFFER,
                    true,
                    new tcu::TestCaseGroup(m_testCtx, "new_specified_buffer", "Test with unused, specified buffers"),
                },
                {0, false,
                 new tcu::TestCaseGroup(m_testCtx, "used_buffer",
                                        "Test with used (data has been sourced from a buffer) buffers")},
            };

            // we OR same flags to both range and flushRange cases, make sure it is legal
            DE_STATIC_ASSERT((int)MapBufferRangeCase::FLAG_USE_UNUSED_SPECIFIED_BUFFER ==
                             (int)MapBufferRangeFlushCase::FLAG_USE_UNUSED_SPECIFIED_BUFFER);
            DE_STATIC_ASSERT((int)MapBufferRangeCase::FLAG_USE_UNUSED_UNSPECIFIED_BUFFER ==
                             (int)MapBufferRangeFlushCase::FLAG_USE_UNUSED_UNSPECIFIED_BUFFER);

            for (int groupNdx = 0; groupNdx < DE_LENGTH_OF_ARRAY(groups); ++groupNdx)
            {
                tcu::TestCaseGroup *const bufferTypeGroup = groups[groupNdx].group;

                mapBufferRangeMethodGroup->addChild(bufferTypeGroup);

                for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(flagCases); ++caseNdx)
                {
                    if (groups[groupNdx].unusedBufferCase && !flagCases[caseNdx].usefulForUnusedBuffers)
                        continue;

                    tcu::TestCaseGroup *const bufferUsageGroup =
                        new tcu::TestCaseGroup(m_testCtx, flagCases[caseNdx].name, "");
                    bufferTypeGroup->addChild(bufferUsageGroup);

                    for (int usageNdx = 0; usageNdx < DE_LENGTH_OF_ARRAY(bufferUsages); ++usageNdx)
                        if (bufferUsages[usageNdx].primaryUsage || flagCases[caseNdx].allUsages)
                            bufferUsageGroup->addChild(new MapBufferRangeCase(
                                m_context, bufferUsages[usageNdx].name,
                                std::string("Test with usage = ").append(bufferUsages[usageNdx].name).c_str(),
                                minBufferSize, maxBufferSize, numMapSamples, bufferUsages[usageNdx].usage,
                                flagCases[caseNdx].glFlags, flagCases[caseNdx].caseFlags | groups[groupNdx].flags));
                }

                for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(flushCases); ++caseNdx)
                {
                    tcu::TestCaseGroup *const bufferUsageGroup =
                        new tcu::TestCaseGroup(m_testCtx, flushCases[caseNdx].name, "");
                    bufferTypeGroup->addChild(bufferUsageGroup);

                    for (int usageNdx = 0; usageNdx < DE_LENGTH_OF_ARRAY(bufferUsages); ++usageNdx)
                        if (bufferUsages[usageNdx].primaryUsage)
                            bufferUsageGroup->addChild(new MapBufferRangeFlushCase(
                                m_context, bufferUsages[usageNdx].name,
                                std::string("Test with usage = ").append(bufferUsages[usageNdx].name).c_str(),
                                minBufferSize, maxBufferSize, numMapSamples, bufferUsages[usageNdx].usage,
                                flushCases[caseNdx].glFlags, flushCases[caseNdx].caseFlags | groups[groupNdx].flags));
                }
            }
        }
    }

    // .modify_after_use
    {
        const int minBufferSize = 0;       // !< 0kiB
        const int maxBufferSize = 1 << 24; // !< 16MiB

        static const struct Usage
        {
            const char *name;
            const char *description;
            uint32_t usage;
        } usages[] = {
            {"static_draw", "Test with GL_STATIC_DRAW", GL_STATIC_DRAW},
            {"dynamic_draw", "Test with GL_DYNAMIC_DRAW", GL_DYNAMIC_DRAW},
            {"stream_draw", "Test with GL_STREAM_DRAW", GL_STREAM_DRAW},

        };

        for (int usageNdx = 0; usageNdx < DE_LENGTH_OF_ARRAY(usages); ++usageNdx)
        {
            tcu::TestCaseGroup *const usageGroup =
                new tcu::TestCaseGroup(m_testCtx, usages[usageNdx].name, usages[usageNdx].description);
            modifyAfterUseGroup->addChild(usageGroup);

            usageGroup->addChild(new ModifyAfterWithBufferDataCase(m_context, "buffer_data",
                                                                   "Respecify buffer contents after use", minBufferSize,
                                                                   maxBufferSize, usages[usageNdx].usage, 0));
            usageGroup->addChild(new ModifyAfterWithBufferDataCase(
                m_context, "buffer_data_different_size", "Respecify buffer contents and size after use", minBufferSize,
                maxBufferSize, usages[usageNdx].usage, ModifyAfterWithBufferDataCase::FLAG_RESPECIFY_SIZE));
            usageGroup->addChild(new ModifyAfterWithBufferDataCase(
                m_context, "buffer_data_repeated", "Respecify buffer contents after upload and use", minBufferSize,
                maxBufferSize, usages[usageNdx].usage, ModifyAfterWithBufferDataCase::FLAG_UPLOAD_REPEATED));

            usageGroup->addChild(new ModifyAfterWithBufferSubDataCase(
                m_context, "buffer_sub_data_full", "Respecify buffer contents after use", minBufferSize, maxBufferSize,
                usages[usageNdx].usage, 0));
            usageGroup->addChild(new ModifyAfterWithBufferSubDataCase(
                m_context, "buffer_sub_data_partial", "Respecify buffer contents partially use", minBufferSize,
                maxBufferSize, usages[usageNdx].usage, ModifyAfterWithBufferSubDataCase::FLAG_PARTIAL));
            usageGroup->addChild(new ModifyAfterWithBufferSubDataCase(
                m_context, "buffer_sub_data_full_repeated", "Respecify buffer contents after upload and use",
                minBufferSize, maxBufferSize, usages[usageNdx].usage,
                ModifyAfterWithBufferSubDataCase::FLAG_UPLOAD_REPEATED));
            usageGroup->addChild(new ModifyAfterWithBufferSubDataCase(
                m_context, "buffer_sub_data_partial_repeated", "Respecify buffer contents partially upload and use",
                minBufferSize, maxBufferSize, usages[usageNdx].usage,
                ModifyAfterWithBufferSubDataCase::FLAG_UPLOAD_REPEATED |
                    ModifyAfterWithBufferSubDataCase::FLAG_PARTIAL));

            usageGroup->addChild(new ModifyAfterWithMapBufferRangeCase(
                m_context, "map_flag_write_full", "Respecify buffer contents after use", minBufferSize, maxBufferSize,
                usages[usageNdx].usage, 0, GL_MAP_WRITE_BIT));
            usageGroup->addChild(new ModifyAfterWithMapBufferRangeCase(
                m_context, "map_flag_write_partial", "Respecify buffer contents partially after use", minBufferSize,
                maxBufferSize, usages[usageNdx].usage, ModifyAfterWithMapBufferRangeCase::FLAG_PARTIAL,
                GL_MAP_WRITE_BIT));
            usageGroup->addChild(new ModifyAfterWithMapBufferRangeCase(
                m_context, "map_flag_read_write_full", "Respecify buffer contents after use", minBufferSize,
                maxBufferSize, usages[usageNdx].usage, 0, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT));
            usageGroup->addChild(new ModifyAfterWithMapBufferRangeCase(
                m_context, "map_flag_read_write_partial", "Respecify buffer contents partially after use",
                minBufferSize, maxBufferSize, usages[usageNdx].usage, ModifyAfterWithMapBufferRangeCase::FLAG_PARTIAL,
                GL_MAP_READ_BIT | GL_MAP_WRITE_BIT));
            usageGroup->addChild(new ModifyAfterWithMapBufferRangeCase(
                m_context, "map_flag_invalidate_range_full", "Respecify buffer contents after use", minBufferSize,
                maxBufferSize, usages[usageNdx].usage, 0, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT));
            usageGroup->addChild(new ModifyAfterWithMapBufferRangeCase(
                m_context, "map_flag_invalidate_range_partial", "Respecify buffer contents partially after use",
                minBufferSize, maxBufferSize, usages[usageNdx].usage, ModifyAfterWithMapBufferRangeCase::FLAG_PARTIAL,
                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT));
            usageGroup->addChild(new ModifyAfterWithMapBufferRangeCase(
                m_context, "map_flag_invalidate_buffer_full", "Respecify buffer contents after use", minBufferSize,
                maxBufferSize, usages[usageNdx].usage, 0, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
            usageGroup->addChild(new ModifyAfterWithMapBufferRangeCase(
                m_context, "map_flag_invalidate_buffer_partial", "Respecify buffer contents partially after use",
                minBufferSize, maxBufferSize, usages[usageNdx].usage, ModifyAfterWithMapBufferRangeCase::FLAG_PARTIAL,
                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
            usageGroup->addChild(new ModifyAfterWithMapBufferRangeCase(
                m_context, "map_flag_unsynchronized_full", "Respecify buffer contents after use", minBufferSize,
                maxBufferSize, usages[usageNdx].usage, 0, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT));
            usageGroup->addChild(new ModifyAfterWithMapBufferRangeCase(
                m_context, "map_flag_unsynchronized_partial", "Respecify buffer contents partially after use",
                minBufferSize, maxBufferSize, usages[usageNdx].usage, ModifyAfterWithMapBufferRangeCase::FLAG_PARTIAL,
                GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT));

            usageGroup->addChild(new ModifyAfterWithMapBufferFlushCase(
                m_context, "map_flag_flush_explicit_full", "Respecify buffer contents after use", minBufferSize,
                maxBufferSize, usages[usageNdx].usage, 0, GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT));
            usageGroup->addChild(new ModifyAfterWithMapBufferFlushCase(
                m_context, "map_flag_flush_explicit_partial", "Respecify buffer contents partially after use",
                minBufferSize, maxBufferSize, usages[usageNdx].usage, ModifyAfterWithMapBufferFlushCase::FLAG_PARTIAL,
                GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT));
        }
    }

    // .render_after_upload
    {
        // .reference
        {
            tcu::TestCaseGroup *const renderReferenceGroup =
                new tcu::TestCaseGroup(m_testCtx, "reference", "Baseline results");
            renderAfterUploadGroup->addChild(renderReferenceGroup);

            // .draw
            {
                tcu::TestCaseGroup *const drawGroup =
                    new tcu::TestCaseGroup(m_testCtx, "draw", "Time usage of functions with non-modified buffers");
                renderReferenceGroup->addChild(drawGroup);

                // Time consumed by readPixels
                drawGroup->addChild(new ReferenceReadPixelsTimeCase(
                    m_context, "read_pixels", "Measure time consumed by readPixels() function call"));

                // Time consumed by rendering
                drawGroup->addChild(new ReferenceRenderTimeCase(m_context, "draw_arrays",
                                                                "Measure time consumed by drawArrays() function call",
                                                                DRAWMETHOD_DRAW_ARRAYS));
                drawGroup->addChild(new ReferenceRenderTimeCase(m_context, "draw_elements",
                                                                "Measure time consumed by drawElements() function call",
                                                                DRAWMETHOD_DRAW_ELEMENTS));
            }

            // .draw_upload_draw
            {
                static const struct
                {
                    const char *name;
                    const char *description;
                    DrawMethod drawMethod;
                    TargetBuffer targetBuffer;
                    bool partial;
                } uploadTargets[] = {
                    {"draw_arrays_upload_vertices",
                     "Measure time consumed by drawArrays, vertex attribute upload, another drawArrays, and readPixels "
                     "function calls.",
                     DRAWMETHOD_DRAW_ARRAYS, TARGETBUFFER_VERTEX, false},
                    {"draw_arrays_upload_vertices_partial",
                     "Measure time consumed by drawArrays, partial vertex attribute upload, another drawArrays, and "
                     "readPixels function calls.",
                     DRAWMETHOD_DRAW_ARRAYS, TARGETBUFFER_VERTEX, true},
                    {"draw_elements_upload_vertices",
                     "Measure time consumed by drawElements, vertex attribute upload, another drawElements, and "
                     "readPixels function calls.",
                     DRAWMETHOD_DRAW_ELEMENTS, TARGETBUFFER_VERTEX, false},
                    {"draw_elements_upload_indices",
                     "Measure time consumed by drawElements, index upload, another drawElements, and readPixels "
                     "function calls.",
                     DRAWMETHOD_DRAW_ELEMENTS, TARGETBUFFER_INDEX, false},
                    {"draw_elements_upload_indices_partial",
                     "Measure time consumed by drawElements, partial index upload, another drawElements, and "
                     "readPixels function calls.",
                     DRAWMETHOD_DRAW_ELEMENTS, TARGETBUFFER_INDEX, true},
                };
                static const struct
                {
                    const char *name;
                    const char *description;
                    UploadMethod uploadMethod;
                    BufferInUseRenderTimeCase::MapFlags mapFlags;
                    bool supportsPartialUpload;
                } uploadMethods[] = {
                    {"buffer_data", "bufferData", UPLOADMETHOD_BUFFER_DATA, BufferInUseRenderTimeCase::MAPFLAG_NONE,
                     false},
                    {"buffer_sub_data", "bufferSubData", UPLOADMETHOD_BUFFER_SUB_DATA,
                     BufferInUseRenderTimeCase::MAPFLAG_NONE, true},
                    {"map_buffer_range_invalidate_range", "mapBufferRange", UPLOADMETHOD_MAP_BUFFER_RANGE,
                     BufferInUseRenderTimeCase::MAPFLAG_INVALIDATE_RANGE, true},
                    {"map_buffer_range_invalidate_buffer", "mapBufferRange", UPLOADMETHOD_MAP_BUFFER_RANGE,
                     BufferInUseRenderTimeCase::MAPFLAG_INVALIDATE_BUFFER, false},
                };

                tcu::TestCaseGroup *const drawUploadDrawGroup = new tcu::TestCaseGroup(
                    m_testCtx, "draw_upload_draw", "Time usage of functions draw, upload and another draw");
                renderReferenceGroup->addChild(drawUploadDrawGroup);

                for (int uploadTargetNdx = 0; uploadTargetNdx < DE_LENGTH_OF_ARRAY(uploadTargets); ++uploadTargetNdx)
                    for (int uploadMethodNdx = 0; uploadMethodNdx < DE_LENGTH_OF_ARRAY(uploadMethods);
                         ++uploadMethodNdx)
                    {
                        const std::string name = std::string() + uploadTargets[uploadTargetNdx].name + "_with_" +
                                                 uploadMethods[uploadMethodNdx].name;

                        if (uploadTargets[uploadTargetNdx].partial &&
                            !uploadMethods[uploadMethodNdx].supportsPartialUpload)
                            continue;

                        drawUploadDrawGroup->addChild(new BufferInUseRenderTimeCase(
                            m_context, name.c_str(), uploadTargets[uploadTargetNdx].description,
                            uploadTargets[uploadTargetNdx].drawMethod, uploadMethods[uploadMethodNdx].mapFlags,
                            uploadTargets[uploadTargetNdx].targetBuffer, uploadMethods[uploadMethodNdx].uploadMethod,
                            (uploadTargets[uploadTargetNdx].partial) ? (UPLOADRANGE_PARTIAL) : (UPLOADRANGE_FULL),
                            BufferInUseRenderTimeCase::UPLOADBUFFERTARGET_DIFFERENT_BUFFER));
                    }
            }
        }

        // .upload_unrelated_and_draw
        {
            static const struct
            {
                const char *name;
                const char *description;
                DrawMethod drawMethod;
            } drawMethods[] = {
                {"draw_arrays", "drawArrays", DRAWMETHOD_DRAW_ARRAYS},
                {"draw_elements", "drawElements", DRAWMETHOD_DRAW_ELEMENTS},
            };

            static const struct
            {
                const char *name;
                UploadMethod uploadMethod;
            } uploadMethods[] = {
                {"buffer_data", UPLOADMETHOD_BUFFER_DATA},
                {"buffer_sub_data", UPLOADMETHOD_BUFFER_SUB_DATA},
                {"map_buffer_range", UPLOADMETHOD_MAP_BUFFER_RANGE},
            };

            tcu::TestCaseGroup *const uploadUnrelatedGroup = new tcu::TestCaseGroup(
                m_testCtx, "upload_unrelated_and_draw", "Time usage of functions after an unrelated upload");
            renderAfterUploadGroup->addChild(uploadUnrelatedGroup);

            for (int drawMethodNdx = 0; drawMethodNdx < DE_LENGTH_OF_ARRAY(drawMethods); ++drawMethodNdx)
                for (int uploadMethodNdx = 0; uploadMethodNdx < DE_LENGTH_OF_ARRAY(uploadMethods); ++uploadMethodNdx)
                {
                    const std::string name = std::string() + drawMethods[drawMethodNdx].name +
                                             "_upload_unrelated_with_" + uploadMethods[uploadMethodNdx].name;
                    const std::string desc = std::string() + "Measure time consumed by " +
                                             drawMethods[drawMethodNdx].description +
                                             " function call after an unrelated upload";

                    // Time consumed by rendering command after an unrelated upload

                    uploadUnrelatedGroup->addChild(new UnrelatedUploadRenderTimeCase(
                        m_context, name.c_str(), desc.c_str(), drawMethods[drawMethodNdx].drawMethod,
                        uploadMethods[uploadMethodNdx].uploadMethod));
                }
        }

        // .upload_and_draw
        {
            static const struct
            {
                const char *name;
                const char *description;
                BufferState bufferState;
                UnrelatedBufferType unrelatedBuffer;
                bool supportsPartialUpload;
            } bufferConfigs[] = {
                {"used_buffer", "Upload to an used buffer", BUFFERSTATE_EXISTING, UNRELATEDBUFFERTYPE_NONE, true},
                {"new_buffer", "Upload to a new buffer", BUFFERSTATE_NEW, UNRELATEDBUFFERTYPE_NONE, false},
                {"used_buffer_and_unrelated_upload", "Upload to an used buffer and an unrelated buffer and then draw",
                 BUFFERSTATE_EXISTING, UNRELATEDBUFFERTYPE_VERTEX, true},
                {"new_buffer_and_unrelated_upload", "Upload to a new buffer and an unrelated buffer and then draw",
                 BUFFERSTATE_NEW, UNRELATEDBUFFERTYPE_VERTEX, false},
            };

            tcu::TestCaseGroup *const uploadAndDrawGroup = new tcu::TestCaseGroup(
                m_testCtx, "upload_and_draw", "Time usage of rendering functions with modified buffers");
            renderAfterUploadGroup->addChild(uploadAndDrawGroup);

            // .used_buffer
            // .new_buffer
            // .used_buffer_and_unrelated_upload
            // .new_buffer_and_unrelated_upload
            for (int stateNdx = 0; stateNdx < DE_LENGTH_OF_ARRAY(bufferConfigs); ++stateNdx)
            {
                static const struct
                {
                    const char *name;
                    const char *description;
                    DrawMethod drawMethod;
                    TargetBuffer targetBuffer;
                    bool partial;
                } uploadTargets[] = {
                    {"draw_arrays_upload_vertices",
                     "Measure time consumed by vertex attribute upload, drawArrays, and readPixels function calls",
                     DRAWMETHOD_DRAW_ARRAYS, TARGETBUFFER_VERTEX, false},
                    {"draw_arrays_upload_vertices_partial",
                     "Measure time consumed by partial vertex attribute upload, drawArrays, and readPixels function "
                     "calls",
                     DRAWMETHOD_DRAW_ARRAYS, TARGETBUFFER_VERTEX, true},
                    {"draw_elements_upload_vertices",
                     "Measure time consumed by vertex attribute upload, drawElements, and readPixels function calls",
                     DRAWMETHOD_DRAW_ELEMENTS, TARGETBUFFER_VERTEX, false},
                    {"draw_elements_upload_indices",
                     "Measure time consumed by index upload, drawElements, and readPixels function calls",
                     DRAWMETHOD_DRAW_ELEMENTS, TARGETBUFFER_INDEX, false},
                    {"draw_elements_upload_indices_partial",
                     "Measure time consumed by partial index upload, drawElements, and readPixels function calls",
                     DRAWMETHOD_DRAW_ELEMENTS, TARGETBUFFER_INDEX, true},
                };
                static const struct
                {
                    const char *name;
                    const char *description;
                    UploadMethod uploadMethod;
                    bool supportsPartialUpload;
                } uploadMethods[] = {
                    {"buffer_data", "bufferData", UPLOADMETHOD_BUFFER_DATA, false},
                    {"buffer_sub_data", "bufferSubData", UPLOADMETHOD_BUFFER_SUB_DATA, true},
                    {"map_buffer_range", "mapBufferRange", UPLOADMETHOD_MAP_BUFFER_RANGE, true},
                };

                tcu::TestCaseGroup *const group = new tcu::TestCaseGroup(m_testCtx, bufferConfigs[stateNdx].name,
                                                                         bufferConfigs[stateNdx].description);
                uploadAndDrawGroup->addChild(group);

                for (int uploadTargetNdx = 0; uploadTargetNdx < DE_LENGTH_OF_ARRAY(uploadTargets); ++uploadTargetNdx)
                    for (int uploadMethodNdx = 0; uploadMethodNdx < DE_LENGTH_OF_ARRAY(uploadMethods);
                         ++uploadMethodNdx)
                    {
                        const std::string name = std::string() + uploadTargets[uploadTargetNdx].name + "_with_" +
                                                 uploadMethods[uploadMethodNdx].name;

                        if (uploadTargets[uploadTargetNdx].partial &&
                            !uploadMethods[uploadMethodNdx].supportsPartialUpload)
                            continue;
                        if (uploadTargets[uploadTargetNdx].partial && !bufferConfigs[stateNdx].supportsPartialUpload)
                            continue;

                        // Don't log unrelated buffer information to samples if there is no such buffer

                        if (bufferConfigs[stateNdx].unrelatedBuffer == UNRELATEDBUFFERTYPE_NONE)
                        {
                            typedef UploadRenderReadDuration SampleType;
                            typedef GenericUploadRenderTimeCase<SampleType> TestType;

                            group->addChild(new TestType(
                                m_context, name.c_str(), uploadTargets[uploadTargetNdx].description,
                                uploadTargets[uploadTargetNdx].drawMethod, uploadTargets[uploadTargetNdx].targetBuffer,
                                uploadMethods[uploadMethodNdx].uploadMethod, bufferConfigs[stateNdx].bufferState,
                                (uploadTargets[uploadTargetNdx].partial) ? (UPLOADRANGE_PARTIAL) : (UPLOADRANGE_FULL),
                                bufferConfigs[stateNdx].unrelatedBuffer));
                        }
                        else
                        {
                            typedef UploadRenderReadDurationWithUnrelatedUploadSize SampleType;
                            typedef GenericUploadRenderTimeCase<SampleType> TestType;

                            group->addChild(new TestType(
                                m_context, name.c_str(), uploadTargets[uploadTargetNdx].description,
                                uploadTargets[uploadTargetNdx].drawMethod, uploadTargets[uploadTargetNdx].targetBuffer,
                                uploadMethods[uploadMethodNdx].uploadMethod, bufferConfigs[stateNdx].bufferState,
                                (uploadTargets[uploadTargetNdx].partial) ? (UPLOADRANGE_PARTIAL) : (UPLOADRANGE_FULL),
                                bufferConfigs[stateNdx].unrelatedBuffer));
                        }
                    }
            }
        }

        // .draw_modify_draw
        {
            static const struct
            {
                const char *name;
                const char *description;
                DrawMethod drawMethod;
                TargetBuffer targetBuffer;
                bool partial;
            } uploadTargets[] = {
                {"draw_arrays_upload_vertices",
                 "Measure time consumed by drawArrays, vertex attribute upload, another drawArrays, and readPixels "
                 "function calls.",
                 DRAWMETHOD_DRAW_ARRAYS, TARGETBUFFER_VERTEX, false},
                {"draw_arrays_upload_vertices_partial",
                 "Measure time consumed by drawArrays, partial vertex attribute upload, another drawArrays, and "
                 "readPixels function calls.",
                 DRAWMETHOD_DRAW_ARRAYS, TARGETBUFFER_VERTEX, true},
                {"draw_elements_upload_vertices",
                 "Measure time consumed by drawElements, vertex attribute upload, another drawElements, and readPixels "
                 "function calls.",
                 DRAWMETHOD_DRAW_ELEMENTS, TARGETBUFFER_VERTEX, false},
                {"draw_elements_upload_indices",
                 "Measure time consumed by drawElements, index upload, another drawElements, and readPixels function "
                 "calls.",
                 DRAWMETHOD_DRAW_ELEMENTS, TARGETBUFFER_INDEX, false},
                {"draw_elements_upload_indices_partial",
                 "Measure time consumed by drawElements, partial index upload, another drawElements, and readPixels "
                 "function calls.",
                 DRAWMETHOD_DRAW_ELEMENTS, TARGETBUFFER_INDEX, true},
            };
            static const struct
            {
                const char *name;
                const char *description;
                UploadMethod uploadMethod;
                BufferInUseRenderTimeCase::MapFlags mapFlags;
                bool supportsPartialUpload;
            } uploadMethods[] = {
                {"buffer_data", "bufferData", UPLOADMETHOD_BUFFER_DATA, BufferInUseRenderTimeCase::MAPFLAG_NONE, false},
                {"buffer_sub_data", "bufferSubData", UPLOADMETHOD_BUFFER_SUB_DATA,
                 BufferInUseRenderTimeCase::MAPFLAG_NONE, true},
                {"map_buffer_range_invalidate_range", "mapBufferRange", UPLOADMETHOD_MAP_BUFFER_RANGE,
                 BufferInUseRenderTimeCase::MAPFLAG_INVALIDATE_RANGE, true},
                {"map_buffer_range_invalidate_buffer", "mapBufferRange", UPLOADMETHOD_MAP_BUFFER_RANGE,
                 BufferInUseRenderTimeCase::MAPFLAG_INVALIDATE_BUFFER, false},
            };

            tcu::TestCaseGroup *const drawModifyDrawGroup = new tcu::TestCaseGroup(
                m_testCtx, "draw_modify_draw",
                "Time used in rendering functions with modified buffers while original buffer is still in use");
            renderAfterUploadGroup->addChild(drawModifyDrawGroup);

            for (int uploadTargetNdx = 0; uploadTargetNdx < DE_LENGTH_OF_ARRAY(uploadTargets); ++uploadTargetNdx)
                for (int uploadMethodNdx = 0; uploadMethodNdx < DE_LENGTH_OF_ARRAY(uploadMethods); ++uploadMethodNdx)
                {
                    const std::string name = std::string() + uploadTargets[uploadTargetNdx].name + "_with_" +
                                             uploadMethods[uploadMethodNdx].name;

                    if (uploadTargets[uploadTargetNdx].partial && !uploadMethods[uploadMethodNdx].supportsPartialUpload)
                        continue;

                    drawModifyDrawGroup->addChild(new BufferInUseRenderTimeCase(
                        m_context, name.c_str(), uploadTargets[uploadTargetNdx].description,
                        uploadTargets[uploadTargetNdx].drawMethod, uploadMethods[uploadMethodNdx].mapFlags,
                        uploadTargets[uploadTargetNdx].targetBuffer, uploadMethods[uploadMethodNdx].uploadMethod,
                        (uploadTargets[uploadTargetNdx].partial) ? (UPLOADRANGE_PARTIAL) : (UPLOADRANGE_FULL),
                        BufferInUseRenderTimeCase::UPLOADBUFFERTARGET_SAME_BUFFER));
                }
        }

        // .upload_wait_draw
        {
            static const struct
            {
                const char *name;
                const char *description;
                BufferState bufferState;
            } bufferStates[] = {
                {"new_buffer", "Uploading to just generated name", BUFFERSTATE_NEW},
                {"used_buffer", "Uploading to a used buffer", BUFFERSTATE_EXISTING},
            };
            static const struct
            {
                const char *name;
                const char *description;
                DrawMethod drawMethod;
                TargetBuffer targetBuffer;
            } uploadTargets[] = {
                {"draw_arrays_vertices", "Upload vertex data, draw with drawArrays", DRAWMETHOD_DRAW_ARRAYS,
                 TARGETBUFFER_VERTEX},
                {"draw_elements_vertices", "Upload vertex data, draw with drawElements", DRAWMETHOD_DRAW_ELEMENTS,
                 TARGETBUFFER_VERTEX},
                {"draw_elements_indices", "Upload index data, draw with drawElements", DRAWMETHOD_DRAW_ELEMENTS,
                 TARGETBUFFER_INDEX},
            };
            static const struct
            {
                const char *name;
                const char *description;
                UploadMethod uploadMethod;
            } uploadMethods[] = {
                {"buffer_data", "bufferData", UPLOADMETHOD_BUFFER_DATA},
                {"buffer_sub_data", "bufferSubData", UPLOADMETHOD_BUFFER_SUB_DATA},
                {"map_buffer_range", "mapBufferRange", UPLOADMETHOD_MAP_BUFFER_RANGE},
            };

            tcu::TestCaseGroup *const uploadSwapDrawGroup = new tcu::TestCaseGroup(
                m_testCtx, "upload_wait_draw", "Time used in rendering functions after a buffer upload N frames ago");
            renderAfterUploadGroup->addChild(uploadSwapDrawGroup);

            for (int bufferStateNdx = 0; bufferStateNdx < DE_LENGTH_OF_ARRAY(bufferStates); ++bufferStateNdx)
            {
                tcu::TestCaseGroup *const bufferGroup = new tcu::TestCaseGroup(
                    m_testCtx, bufferStates[bufferStateNdx].name, bufferStates[bufferStateNdx].description);
                uploadSwapDrawGroup->addChild(bufferGroup);

                for (int uploadTargetNdx = 0; uploadTargetNdx < DE_LENGTH_OF_ARRAY(uploadTargets); ++uploadTargetNdx)
                    for (int uploadMethodNdx = 0; uploadMethodNdx < DE_LENGTH_OF_ARRAY(uploadMethods);
                         ++uploadMethodNdx)
                    {
                        const std::string name = std::string() + uploadTargets[uploadTargetNdx].name + "_with_" +
                                                 uploadMethods[uploadMethodNdx].name;

                        bufferGroup->addChild(new UploadWaitDrawCase(
                            m_context, name.c_str(), uploadTargets[uploadTargetNdx].description,
                            uploadTargets[uploadTargetNdx].drawMethod, uploadTargets[uploadTargetNdx].targetBuffer,
                            uploadMethods[uploadMethodNdx].uploadMethod, bufferStates[bufferStateNdx].bufferState));
                    }
            }
        }
    }
}

} // namespace Performance
} // namespace gles3
} // namespace deqp
