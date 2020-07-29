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
#include "third_party/perf/perf_test.h"
#include "third_party/trace_event/trace_event.h"
#include "util/shader_utils.h"
#include "util/test_utils.h"

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

#include <json/json.h>

#if defined(ANGLE_USE_UTIL_LOADER) && defined(ANGLE_PLATFORM_WINDOWS)
#    include "util/windows/WGLWindow.h"
#endif  // defined(ANGLE_USE_UTIL_LOADER) &&defined(ANGLE_PLATFORM_WINDOWS)

using namespace angle;

namespace
{
constexpr size_t kInitialTraceEventBufferSize = 50000;
constexpr double kMicroSecondsPerSecond       = 1e6;
constexpr double kNanoSecondsPerSecond        = 1e9;
constexpr double kCalibrationRunTimeSeconds   = 1.0;
constexpr double kMaximumRunTimeSeconds       = 10.0;
constexpr unsigned int kNumTrials             = 3;

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

    ANGLERenderTest *renderTest     = static_cast<ANGLERenderTest *>(platform->context);
    std::vector<TraceEvent> &buffer = renderTest->getTraceEventBuffer();
    buffer.emplace_back(phase, category->name, name, timestamp);
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

void DumpTraceEventsToJSONFile(const std::vector<TraceEvent> &traceEvents,
                               const char *outputFileName)
{
    Json::Value eventsValue(Json::arrayValue);

    for (const TraceEvent &traceEvent : traceEvents)
    {
        Json::Value value(Json::objectValue);

        const auto microseconds = static_cast<Json::LargestInt>(traceEvent.timestamp) * 1000 * 1000;

        value["name"] = traceEvent.name;
        value["cat"]  = traceEvent.categoryName;
        value["ph"]   = std::string(1, traceEvent.phase);
        value["ts"]   = microseconds;
        value["pid"]  = strcmp(traceEvent.categoryName, "gpu.angle.gpu") == 0 ? "GPU" : "ANGLE";
        value["tid"]  = 1;

        eventsValue.append(std::move(value));
    }

    Json::Value root(Json::objectValue);
    root["traceEvents"] = eventsValue;

    std::ofstream outFile;
    outFile.open(outputFileName);

    Json::StreamWriterBuilder factory;
    std::unique_ptr<Json::StreamWriter> const writer(factory.newStreamWriter());
    std::ostringstream stream;
    writer->write(root, &outFile);

    outFile.close();
}

ANGLE_MAYBE_UNUSED void KHRONOS_APIENTRY DebugMessageCallback(GLenum source,
                                                              GLenum type,
                                                              GLuint id,
                                                              GLenum severity,
                                                              GLsizei length,
                                                              const GLchar *message,
                                                              const void *userParam)
{
    std::string sourceText   = gl::GetDebugMessageSourceString(source);
    std::string typeText     = gl::GetDebugMessageTypeString(type);
    std::string severityText = gl::GetDebugMessageSeverityString(severity);
    std::cerr << sourceText << ", " << typeText << ", " << severityText << ": " << message << "\n";
}
}  // anonymous namespace

TraceEvent::TraceEvent(char phaseIn,
                       const char *categoryNameIn,
                       const char *nameIn,
                       double timestampIn)
    : phase(phaseIn), categoryName(categoryNameIn), name{}, timestamp(timestampIn), tid(1)
{
    ASSERT(strlen(nameIn) < kMaxNameLen);
    strcpy(name, nameIn);
}

ANGLEPerfTest::ANGLEPerfTest(const std::string &name,
                             const std::string &backend,
                             const std::string &story,
                             unsigned int iterationsPerStep)
    : mName(name),
      mBackend(backend),
      mStory(story),
      mGPUTimeNs(0),
      mSkipTest(false),
      mStepsToRun(gStepsToRunOverride),
      mNumStepsPerformed(0),
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
    mReporter->RegisterImportantMetric(".wall_time", "ns");
    mReporter->RegisterImportantMetric(".gpu_time", "ns");
    mReporter->RegisterFyiMetric(".steps", "count");
}

