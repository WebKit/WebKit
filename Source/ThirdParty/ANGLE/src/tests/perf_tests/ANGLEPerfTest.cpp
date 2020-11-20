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
#include "common/platform.h"
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

struct TraceCategory
{
    unsigned char enabled;
    const char *name;
};

constexpr TraceCategory gTraceCategories[2] = {
    {1, "gpu.angle"},
    {1, "gpu.angle.gpu"},
};

void EmptyPlatformMethod(angle::PlatformMethods *, const char *) {}

void CustomLogError(angle::PlatformMethods *platform, const char *errorMessage)
{
    auto *angleRenderTest = static_cast<ANGLERenderTest *>(platform->context);
    angleRenderTest->onErrorMessage(errorMessage);
}

void OverrideWorkaroundsD3D(angle::PlatformMethods *platform, angle::FeaturesD3D *featuresD3D)
{
    auto *angleRenderTest = static_cast<ANGLERenderTest *>(platform->context);
    angleRenderTest->overrideWorkaroundsD3D(featuresD3D);
}

angle::TraceEventHandle AddPerfTraceEvent(angle::PlatformMethods *platform,
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

    uint32_t tid = renderTest->getCurrentThreadSerial();

    std::vector<TraceEvent> &buffer = renderTest->getTraceEventBuffer();
    buffer.emplace_back(phase, category->name, name, timestamp, tid);
    return buffer.size();
}

const unsigned char *GetPerfTraceCategoryEnabled(angle::PlatformMethods *platform,
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

void UpdateTraceEventDuration(angle::PlatformMethods *platform,
                              const unsigned char *categoryEnabledFlag,
                              const char *name,
                              angle::TraceEventHandle eventHandle)
{
    // Not implemented.
}

double MonotonicallyIncreasingTime(angle::PlatformMethods *platform)
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
      mStepsPerRunLoopStep(1),
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
    mReporter->RegisterImportantMetric(".gpu_time", units);
    mReporter->RegisterFyiMetric(".trial_steps", "count");
    mReporter->RegisterFyiMetric(".total_steps", "count");
}

ANGLEPerfTest::~ANGLEPerfTest() {}

void ANGLEPerfTest::run()
{
    if (mSkipTest)
    {
        return;
    }

    uint32_t numTrials = OneFrame() ? 1 : gTestTrials;
    if (gVerboseLogging)
    {
        printf("Test Trials: %d\n", static_cast<int>(numTrials));
    }

    for (uint32_t trial = 0; trial < numTrials; ++trial)
    {
        doRunLoop(gTestTimeSeconds, mStepsToRun, RunLoopPolicy::RunContinuously);
        printResults();
        if (gVerboseLogging)
        {
            double trialTime = mTimer.getElapsedTime();
            printf("Trial %d time: %.2lf seconds.\n", trial + 1, trialTime);

            double secondsPerStep      = trialTime / static_cast<double>(mTrialNumStepsPerformed);
            double secondsPerIteration = secondsPerStep / static_cast<double>(mIterationsPerStep);
            mTestTrialResults.push_back(secondsPerIteration * 1000.0);
        }
    }

    if (gVerboseLogging)
    {
        double numResults = static_cast<double>(mTestTrialResults.size());
        double mean       = 0;
        for (double trialResult : mTestTrialResults)
        {
            mean += trialResult;
        }
        mean /= numResults;

        double variance = 0;
        for (double trialResult : mTestTrialResults)
        {
            double difference = trialResult - mean;
            variance += difference * difference;
        }
        variance /= numResults;

        double standardDeviation      = std::sqrt(variance);
        double coefficientOfVariation = standardDeviation / mean;

        printf("Mean result time: %.4lf ms.\n", mean);
        printf("Coefficient of variation: %.2lf%%\n", coefficientOfVariation * 100.0);
    }
}

