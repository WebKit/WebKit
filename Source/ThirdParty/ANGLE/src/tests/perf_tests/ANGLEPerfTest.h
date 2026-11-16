//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ANGLEPerfTests:
//   Base class for google test performance tests
//

#ifndef PERF_TESTS_ANGLE_PERF_TEST_H_
#define PERF_TESTS_ANGLE_PERF_TEST_H_

#include <gtest/gtest.h>

#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include "libANGLE/trace.h"
#include "platform/PlatformMethods.h"
#include "test_utils/angle_test_configs.h"
#include "test_utils/angle_test_instantiate.h"
#include "test_utils/angle_test_platform.h"
#include "third_party/perf/perf_result_reporter.h"
#include "util/EGLWindow.h"
#include "util/OSWindow.h"
#include "util/Timer.h"
#include "util/shader_utils.h"
#include "util/util_gl.h"

class Event;

#if !defined(ASSERT_GL_NO_ERROR)
#    define ASSERT_GL_NO_ERROR() ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError())
#endif  // !defined(ASSERT_GL_NO_ERROR)

#if !defined(ASSERT_GLENUM_EQ)
#    define ASSERT_GLENUM_EQ(expected, actual) \
        ASSERT_EQ(static_cast<GLenum>(expected), static_cast<GLenum>(actual))
#endif  // !defined(ASSERT_GLENUM_EQ)

#if defined(ANGLE_USE_PERFETTO)
namespace perfetto
{
class TracingSession;
}  // namespace perfetto
#endif

enum class VulkanApiWallTimeTracking
{
    None,
    Summary,
    Detailed,
};

class ANGLEPerfTest : public testing::Test, angle::NonCopyable
{
  public:
    ANGLEPerfTest(const std::string &name,
                  const std::string &backend,
                  const std::string &story,
                  unsigned int iterationsPerStep,
                  const char *units = "ns");
    ~ANGLEPerfTest() override;

    virtual void step() = 0;

    // Called right after the timer starts to let the test initialize other metrics if necessary
    virtual void startTest() {}
    // Called right before timer is stopped to let the test wait for asynchronous operations.
    virtual void finishTest() {}
    virtual void flush() {}

    // Can be overridden in child tests that require a certain number of steps per trial.
    virtual int getStepAlignment() const;

    virtual bool isRenderTest() const { return false; }

  protected:
    enum class RunTrialPolicy
    {
        FinishEveryStep,
        RunContinuouslyWarmup,
        RunContinuously,
    };

    void run();
    void SetUp() override;
    void TearDown() override;

    // Normalize a time value according to the number of test trial iterations (mFrameCount)
    double normalizedTime(size_t value) const;

    // Call if the test step was aborted and the test should stop running.
    void abortTest() { mRunning = false; }

    int getNumStepsPerformed() const { return mTrialNumStepsPerformed; }

    bool isVulkanApiWallTimeTrackingActive() const
    {
        return mVulkanApiWallTimeTracking != VulkanApiWallTimeTracking::None;
    }

    void runTrial(double maxRunTime, int maxStepsToRun, RunTrialPolicy runPolicy);
    void resetVulkanApiCounters();

    // Overriden in trace perf tests.
    virtual void computeGPUTime() {}

    void calibrateStepsToRun();
    int estimateStepsToRun() const;

    void recordIntegerMetric(const char *metric, size_t value, const std::string &units);
    void recordDoubleMetric(const char *metric, double value, const std::string &units);
    void addHistogramSample(const char *metric, double value, const std::string &units);

    void processResults();
    void processClockResult(const char *metric, double resultSeconds);
    void processMemoryResult(const char *metric, uint64_t resultKB);
    void processVulkanApiCounters();

    void skipTest(const std::string &reason)
    {
        mSkipTestReason = reason;
        mSkipTest       = true;
    }

    void failTest(const std::string &reason)
    {
        skipTest(reason);
        FAIL() << reason;
    }

    void atraceCounter(const char *counterName, int64_t counterValue);

    std::string mName;
    std::string mBackend;
    std::string mStory;
    Timer mTrialTimer;
    uint64_t mGPUTimeNs;
    double mFrameWallTimeSec;
    double mBusyWaitCpuTimeSec;
    bool mSkipTest;
    std::string mSkipTestReason;
    std::unique_ptr<perf_test::PerfResultReporter> mReporter;
    int mStepsToRun;
    int mTrialNumStepsPerformed;
    int mTotalNumStepsPerformed;
    int mIterationsPerStep;
    bool mRunning;
    std::vector<double> mTestTrialResults;

    struct CounterInfo
    {
        std::string name;
        std::vector<GLuint64> samples;
    };
    std::map<GLuint, CounterInfo> mPerfCounterInfo;

    struct VulkanApiCounterInfo
    {
        GLuint counterIndex;
        std::string metricName;
        uint64_t count;
    };
    template <typename T>
    using VulkanApiCounterMap =
        angle::PackedEnumMap<angle::VulkanApiPerfCounterType,
                             angle::PackedEnumMap<angle::VulkanApiPerfCounterGroup, T>>;
    VulkanApiCounterMap<VulkanApiCounterInfo> mVulkanApiCounterInfos;
    VulkanApiWallTimeTracking mVulkanApiWallTimeTracking;