ANGLEPerfTest::~ANGLEPerfTest() {}

void ANGLEPerfTest::run()
{
    if (mSkipTest)
    {
        return;
    }

    // Calibrate to a fixed number of steps during an initial set time.
    if (mStepsToRun <= 0)
    {
        calibrateStepsToRun();
    }

    // Check again for early exit.
    if (mSkipTest)
    {
        return;
    }

    // Do another warmup run. Seems to consistently improve results.
    doRunLoop(kMaximumRunTimeSeconds);

    for (unsigned int trial = 0; trial < kNumTrials; ++trial)
    {
        doRunLoop(kMaximumRunTimeSeconds);
        printResults();
        if (gVerboseLogging)
        {
            printf("Trial %d time: %.2lf seconds.\n", trial + 1, mTimer.getElapsedTime());
        }
    }
}

void ANGLEPerfTest::doRunLoop(double maxRunTime)
{
    mNumStepsPerformed = 0;
    mRunning           = true;
    mTimer.start();
    startTest();

    while (mRunning)
    {
        step();
        if (mRunning)
        {
            ++mNumStepsPerformed;
            if (mTimer.getElapsedTime() > maxRunTime)
            {
                mRunning = false;
            }
            else if (mNumStepsPerformed >= mStepsToRun)
            {
                mRunning = false;
            }
        }
    }
    finishTest();
    mTimer.stop();
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
        double secondsPerStep = elapsedTimeSeconds[i] / static_cast<double>(mNumStepsPerformed);
        double secondsPerIteration = secondsPerStep / static_cast<double>(mIterationsPerStep);

        perf_test::MetricInfo metricInfo;
        std::string units;
        // Lazily register the metric, re-using the existing units if it is
        // already registered.
        if (!mReporter->GetMetricInfo(clockNames[i], &metricInfo))
        {
            units = secondsPerIteration > 1e-3 ? "us" : "ns";
            mReporter->RegisterImportantMetric(clockNames[i], units);
        }
        else
        {
            units = metricInfo.units;
        }

        if (units == "us")
        {
            retValue = secondsPerIteration * kMicroSecondsPerSecond;
        }
        else
        {
            retValue = secondsPerIteration * kNanoSecondsPerSecond;
        }
        mReporter->AddResult(clockNames[i], retValue);
    }
    return retValue;
}

double ANGLEPerfTest::normalizedTime(size_t value) const
{
    return static_cast<double>(value) / static_cast<double>(mNumStepsPerformed);
}

void ANGLEPerfTest::calibrateStepsToRun()
{
    // First do two warmup loops. There's no science to this. Two loops was experimentally helpful
    // on a Windows NVIDIA setup when testing with Vulkan and native trace tests.
    for (int i = 0; i < 2; ++i)
    {
        doRunLoop(kCalibrationRunTimeSeconds);
        if (gVerboseLogging)
        {
            printf("Pre-calibration warm-up took %.2lf seconds.\n", mTimer.getElapsedTime());
        }
    }

    // Now the real computation.
    doRunLoop(kCalibrationRunTimeSeconds);

    double elapsedTime = mTimer.getElapsedTime();

    // Scale steps down according to the time that exeeded one second.
    double scale = kCalibrationRunTimeSeconds / elapsedTime;
    mStepsToRun  = static_cast<unsigned int>(static_cast<double>(mNumStepsPerformed) * scale);

    if (gVerboseLogging)
    {
        printf(
            "Running %d steps (calibration took %.2lf seconds). Expecting trial time of %.2lf "
            "seconds.\n",
            mStepsToRun, elapsedTime,
            mStepsToRun * (elapsedTime / static_cast<double>(mNumStepsPerformed)));
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

    if (eglParameters.deviceType == EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE)
    {
        strstr << "_null";
    }

    return strstr.str();
}

std::string RenderTestParams::story() const
{
    return (surfaceType == SurfaceType::Offscreen ? "_offscreen" : "");
}

std::string RenderTestParams::backendAndStory() const
{
    return backend() + story();
}

ANGLERenderTest::ANGLERenderTest(const std::string &name, const RenderTestParams &testParams)
    : ANGLEPerfTest(name,
                    testParams.backend(),
                    testParams.story(),
                    OneFrame() ? 1 : testParams.iterationsPerStep),
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
    mPlatformMethods.logError                    = EmptyPlatformMethod;
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
    if (!mGLWindow->setSwapInterval(0))
    {
        mSkipTest = true;
        FAIL() << "Failed setting swap interval";
        // FAIL returns.
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
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        // Enable medium and high priority messages.
        glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_HIGH, 0, nullptr,
                                 GL_TRUE);
        glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_MEDIUM, 0, nullptr,
                                 GL_TRUE);
        // Disable low and notification priority messages.
        glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW, 0, nullptr,
                                 GL_FALSE);
        glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0,
                                 nullptr, GL_FALSE);
        // Disable medium priority performance messages to reduce spam.
        glDebugMessageControlKHR(GL_DONT_CARE, GL_DEBUG_TYPE_PERFORMANCE, GL_DEBUG_SEVERITY_MEDIUM,
                                 0, nullptr, GL_FALSE);
        glDebugMessageCallbackKHR(DebugMessageCallback, this);
    }
