//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ANGLEPerfTests:
//   Base class for google test performance tests
//

#include "ANGLEPerfTest.h"

#include "ANGLEPerfTestArgs.h"
#include "common/debug.h"
#include "common/mathutil.h"
#include "common/platform.h"
#include "common/string_utils.h"
#include "common/system_utils.h"
#include "common/utilities.h"
#include "test_utils/runner/TestSuite.h"
#include "third_party/perf/perf_test.h"
#include "third_party/trace_event/trace_event.h"
#include "util/shader_utils.h"
#include "util/test_utils.h"

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/prettywriter.h>

#if defined(ANGLE_USE_UTIL_LOADER) && defined(ANGLE_PLATFORM_WINDOWS)
#    include "util/windows/WGLWindow.h"
#endif  // defined(ANGLE_USE_UTIL_LOADER) &&defined(ANGLE_PLATFORM_WINDOWS)

using namespace angle;
namespace js = rapidjson;

namespace
{
constexpr size_t kInitialTraceEventBufferSize = 50000;
constexpr double kMilliSecondsPerSecond       = 1e3;
constexpr double kMicroSecondsPerSecond       = 1e6;
constexpr double kNanoSecondsPerSecond        = 1e9;
constexpr char kPeakMemoryMetric[]            = ".peak_memory";
constexpr char kMedianMemoryMetric[]          = ".median_memory";

struct TraceCategory
{
    unsigned char enabled;
    const char *name;
};

constexpr TraceCategory gTraceCategories[2] = {
    {1, "gpu.angle"},
    {1, "gpu.angle.gpu"},
};

void EmptyPlatformMethod(PlatformMethods *, const char *) {}

void CustomLogError(PlatformMethods *platform, const char *errorMessage)
{
    auto *angleRenderTest = static_cast<ANGLERenderTest *>(platform->context);
    angleRenderTest->onErrorMessage(errorMessage);
}

void OverrideWorkaroundsD3D(PlatformMethods *platform, FeaturesD3D *featuresD3D)
{
    auto *angleRenderTest = static_cast<ANGLERenderTest *>(platform->context);
    angleRenderTest->overrideWorkaroundsD3D(featuresD3D);
}

TraceEventHandle AddPerfTraceEvent(PlatformMethods *platform,
                                   char phase,
                                   const unsigned char *categoryEnabledFlag,
                                   const char *name,
                                   unsigned long long id,
                                   double timestamp,
                                   int numArgs,
                                   const char **argNames,
                                   const unsigned char *argTypes,
                                   const unsigned long long *argValues,
                                   unsigned char flags)
{
    if (!gEnableTrace)
        return 0;

    // Discover the category name based on categoryEnabledFlag.  This flag comes from the first
    // parameter of TraceCategory, and corresponds to one of the entries in gTraceCategories.
    static_assert(offsetof(TraceCategory, enabled) == 0,
                  "|enabled| must be the first field of the TraceCategory class.");
    const TraceCategory *category = reinterpret_cast<const TraceCategory *>(categoryEnabledFlag);

    ANGLERenderTest *renderTest = static_cast<ANGLERenderTest *>(platform->context);

    std::lock_guard<std::mutex> lock(renderTest->getTraceEventMutex());

    uint32_t tid = renderTest->getCurrentThreadSerial();

    std::vector<TraceEvent> &buffer = renderTest->getTraceEventBuffer();
    buffer.emplace_back(phase, category->name, name, timestamp, tid);
    return buffer.size();
}

const unsigned char *GetPerfTraceCategoryEnabled(PlatformMethods *platform,
                                                 const char *categoryName)
{
    if (gEnableTrace)
    {
        for (const TraceCategory &category : gTraceCategories)
        {
            if (strcmp(category.name, categoryName) == 0)
            {
                return &category.enabled;
            }
        }
    }

    constexpr static unsigned char kZero = 0;
    return &kZero;
}

void UpdateTraceEventDuration(PlatformMethods *platform,
                              const unsigned char *categoryEnabledFlag,
                              const char *name,
                              TraceEventHandle eventHandle)
{
    // Not implemented.
}

double MonotonicallyIncreasingTime(PlatformMethods *platform)
{
    return GetHostTimeSeconds();
}

bool WriteJsonFile(const std::string &outputFile, js::Document *doc)
{
    FILE *fp = fopen(outputFile.c_str(), "w");
    if (!fp)
    {
        return false;
    }

    constexpr size_t kBufferSize = 0xFFFF;
    std::vector<char> writeBuffer(kBufferSize);
    js::FileWriteStream os(fp, writeBuffer.data(), kBufferSize);
    js::PrettyWriter<js::FileWriteStream> writer(os);
    if (!doc->Accept(writer))
    {
        fclose(fp);
        return false;
    }
    fclose(fp);
    return true;
}

void DumpTraceEventsToJSONFile(const std::vector<TraceEvent> &traceEvents,
                               const char *outputFileName)
{
    js::Document doc(js::kObjectType);
    js::Document::AllocatorType &allocator = doc.GetAllocator();

    js::Value events(js::kArrayType);

    for (const TraceEvent &traceEvent : traceEvents)
    {
        js::Value value(js::kObjectType);

        const uint64_t microseconds = static_cast<uint64_t>(traceEvent.timestamp * 1000.0 * 1000.0);

        js::Document::StringRefType eventName(traceEvent.name);
        js::Document::StringRefType categoryName(traceEvent.categoryName);
        js::Document::StringRefType pidName(
            strcmp(traceEvent.categoryName, "gpu.angle.gpu") == 0 ? "GPU" : "ANGLE");

        value.AddMember("name", eventName, allocator);
        value.AddMember("cat", categoryName, allocator);
        value.AddMember("ph", std::string(1, traceEvent.phase), allocator);
        value.AddMember("ts", microseconds, allocator);
        value.AddMember("pid", pidName, allocator);
        value.AddMember("tid", traceEvent.tid, allocator);

        events.PushBack(value, allocator);
    }

    doc.AddMember("traceEvents", events, allocator);

    if (WriteJsonFile(outputFileName, &doc))
    {
        printf("Wrote trace file to %s\n", outputFileName);
    }
    else
    {
        printf("Error writing trace file to %s\n", outputFileName);
    }
}

ANGLE_MAYBE_UNUSED void KHRONOS_APIENTRY PerfTestDebugCallback(GLenum source,
                                                               GLenum type,
                                                               GLuint id,
                                                               GLenum severity,
                                                               GLsizei length,
                                                               const GLchar *message,
                                                               const void *userParam)
{
    // Early exit on non-errors.
    if (type != GL_DEBUG_TYPE_ERROR || !userParam)
    {
        return;
    }

    ANGLERenderTest *renderTest =
        const_cast<ANGLERenderTest *>(reinterpret_cast<const ANGLERenderTest *>(userParam));
    renderTest->onErrorMessage(message);
}

double ComputeMean(const std::vector<double> &values)
{
    double sum = std::accumulate(values.begin(), values.end(), 0.0);

    double mean = sum / static_cast<double>(values.size());
    return mean;
}
}  // anonymous namespace