void ANGLEPerfTest::setStepsPerRunLoopStep(int stepsPerRunLoop)
{
    ASSERT(stepsPerRunLoop >= 1);
    mStepsPerRunLoopStep = stepsPerRunLoop;
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
        step();

        if (runPolicy == RunLoopPolicy::FinishEveryStep)
        {
            glFinish();
        }

        if (mRunning)
        {
            mTrialNumStepsPerformed += mStepsPerRunLoopStep;
            mTotalNumStepsPerformed += mStepsPerRunLoopStep;
            if (mTimer.getElapsedTime() > maxRunTime)
            {
                mRunning = false;
            }
            else if (mTrialNumStepsPerformed >= maxStepsToRun)
            {
                mRunning = false;
            }
            else if (gMaxStepsPerformed > 0 && mTotalNumStepsPerformed >= gMaxStepsPerformed)
            {
                mRunning = false;
            }
        }
    }
    finishTest();
    mTimer.stop();
    computeGPUTime();
}

void ANGLEPerfTest::SetUp() {}

void ANGLEPerfTest::TearDown() {}

double ANGLEPerfTest::printResults()
{
    double elapsedTimeSeconds[2] = {
        mTimer.getElapsedTime(),
        mGPUTimeNs * 1e-9,
    };

    const char *clockNames[2] = {
        ".wall_time",
        ".gpu_time",
    };

    // If measured gpu time is non-zero, print that too.
    size_t clocksToOutput = mGPUTimeNs > 0 ? 2 : 1;

    double retValue = 0.0;
    for (size_t i = 0; i < clocksToOutput; ++i)
    {
        double secondsPerStep =
            elapsedTimeSeconds[i] / static_cast<double>(mTrialNumStepsPerformed);
        double secondsPerIteration = secondsPerStep / static_cast<double>(mIterationsPerStep);

        perf_test::MetricInfo metricInfo;
        std::string units;
        // Lazily register the metric, re-using the existing units if it is
        // already registered.
        if (!mReporter->GetMetricInfo(clockNames[i], &metricInfo))
        {
            printf("Seconds per iteration: %lf\n", secondsPerIteration);
            units = secondsPerIteration > 1e-3 ? "us" : "ns";
            mReporter->RegisterImportantMetric(clockNames[i], units);
        }
        else
        {
            units = metricInfo.units;
        }

        if (units == "ms")
        {
            retValue = secondsPerIteration * kMilliSecondsPerSecond;
        }
        else if (units == "us")
        {
            retValue = secondsPerIteration * kMicroSecondsPerSecond;
        }
        else
        {
            retValue = secondsPerIteration * kNanoSecondsPerSecond;
        }
        mReporter->AddResult(clockNames[i], retValue);
    }

    if (gVerboseLogging)
    {
        double fps = static_cast<double>(mTrialNumStepsPerformed * mIterationsPerStep) /
                     elapsedTimeSeconds[0];
        printf("Ran %0.2lf iterations per second\n", fps);
    }

    mReporter->AddResult(".trial_steps", static_cast<size_t>(mTrialNumStepsPerformed));
    mReporter->AddResult(".total_steps", static_cast<size_t>(mTotalNumStepsPerformed));

    // Output histogram JSON set format if enabled.
    double secondsPerStep = elapsedTimeSeconds[0] / static_cast<double>(mTrialNumStepsPerformed);
    double secondsPerIteration = secondsPerStep / static_cast<double>(mIterationsPerStep);
    TestSuite::GetInstance()->addHistogramSample(
        mName + mBackend, mStory, secondsPerIteration * kMilliSecondsPerSecond, "msBestFitFormat");
    return retValue;
}

double ANGLEPerfTest::normalizedTime(size_t value) const
{
    return static_cast<double>(value) / static_cast<double>(mTrialNumStepsPerformed);
}

void ANGLEPerfTest::calibrateStepsToRun()
{
    doRunLoop(gCalibrationTimeSeconds, std::numeric_limits<int>::max(),
              RunLoopPolicy::FinishEveryStep);

    double elapsedTime = mTimer.getElapsedTime();

    // Scale steps down according to the time that exeeded one second.
    double scale = gCalibrationTimeSeconds / elapsedTime;
    mStepsToRun  = static_cast<unsigned int>(static_cast<double>(mTrialNumStepsPerformed) * scale);
    mStepsToRun  = std::max(1, mStepsToRun);

    if (gVerboseLogging)
    {
        printf(
            "Running %d steps (calibration took %.2lf seconds). Expecting trial time of %.2lf "
            "seconds.\n",
            mStepsToRun, elapsedTime,
            mStepsToRun * (elapsedTime / static_cast<double>(mTrialNumStepsPerformed)));
    }

    // Calibration allows the perf test runner script to save some time.
    if (gCalibration)
    {
        mReporter->AddResult(".steps", static_cast<size_t>(mStepsToRun));
        return;
    }
}