    GLuint mPerfMonitor;
    bool mPerfMonitorReady;

    std::vector<uint64_t> mProcessMemoryUsageKBSamples;
#if defined(ANGLE_USE_PERFETTO)
    std::unique_ptr<perfetto::TracingSession> mTracingSession;
#endif
};

enum class SurfaceType
{
    Window,
    WindowWithVSync,
    Offscreen,
};

struct RenderTestParams : public angle::PlatformParameters
{
    RenderTestParams();
    virtual ~RenderTestParams() {}

    virtual std::string backend() const;
    virtual std::string story() const;
    std::string backendAndStory() const;

    EGLint windowWidth             = 64;
    EGLint windowHeight            = 64;
    unsigned int iterationsPerStep = 0;
    bool trackGpuTime              = false;
    SurfaceType surfaceType        = SurfaceType::Window;
    EGLenum colorSpace             = EGL_COLORSPACE_LINEAR;
    bool multisample               = false;
    EGLint samples                 = -1;
    bool isCL                      = false;

    VulkanApiWallTimeTracking vulkanApiWallTimeTracking = VulkanApiWallTimeTracking::None;
};

class ANGLERenderTest : public ANGLEPerfTest
{
  public:
    ANGLERenderTest(const std::string &name,
                    const RenderTestParams &testParams,
                    const char *units = "ns");
    ~ANGLERenderTest() override;

    void addExtensionPrerequisite(std::string extensionName);
    void addIntegerPrerequisite(GLenum target, int min);

    virtual void initializeBenchmark() {}
    virtual void destroyBenchmark() {}

    virtual void drawBenchmark() = 0;

    bool popEvent(Event *event);

    OSWindow *getWindow();
    GLWindowBase *getGLWindow();

    virtual void overrideWorkaroundsD3D(angle::FeaturesD3D *featuresD3D) {}
    void onErrorMessage(const char *errorMessage);

    uint32_t getCurrentThreadSerial();
    bool isRenderTest() const override { return true; }

  protected:
    const RenderTestParams &mTestParams;

    void setWebGLCompatibilityEnabled(bool webglCompatibility);
    void setHardenedContextEnabled(bool hardenedContext);
    void setRobustResourceInit(bool enabled);

    virtual void startGpuTimer();
    virtual void stopGpuTimer(bool mayNeedFlush = true);

    void disableTestHarnessSwap() { mSwapEnabled = false; }
    void updatePerfCounters();

    void startVulkanApiTimer();
    void stopVulkanApiTimer();

    bool mIsTimestampQueryAvailable;
    bool mEnableDebugCallback = true;

    void startTest() override;
    void finishTest() override;

    // non-const, so tests (e.g., TracePerfTest,
    // ProgramPipelineObjectBenchmark) can set the values they need.
    ConfigParameters &getConfigParams() { return mConfigParams; }

  private:
    void SetUp() override;
    void TearDown() override;

    void step() override;
    void computeGPUTime() override;

    void skipTestIfMissingExtensionPrerequisites();
    void skipTestIfFailsIntegerPrerequisite();

    void initPerfCounters();
    void initVulkanApiCounters(const CounterNameToIndexMap &indexMap);

    bool isVulkanApiWallTimeTrackingEnabled() const
    {
        return mTestParams.vulkanApiWallTimeTracking != VulkanApiWallTimeTracking::None;
    }

    GLWindowBase *mGLWindow;
    OSWindow *mOSWindow;
    std::vector<std::string> mExtensionPrerequisites;
    struct IntegerPrerequisite
    {
        GLenum target;
        int min;
    };
    std::vector<IntegerPrerequisite> mIntegerPrerequisites;
    angle::PlatformMethods mPlatformMethods;
    ConfigParameters mConfigParams;
    bool mSwapEnabled;

    enum class EndQueryFlushPolicy
    {
        NoFlush,
        Flush,
        FenceSync
    };

    struct TimestampSample
    {
        GLuint beginQuery;
        GLuint endQuery;
    };

    GLuint mCurrentTimestampBeginQuery = 0;
    std::queue<TimestampSample> mTimestampQueries;
    EndQueryFlushPolicy mEndQueryFlushPolicy = EndQueryFlushPolicy::NoFlush;

    VulkanApiCounterMap<uint64_t> mCurrentVulkanApiCounterBeginValues;

    // Handle to the entry point binding library.
    std::unique_ptr<angle::Library> mEntryPointsLib;

    std::vector<uint64_t> mThreadIDs;
};

// Mixins.
namespace params
{
template <typename ParamsT>
ParamsT Offscreen(const ParamsT &input)
{
    ParamsT output     = input;
    output.surfaceType = SurfaceType::Offscreen;
    return output;
}

template <typename ParamsT>
ParamsT NullDevice(const ParamsT &input)
{
    ParamsT output                  = input;
    output.eglParameters.deviceType = EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE;
    output.trackGpuTime             = false;
    return output;
}

template <typename ParamsT>
ParamsT Passthrough(const ParamsT &input)
{
    return input;
}
}  // namespace params

namespace angle
{
// Returns the time of the host since the application started in seconds.
double GetHostTimeSeconds();
}  // namespace angle
#endif  // PERF_TESTS_ANGLE_PERF_TEST_H_