TraceEvent::TraceEvent(char phaseIn,
                       const char *categoryNameIn,
                       const char *nameIn,
                       double timestampIn,
                       uint32_t tidIn)
    : phase(phaseIn), categoryName(categoryNameIn), name{}, timestamp(timestampIn), tid(tidIn)
{
    ASSERT(strlen(nameIn) < kMaxNameLen);
    strcpy(name, nameIn);
}

ANGLEPerfTest::ANGLEPerfTest(const std::string &name,
                             const std::string &backend,
                             const std::string &story,
                             unsigned int iterationsPerStep,
                             const char *units)
    : mName(name),
      mBackend(backend),
      mStory(story),
      mGPUTimeNs(0),
      mSkipTest(false),
      mStepsToRun(std::max(gStepsPerTrial, gMaxStepsPerformed)),
      mTrialNumStepsPerformed(0),
      mTotalNumStepsPerformed(0),
      mIterationsPerStep(iterationsPerStep),
      mRunning(true)
{
    if (mStory == "")
    {
        mStory = "baseline_story";
    }
    if (mStory[0] == '_')
    {
        mStory = mStory.substr(1);
    }
    mReporter = std::make_unique<perf_test::PerfResultReporter>(mName + mBackend, mStory);
    mReporter->RegisterImportantMetric(".wall_time", units);
    mReporter->RegisterImportantMetric(".cpu_time", units);
    mReporter->RegisterImportantMetric(".gpu_time", units);
    mReporter->RegisterFyiMetric(".trial_steps", "count");
    mReporter->RegisterFyiMetric(".total_steps", "count");
    mReporter->RegisterFyiMetric(".steps_to_run", "count");
}

ANGLEPerfTest::~ANGLEPerfTest() {}