#endif

    initializeBenchmark();

    if (mTestParams.iterationsPerStep == 0)
    {
        mSkipTest = true;
        FAIL() << "Please initialize 'iterationsPerStep'.";
        // FAIL returns.
    }

    // Capture a screenshot if enabled.
    if (gScreenShotDir != nullptr)
    {
        std::stringstream screenshotNameStr;
        screenshotNameStr << gScreenShotDir << GetPathSeparator() << "angle" << mBackend << "_"
                          << mStory << ".png";
        std::string screenshotName = screenshotNameStr.str();
        saveScreenshot(screenshotName);
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
                                       MonotonicallyIncreasingTime(&mPlatformMethods));
    }
}

void ANGLERenderTest::endInternalTraceEvent(const char *name)
{
    if (gEnableTrace)
    {
        mTraceEventBuffer.emplace_back(TRACE_EVENT_PHASE_END, gTraceCategories[0].name, name,
                                       MonotonicallyIncreasingTime(&mPlatformMethods));
    }
}

void ANGLERenderTest::beginGLTraceEvent(const char *name, double hostTimeSec)
{
    if (gEnableTrace)
    {
        mTraceEventBuffer.emplace_back(TRACE_EVENT_PHASE_BEGIN, gTraceCategories[1].name, name,
                                       hostTimeSec);
    }
}

void ANGLERenderTest::endGLTraceEvent(const char *name, double hostTimeSec)
{
    if (gEnableTrace)
    {
        mTraceEventBuffer.emplace_back(TRACE_EVENT_PHASE_END, gTraceCategories[1].name, name,
                                       hostTimeSec);
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
        glBeginQueryEXT(GL_TIME_ELAPSED_EXT, mTimestampQuery);
    }
}

void ANGLERenderTest::stopGpuTimer()
{
    if (mTestParams.trackGpuTime && mIsTimestampQueryAvailable)
    {
        glEndQueryEXT(GL_TIME_ELAPSED_EXT);
        uint64_t gpuTimeNs = 0;
        glGetQueryObjectui64vEXT(mTimestampQuery, GL_QUERY_RESULT_EXT, &gpuTimeNs);

        mGPUTimeNs += gpuTimeNs;
    }
}

void ANGLERenderTest::startTest()
{
    if (mTestParams.trackGpuTime)
    {
        glGenQueriesEXT(1, &mTimestampQuery);
        mGPUTimeNs = 0;
    }
}

void ANGLERenderTest::finishTest()
{
    if (mTestParams.trackGpuTime)
    {
        glDeleteQueriesEXT(1, &mTimestampQuery);
    }
    if (mTestParams.eglParameters.deviceType != EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE)
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