std::string RenderTestParams::backend() const
{
    std::stringstream strstr;

    switch (driver)
    {
        case angle::GLESDriverType::AngleEGL:
            break;
        case angle::GLESDriverType::SystemWGL:
        case angle::GLESDriverType::SystemEGL:
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
        case angle::GLESDriverType::AngleEGL:
            mGLWindow = EGLWindow::New(testParams.majorVersion, testParams.minorVersion);
            mEntryPointsLib.reset(angle::OpenSharedLibrary(ANGLE_EGL_LIBRARY_NAME,
                                                           angle::SearchType::ApplicationDir));
            break;
        case angle::GLESDriverType::SystemEGL:
#if defined(ANGLE_USE_UTIL_LOADER) && !defined(ANGLE_PLATFORM_WINDOWS)
            mGLWindow = EGLWindow::New(testParams.majorVersion, testParams.minorVersion);
            mEntryPointsLib.reset(
                angle::OpenSharedLibraryWithExtension(GetNativeEGLLibraryNameWithExtension()));
#else
            std::cerr << "Not implemented." << std::endl;
            mSkipTest = true;
#endif  // defined(ANGLE_USE_UTIL_LOADER) && !defined(ANGLE_PLATFORM_WINDOWS)
            break;
        case angle::GLESDriverType::SystemWGL:
#if defined(ANGLE_USE_UTIL_LOADER) && defined(ANGLE_PLATFORM_WINDOWS)
            mGLWindow = WGLWindow::New(testParams.majorVersion, testParams.minorVersion);
            mEntryPointsLib.reset(
                angle::OpenSharedLibrary("opengl32", angle::SearchType::SystemDir));
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
    angle::StabilizeCPUForBenchmarking();

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

    if (!mGLWindow->initializeGL(mOSWindow, mEntryPointsLib.get(), mTestParams.driver, withMethods,
                                 mConfigParams))
    {
        mSkipTest = true;
        FAIL() << "Failed initializing GL Window";
        // FAIL returns.
    }

    // Disable vsync.
    if (mTestParams.surfaceType != SurfaceType::WindowWithVSync)
    {
        if (!mGLWindow->setSwapInterval(0))
        {
            mSkipTest = true;
            FAIL() << "Failed setting swap interval";
            // FAIL returns.
        }
    }

    mIsTimestampQueryAvailable = IsGLExtensionEnabled("GL_EXT_disjoint_timer_query");

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
        EnableDebugCallback(this);
    }
#endif

    initializeBenchmark();

    if (mTestParams.iterationsPerStep == 0)
    {
        mSkipTest = true;
        FAIL() << "Please initialize 'iterationsPerStep'.";
        // FAIL returns.
    }

    mTestTrialResults.reserve(gTestTrials);

    // Capture a screenshot if enabled.
    if (gScreenShotDir != nullptr)
    {
        std::stringstream screenshotNameStr;
        screenshotNameStr << gScreenShotDir << GetPathSeparator() << "angle" << mBackend << "_"
                          << mStory << ".png";
        std::string screenshotName = screenshotNameStr.str();
        saveScreenshot(screenshotName);
    }

    for (int loopIndex = 0; loopIndex < gWarmupLoops; ++loopIndex)
    {
        doRunLoop(gCalibrationTimeSeconds, std::numeric_limits<int>::max(),
                  RunLoopPolicy::FinishEveryStep);
        if (gVerboseLogging)
        {
            printf("Warm-up loop took %.2lf seconds.\n", mTimer.getElapsedTime());
        }
    }

    if (mStepsToRun <= 0)
    {
        calibrateStepsToRun();
    }
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
            mGLWindow->swap();
        }
        mOSWindow->messageLoop();

#if defined(ANGLE_ENABLE_ASSERTS)
        EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
#endif  // defined(ANGLE_ENABLE_ASSERTS)
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
        !gNoFinish)
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
    FAIL() << "Failing test because of unexpected internal ANGLE error:\n" << errorMessage << "\n";
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
    static double origin = angle::GetCurrentTime();
    return angle::GetCurrentTime() - origin;
}
}  // namespace angle