void ANGLEPerfTest::run()
{
    if (mSkipTest)
    {
        return;
    }

    if (mStepsToRun <= 0)
    {
        // We don't call finish between calibration steps when calibrating non-Render tests. The
        // Render tests will have already calibrated when this code is run.
        calibrateStepsToRun(RunLoopPolicy::RunContinuously);
        ASSERT(mStepsToRun > 0);
    }

    uint32_t numTrials = OneFrame() ? 1 : gTestTrials;
    if (gVerboseLogging)
    {
        printf("Test Trials: %d\n", static_cast<int>(numTrials));
    }

    for (uint32_t trial = 0; trial < numTrials; ++trial)
    {
        doRunLoop(gMaxTrialTimeSeconds, mStepsToRun, RunLoopPolicy::RunContinuously);
        processResults();
        if (gVerboseLogging)
        {
            double trialTime = mTimer.getElapsedWallClockTime();
            printf("Trial %d time: %.2lf seconds.\n", trial + 1, trialTime);

            double secondsPerStep      = trialTime / static_cast<double>(mTrialNumStepsPerformed);
            double secondsPerIteration = secondsPerStep / static_cast<double>(mIterationsPerStep);
            mTestTrialResults.push_back(secondsPerIteration * 1000.0);
        }
    }

    if (gVerboseLogging && !mTestTrialResults.empty())
    {
        double numResults = static_cast<double>(mTestTrialResults.size());
        double mean       = ComputeMean(mTestTrialResults);

        double variance = 0;
        for (double trialResult : mTestTrialResults)
        {
            double difference = trialResult - mean;
            variance += difference * difference;
        }
        variance /= numResults;

        double standardDeviation      = std::sqrt(variance);
        double coefficientOfVariation = standardDeviation / mean;

        if (mean < 0.001)
        {
            printf("Mean result time: %.4lf ns.\n", mean * 1000.0);
        }
        else
        {
            printf("Mean result time: %.4lf ms.\n", mean);
        }
        printf("Coefficient of variation: %.2lf%%\n", coefficientOfVariation * 100.0);
    }
}

void ANGLEPerfTest::doRunLoop(double maxRunTime, int maxStepsToRun, RunLoopPolicy runPolicy)
{
    mTrialNumStepsPerformed = 0;
    mRunning                = true;
    mGPUTimeNs              = 0;
    mTimer.start();
    startTest();

    while (mRunning)
    {
        if (gMaxStepsPerformed > 0 && mTotalNumStepsPerformed >= gMaxStepsPerformed)
        {
            if (gVerboseLogging)
            {
                printf("Stopping test after %d steps.\n", mTotalNumStepsPerformed);
            }
            mRunning = false;
        }
        else if (mTimer.getElapsedWallClockTime() > maxRunTime)
        {
            mRunning = false;
        }
        else if (mTrialNumStepsPerformed >= maxStepsToRun)
        {
            mRunning = false;
        }
        else
        {
            step();

            if (runPolicy == RunLoopPolicy::FinishEveryStep)
            {
                glFinish();
            }

            if (mRunning)
            {
                mTrialNumStepsPerformed++;
                mTotalNumStepsPerformed++;
            }
        }
    }
    finishTest();
    mTimer.stop();
    computeGPUTime();
}

void ANGLEPerfTest::SetUp() {}

void ANGLEPerfTest::TearDown() {}

void ANGLEPerfTest::processResults()
{
    processClockResult(".cpu_time", mTimer.getElapsedCpuTime());
    processClockResult(".wall_time", mTimer.getElapsedWallClockTime());

    if (mGPUTimeNs > 0)
    {
        processClockResult(".gpu_time", mGPUTimeNs * 1e-9);
    }

    if (gVerboseLogging)
    {
        double fps = static_cast<double>(mTrialNumStepsPerformed * mIterationsPerStep) /
                     mTimer.getElapsedWallClockTime();
        printf("Ran %0.2lf iterations per second\n", fps);
    }

    if (gCalibration)
    {
        mReporter->AddResult(".steps_to_run", static_cast<size_t>(mStepsToRun));
    }
    else
    {
        mReporter->AddResult(".trial_steps", static_cast<size_t>(mTrialNumStepsPerformed));
        mReporter->AddResult(".total_steps", static_cast<size_t>(mTotalNumStepsPerformed));
    }

    if (!mProcessMemoryUsageKBSamples.empty())
    {
        std::sort(mProcessMemoryUsageKBSamples.begin(), mProcessMemoryUsageKBSamples.end());

        // Compute median.
        size_t medianIndex      = mProcessMemoryUsageKBSamples.size() / 2;
        uint64_t medianMemoryKB = mProcessMemoryUsageKBSamples[medianIndex];
        auto peakMemoryIterator = std::max_element(mProcessMemoryUsageKBSamples.begin(),
                                                   mProcessMemoryUsageKBSamples.end());
        uint64_t peakMemoryKB   = *peakMemoryIterator;

        processMemoryResult(kMedianMemoryMetric, medianMemoryKB);
        processMemoryResult(kPeakMemoryMetric, peakMemoryKB);
    }

    for (const auto &iter : mPerfCounterInfo)
    {
        const std::string &counterName = iter.second.name;
        std::vector<GLuint> samples    = iter.second.samples;

        size_t midpoint = samples.size() >> 1;
        std::nth_element(samples.begin(), samples.begin() + midpoint, samples.end());

        {
            std::stringstream medianStr;
            medianStr << "." << counterName << "_median";
            std::string medianName = medianStr.str();

            mReporter->AddResult(medianName, static_cast<size_t>(samples[midpoint]));
        }

        {
            std::string measurement = mName + mBackend + "." + counterName + "_median";
            TestSuite::GetInstance()->addHistogramSample(measurement, mStory, samples[midpoint],
                                                         "count");
        }

        const auto &maxIt = std::max_element(samples.begin(), samples.end());

        {
            std::stringstream maxStr;
            maxStr << "." << counterName << "_max";
            std::string maxName = maxStr.str();
            mReporter->AddResult(maxName, static_cast<size_t>(*maxIt));
        }

        {
            std::string measurement = mName + mBackend + "." + counterName + "_max";
            TestSuite::GetInstance()->addHistogramSample(measurement, mStory, *maxIt, "count");
        }
    }
}

void ANGLEPerfTest::processClockResult(const char *metric, double resultSeconds)
{
    double secondsPerStep      = resultSeconds / static_cast<double>(mTrialNumStepsPerformed);
    double secondsPerIteration = secondsPerStep / static_cast<double>(mIterationsPerStep);

    perf_test::MetricInfo metricInfo;
    std::string units;
    bool foundMetric = mReporter->GetMetricInfo(metric, &metricInfo);
    if (!foundMetric)
    {
        fprintf(stderr, "Error getting metric info for %s.\n", metric);
        return;
    }
    units = metricInfo.units;

    double result;

    if (units == "ms")
    {
        result = secondsPerIteration * kMilliSecondsPerSecond;
    }
    else if (units == "us")
    {
        result = secondsPerIteration * kMicroSecondsPerSecond;
    }
    else
    {
        result = secondsPerIteration * kNanoSecondsPerSecond;
    }
    mReporter->AddResult(metric, result);

    // Output histogram JSON set format if enabled.
    TestSuite::GetInstance()->addHistogramSample(mName + mBackend + metric, mStory,
                                                 secondsPerIteration * kMilliSecondsPerSecond,
                                                 "msBestFitFormat_smallerIsBetter");
}

void ANGLEPerfTest::processMemoryResult(const char *metric, uint64_t resultKB)
{
    perf_test::MetricInfo metricInfo;
    if (!mReporter->GetMetricInfo(metric, &metricInfo))
    {
        mReporter->RegisterImportantMetric(metric, "sizeInBytes");
    }

    mReporter->AddResult(metric, static_cast<size_t>(resultKB * 1000));

    TestSuite::GetInstance()->addHistogramSample(mName + mBackend + metric, mStory,
                                                 static_cast<double>(resultKB) * 1000.0,
                                                 "sizeInBytes_smallerIsBetter");
}

double ANGLEPerfTest::normalizedTime(size_t value) const
{
    return static_cast<double>(value) / static_cast<double>(mTrialNumStepsPerformed);
}

void ANGLEPerfTest::calibrateStepsToRun(RunLoopPolicy policy)
{
    // Run initially for "gCalibrationTimeSeconds" using the run loop policy.
    doRunLoop(gCalibrationTimeSeconds, std::numeric_limits<int>::max(), policy);

    double elapsedTime = mTimer.getElapsedWallClockTime();
    int stepsPerformed = mTrialNumStepsPerformed;

    double scale   = gCalibrationTimeSeconds / elapsedTime;
    int stepsToRun = static_cast<int>(static_cast<double>(stepsPerformed) * scale);
    stepsToRun     = std::max(1, stepsPerformed);
    if (getStepAlignment() != 1)
    {
        stepsToRun = rx::roundUp(stepsToRun, getStepAlignment());
    }

    // The run loop policy "FinishEveryStep" indicates we're running GPU tests. GPU work
    // completes asynchronously from the issued CPU steps. Therefore we need to call
    // glFinish before we can compute an accurate time elapsed by the test.
    //
    // To compute an accurate value for "mStepsToRun" we do a two-pass calibration. The
    // first pass runs for "gCalibrationTimeSeconds" and calls glFinish every step. The
    // estimated steps to run using this method is very inaccurate but is guaranteed to
    // complete in a fixed amount of time. Using that estimate we then run a second pass
    // and call glFinish a single time after "mStepsToRun" steps. We can then use the
    // "actual" time elapsed to compute an accurate estimate for "mStepsToRun".

    if (policy == RunLoopPolicy::FinishEveryStep)
    {
        for (int loopIndex = 0; loopIndex < gWarmupLoops; ++loopIndex)
        {
            doRunLoop(gMaxTrialTimeSeconds, stepsToRun, RunLoopPolicy::RunContinuously);

            // Compute mean of the calibration results.
            double sampleElapsedTime = mTimer.getElapsedWallClockTime();
            int sampleStepsPerformed = mTrialNumStepsPerformed;

            if (gVerboseLogging)
            {
                printf("Calibration loop took %.2lf seconds, with %d steps.\n", sampleElapsedTime,
                       sampleStepsPerformed);
            }

            // Scale steps down according to the time that exceeded one second.
            double sampleScale = gCalibrationTimeSeconds / sampleElapsedTime;
            stepsToRun = static_cast<int>(static_cast<double>(sampleStepsPerformed) * sampleScale);
            stepsToRun = std::max(1, stepsToRun);
            if (getStepAlignment() != 1)
            {
                stepsToRun = rx::roundUp(stepsToRun, getStepAlignment());
            }
        }
    }

    // Scale steps down according to the time that exceeded one second.
    mStepsToRun = stepsToRun;

    if (gVerboseLogging)
    {
        printf("Running %d steps after calibration.", mStepsToRun);
    }

    // Calibration allows the perf test runner script to save some time.
    if (gCalibration)
    {
        processResults();
        return;
    }
}

int ANGLEPerfTest::getStepAlignment() const
{
    // Default: No special alignment rules.
    return 1;
}

std::string RenderTestParams::backend() const
{
    std::stringstream strstr;

    switch (driver)
    {
        case GLESDriverType::AngleEGL:
            break;
        case GLESDriverType::SystemWGL:
        case GLESDriverType::SystemEGL:
            strstr << "_native";
            break;
        default:
            assert(0);
            return "_unk";
    }

    switch (getRenderer())
    {
        case EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE:
            break;
        case EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE:
            strstr << "_d3d11";
            break;
        case EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE:
            strstr << "_d3d9";
            break;
        case EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE:
            strstr << "_gl";
            break;
        case EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE:
            strstr << "_gles";
            break;
        case EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE:
            strstr << "_vulkan";
            break;
        default:
            assert(0);
            return "_unk";
    }

    switch (eglParameters.deviceType)
    {
        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE:
            strstr << "_null";
            break;
        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE:
            strstr << "_swiftshader";
            break;
        default:
            break;
    }

    return strstr.str();
}

std::string RenderTestParams::story() const
{
    switch (surfaceType)
    {
        case SurfaceType::Window:
            return "";
        case SurfaceType::WindowWithVSync:
            return "_vsync";
        case SurfaceType::Offscreen:
            return "_offscreen";
        default:
            UNREACHABLE();
            return "";
    }
}

std::string RenderTestParams::backendAndStory() const
{
    return backend() + story();
}

ANGLERenderTest::ANGLERenderTest(const std::string &name,
                                 const RenderTestParams &testParams,
                                 const char *units)
    : ANGLEPerfTest(name,
                    testParams.backend(),
                    testParams.story(),
                    OneFrame() ? 1 : testParams.iterationsPerStep,
                    units),
      mTestParams(testParams),
      mIsTimestampQueryAvailable(false),
      mGLWindow(nullptr),
      mOSWindow(nullptr),
      mSwapEnabled(true)
{
    // Force fast tests to make sure our slowest bots don't time out.
    if (OneFrame())
    {
        const_cast<RenderTestParams &>(testParams).iterationsPerStep = 1;
    }

    // Try to ensure we don't trigger allocation during execution.
    mTraceEventBuffer.reserve(kInitialTraceEventBufferSize);

    switch (testParams.driver)
    {
        case GLESDriverType::AngleEGL:
            mGLWindow = EGLWindow::New(testParams.majorVersion, testParams.minorVersion);
            mEntryPointsLib.reset(OpenSharedLibrary(ANGLE_EGL_LIBRARY_NAME, SearchType::ModuleDir));
            break;
        case GLESDriverType::SystemEGL:
#if defined(ANGLE_USE_UTIL_LOADER) && !defined(ANGLE_PLATFORM_WINDOWS)
            mGLWindow = EGLWindow::New(testParams.majorVersion, testParams.minorVersion);
            mEntryPointsLib.reset(OpenSharedLibraryWithExtension(
                GetNativeEGLLibraryNameWithExtension(), SearchType::SystemDir));
#else
            std::cerr << "Not implemented." << std::endl;
            mSkipTest = true;
#endif  // defined(ANGLE_USE_UTIL_LOADER) && !defined(ANGLE_PLATFORM_WINDOWS)
            break;
        case GLESDriverType::SystemWGL:
#if defined(ANGLE_USE_UTIL_LOADER) && defined(ANGLE_PLATFORM_WINDOWS)
            mGLWindow = WGLWindow::New(testParams.majorVersion, testParams.minorVersion);
            mEntryPointsLib.reset(OpenSharedLibrary("opengl32", SearchType::SystemDir));
#else
            std::cout << "WGL driver not available. Skipping test." << std::endl;
            mSkipTest = true;
#endif  // defined(ANGLE_USE_UTIL_LOADER) && defined(ANGLE_PLATFORM_WINDOWS)
            break;
        default:
            std::cerr << "Error in switch." << std::endl;
            mSkipTest = true;
            break;
    }
}

ANGLERenderTest::~ANGLERenderTest()
{
    OSWindow::Delete(&mOSWindow);
    GLWindowBase::Delete(&mGLWindow);
}

void ANGLERenderTest::addExtensionPrerequisite(const char *extensionName)
{
    mExtensionPrerequisites.push_back(extensionName);
}

void ANGLERenderTest::SetUp()
{
    if (mSkipTest)
    {
        return;
    }

    ANGLEPerfTest::SetUp();

    // Set a consistent CPU core affinity and high priority.
    StabilizeCPUForBenchmarking();

    mOSWindow = OSWindow::New();

    if (!mGLWindow)
    {
        mSkipTest = true;
        return;
    }

    mPlatformMethods.overrideWorkaroundsD3D      = OverrideWorkaroundsD3D;
    mPlatformMethods.logError                    = CustomLogError;
    mPlatformMethods.logWarning                  = EmptyPlatformMethod;
    mPlatformMethods.logInfo                     = EmptyPlatformMethod;
    mPlatformMethods.addTraceEvent               = AddPerfTraceEvent;
    mPlatformMethods.getTraceCategoryEnabledFlag = GetPerfTraceCategoryEnabled;
    mPlatformMethods.updateTraceEventDuration    = UpdateTraceEventDuration;
    mPlatformMethods.monotonicallyIncreasingTime = MonotonicallyIncreasingTime;
    mPlatformMethods.context                     = this;

    if (!mOSWindow->initialize(mName, mTestParams.windowWidth, mTestParams.windowHeight))
    {
        mSkipTest = true;
        FAIL() << "Failed initializing OSWindow";
        // FAIL returns.
    }

    // Override platform method parameter.
    EGLPlatformParameters withMethods = mTestParams.eglParameters;
    withMethods.platformMethods       = &mPlatformMethods;

    // Request a common framebuffer config
    mConfigParams.redBits     = 8;
    mConfigParams.greenBits   = 8;
    mConfigParams.blueBits    = 8;
    mConfigParams.alphaBits   = 8;
    mConfigParams.depthBits   = 24;
    mConfigParams.stencilBits = 8;
    mConfigParams.colorSpace  = mTestParams.colorSpace;
    if (mTestParams.surfaceType != SurfaceType::WindowWithVSync)
    {
        mConfigParams.swapInterval = 0;
    }

    GLWindowResult res = mGLWindow->initializeGLWithResult(
        mOSWindow, mEntryPointsLib.get(), mTestParams.driver, withMethods, mConfigParams);
    switch (res)
    {
        case GLWindowResult::NoColorspaceSupport:
            mSkipTest = true;
            std::cout << "Test skipped due to missing support for color spaces." << std::endl;
            return;
        case GLWindowResult::Error:
            mSkipTest = true;
            FAIL() << "Failed initializing GL Window";
            // FAIL returns.
        default:
            break;
    }

    // Disable vsync (if not done by the window init).
    if (mTestParams.surfaceType != SurfaceType::WindowWithVSync)
    {
        if (!mGLWindow->setSwapInterval(0))
        {
            mSkipTest = true;
            FAIL() << "Failed setting swap interval";
            // FAIL returns.
        }
    }

    if (mTestParams.trackGpuTime)
    {
        mIsTimestampQueryAvailable = EnsureGLExtensionEnabled("GL_EXT_disjoint_timer_query");
    }

    if (!areExtensionPrerequisitesFulfilled())
    {
        mSkipTest = true;
    }

    if (mSkipTest)
    {
        return;
    }

#if defined(ANGLE_ENABLE_ASSERTS)
    if (IsGLExtensionEnabled("GL_KHR_debug"))
    {
        EnableDebugCallback(&PerfTestDebugCallback, this);
    }
#endif

    initializeBenchmark();

    if (mSkipTest)
    {
        return;
    }

    if (mTestParams.iterationsPerStep == 0)
    {
        mSkipTest = true;
        FAIL() << "Please initialize 'iterationsPerStep'.";
        // FAIL returns.
    }

    if (gVerboseLogging)
    {
        printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
        printf("GL_VERSION: %s\n", glGetString(GL_VERSION));
    }

    mTestTrialResults.reserve(gTestTrials);

    for (int loopIndex = 0; loopIndex < gWarmupLoops; ++loopIndex)
    {
        doRunLoop(gCalibrationTimeSeconds, std::numeric_limits<int>::max(),
                  RunLoopPolicy::FinishEveryStep);
        if (gVerboseLogging)
        {
            printf("Warm-up loop took %.2lf seconds.\n", mTimer.getElapsedWallClockTime());
        }
    }

    if (mStepsToRun <= 0)
    {
        // Ensure we always call Finish when calibrating Render tests. This completes our work
        // between calibration measurements.
        calibrateStepsToRun(RunLoopPolicy::FinishEveryStep);
    }

    initPerfCounters();
}

void ANGLERenderTest::TearDown()
{
    if (!mSkipTest)
    {
        destroyBenchmark();
    }

    if (mGLWindow)
    {
        mGLWindow->destroyGL();
        mGLWindow = nullptr;
    }

    if (mOSWindow)
    {
        mOSWindow->destroy();
        mOSWindow = nullptr;
    }

    // Dump trace events to json file.
    if (gEnableTrace)
    {
        DumpTraceEventsToJSONFile(mTraceEventBuffer, gTraceFile);
    }

    ANGLEPerfTest::TearDown();
}

void ANGLERenderTest::initPerfCounters()
{
    if (!gPerfCounters)
    {
        return;
    }

    if (!IsGLExtensionEnabled(kPerfMonitorExtensionName))
    {
        fprintf(stderr, "Cannot report perf metrics because %s is not available.\n",
                kPerfMonitorExtensionName);
        return;
    }

    CounterNameToIndexMap indexMap = BuildCounterNameToIndexMap();

    std::vector<std::string> counters =
        angle::SplitString(gPerfCounters, ":", angle::WhitespaceHandling::TRIM_WHITESPACE,
                           angle::SplitResult::SPLIT_WANT_NONEMPTY);
    for (const std::string &counter : counters)
    {
        auto iter = indexMap.find(counter);
        if (iter == indexMap.end())
        {
            fprintf(stderr, "Counter '%s' not in list of available perf counters.\n",
                    counter.c_str());
        }
        else
        {
            {
                std::stringstream medianStr;
                medianStr << '.' << counter << "_median";
                std::string medianName = medianStr.str();
                mReporter->RegisterImportantMetric(medianName, "count");
            }

            {
                std::stringstream maxStr;
                maxStr << '.' << counter << "_max";
                std::string maxName = maxStr.str();
                mReporter->RegisterImportantMetric(maxName, "count");
            }

            GLuint index            = indexMap[counter];
            mPerfCounterInfo[index] = {counter, {}};
        }
    }
}

void ANGLERenderTest::updatePerfCounters()
{
    if (mPerfCounterInfo.empty())
    {
        return;
    }

    std::vector<PerfMonitorTriplet> perfData = GetPerfMonitorTriplets();
    ASSERT(!perfData.empty());

    for (auto &iter : mPerfCounterInfo)
    {
        uint32_t counter             = iter.first;
        std::vector<GLuint> &samples = iter.second.samples;
        samples.push_back(perfData[counter].value);
    }
}

void ANGLERenderTest::beginInternalTraceEvent(const char *name)
{
    if (gEnableTrace)
    {
        mTraceEventBuffer.emplace_back(TRACE_EVENT_PHASE_BEGIN, gTraceCategories[0].name, name,
                                       MonotonicallyIncreasingTime(&mPlatformMethods),
                                       getCurrentThreadSerial());
    }
}

void ANGLERenderTest::endInternalTraceEvent(const char *name)
{
    if (gEnableTrace)
    {
        mTraceEventBuffer.emplace_back(TRACE_EVENT_PHASE_END, gTraceCategories[0].name, name,
                                       MonotonicallyIncreasingTime(&mPlatformMethods),
                                       getCurrentThreadSerial());
    }
}

void ANGLERenderTest::beginGLTraceEvent(const char *name, double hostTimeSec)
{
    if (gEnableTrace)
    {
        mTraceEventBuffer.emplace_back(TRACE_EVENT_PHASE_BEGIN, gTraceCategories[1].name, name,
                                       hostTimeSec, getCurrentThreadSerial());
    }
}

void ANGLERenderTest::endGLTraceEvent(const char *name, double hostTimeSec)
{
    if (gEnableTrace)
    {
        mTraceEventBuffer.emplace_back(TRACE_EVENT_PHASE_END, gTraceCategories[1].name, name,
                                       hostTimeSec, getCurrentThreadSerial());
    }
}

void ANGLERenderTest::step()
{
    beginInternalTraceEvent("step");

    // Clear events that the application did not process from this frame
    Event event;
    bool closed = false;
    while (popEvent(&event))
    {
        // If the application did not catch a close event, close now
        if (event.Type == Event::EVENT_CLOSED)
        {
            closed = true;
        }
    }

    if (closed)
    {
        abortTest();
    }
    else
    {
        drawBenchmark();

        // Swap is needed so that the GPU driver will occasionally flush its
        // internal command queue to the GPU. This is enabled for null back-end
        // devices because some back-ends (e.g. Vulkan) also accumulate internal
        // command queues.
        if (mSwapEnabled)
        {
            updatePerfCounters();
            mGLWindow->swap();
        }
        mOSWindow->messageLoop();

#if defined(ANGLE_ENABLE_ASSERTS)
        if (!gRetraceMode)
        {
            EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
        }
#endif  // defined(ANGLE_ENABLE_ASSERTS)

        // Sample system memory
        uint64_t processMemoryUsageKB = GetProcessMemoryUsageKB();
        if (processMemoryUsageKB)
        {
            mProcessMemoryUsageKBSamples.push_back(processMemoryUsageKB);
        }
    }

    endInternalTraceEvent("step");
}

void ANGLERenderTest::startGpuTimer()
{
    if (mTestParams.trackGpuTime && mIsTimestampQueryAvailable)
    {
        glGenQueriesEXT(1, &mCurrentTimestampBeginQuery);
        glQueryCounterEXT(mCurrentTimestampBeginQuery, GL_TIMESTAMP_EXT);
    }
}

void ANGLERenderTest::stopGpuTimer()
{
    if (mTestParams.trackGpuTime && mIsTimestampQueryAvailable)
    {
        GLuint endQuery = 0;
        glGenQueriesEXT(1, &endQuery);
        glQueryCounterEXT(endQuery, GL_TIMESTAMP_EXT);
        mTimestampQueries.push_back({mCurrentTimestampBeginQuery, endQuery});
    }
}

void ANGLERenderTest::computeGPUTime()
{
    if (mTestParams.trackGpuTime && mIsTimestampQueryAvailable)
    {
        for (const TimestampSample &sample : mTimestampQueries)
        {
            uint64_t beginGLTimeNs = 0;
            uint64_t endGLTimeNs   = 0;
            glGetQueryObjectui64vEXT(sample.beginQuery, GL_QUERY_RESULT_EXT, &beginGLTimeNs);
            glGetQueryObjectui64vEXT(sample.endQuery, GL_QUERY_RESULT_EXT, &endGLTimeNs);
            glDeleteQueriesEXT(1, &sample.beginQuery);
            glDeleteQueriesEXT(1, &sample.endQuery);
            mGPUTimeNs += endGLTimeNs - beginGLTimeNs;
        }

        mTimestampQueries.clear();
    }
}

void ANGLERenderTest::startTest() {}

void ANGLERenderTest::finishTest()
{
    if (mTestParams.eglParameters.deviceType != EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE &&
        !gNoFinish && !gRetraceMode)
    {
        glFinish();
    }
}

bool ANGLERenderTest::popEvent(Event *event)
{
    return mOSWindow->popEvent(event);
}

OSWindow *ANGLERenderTest::getWindow()
{
    return mOSWindow;
}

GLWindowBase *ANGLERenderTest::getGLWindow()
{
    return mGLWindow;
}

bool ANGLERenderTest::areExtensionPrerequisitesFulfilled() const
{
    for (const char *extension : mExtensionPrerequisites)
    {
        if (!CheckExtensionExists(reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)),
                                  extension))
        {
            std::cout << "Test skipped due to missing extension: " << extension << std::endl;
            return false;
        }
    }
    return true;
}

void ANGLERenderTest::setWebGLCompatibilityEnabled(bool webglCompatibility)
{
    mConfigParams.webGLCompatibility = webglCompatibility;
}

void ANGLERenderTest::setRobustResourceInit(bool enabled)
{
    mConfigParams.robustResourceInit = enabled;
}

std::vector<TraceEvent> &ANGLERenderTest::getTraceEventBuffer()
{
    return mTraceEventBuffer;
}

void ANGLERenderTest::onErrorMessage(const char *errorMessage)
{
    abortTest();
    FAIL() << "Failing test because of unexpected error:\n" << errorMessage << "\n";
}

uint32_t ANGLERenderTest::getCurrentThreadSerial()
{
    std::thread::id id = std::this_thread::get_id();

    for (uint32_t serial = 0; serial < static_cast<uint32_t>(mThreadIDs.size()); ++serial)
    {
        if (mThreadIDs[serial] == id)
        {
            return serial + 1;
        }
    }

    mThreadIDs.push_back(id);
    return static_cast<uint32_t>(mThreadIDs.size());
}

namespace angle
{
double GetHostTimeSeconds()
{
    // Move the time origin to the first call to this function, to avoid generating unnecessarily
    // large timestamps.
    static double origin = GetCurrentSystemTime();
    return GetCurrentSystemTime() - origin;
}
}  // namespace angle
