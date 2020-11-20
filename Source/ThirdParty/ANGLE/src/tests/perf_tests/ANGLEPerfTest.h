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

#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "platform/PlatformMethods.h"
#include "test_utils/angle_test_configs.h"
#include "test_utils/angle_test_instantiate.h"
#include "test_utils/angle_test_platform.h"
#include "third_party/perf/perf_result_reporter.h"
#include "util/EGLWindow.h"
#include "util/OSWindow.h"
#include "util/Timer.h"
#include "util/util_gl.h"

class Event;

#if !defined(ASSERT_GL_NO_ERROR)
#    define ASSERT_GL_NO_ERROR() ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError())
#endif  // !defined(ASSERT_GL_NO_ERROR)

#if !defined(ASSERT_GLENUM_EQ)
#    define ASSERT_GLENUM_EQ(expected, actual) \
        ASSERT_EQ(static_cast<GLenum>(expected), static_cast<GLenum>(actual))
#endif  // !defined(ASSERT_GLENUM_EQ)

// These are trace events according to Google's "Trace Event Format".
// See https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU
// Only a subset of the properties are implemented.
struct TraceEvent final
{
    TraceEvent() {}
    TraceEvent(char phaseIn,
               const char *categoryNameIn,
               const char *nameIn,
               double timestampIn,
               uint32_t tidIn);

    static constexpr uint32_t kMaxNameLen = 64;

    char phase               = 0;
    const char *categoryName = nullptr;
    char name[kMaxNameLen]   = {};
    double timestamp         = 0;
    uint32_t tid             = 0;
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

  protected:
    enum class RunLoopPolicy
    {
        FinishEveryStep,
        RunContinuously,
    };

    void run();
    void SetUp() override;
    void TearDown() override;

    // Normalize a time value according to the number of test loop iterations (mFrameCount)
    double normalizedTime(size_t value) const;

    // Call if the test step was aborted and the test should stop running.
    void abortTest() { mRunning = false; }

    int getNumStepsPerformed() const { return mTrialNumStepsPerformed; }

    // Defaults to one step per run loop. Can be changed in any test.
    void setStepsPerRunLoopStep(int stepsPerRunLoop);
    void doRunLoop(double maxRunTime, int maxStepsToRun, RunLoopPolicy runPolicy);

    // Overriden in trace perf tests.
    virtual void saveScreenshot(const std::string &screenshotName) {}
    virtual void computeGPUTime() {}

    double printResults();
    void calibrateStepsToRun();

    std::string mName;
    std::string mBackend;
    std::string mStory;
    Timer mTimer;
    uint64_t mGPUTimeNs;
    bool mSkipTest;
    std::unique_ptr<perf_test::PerfResultReporter> mReporter;
    int mStepsToRun;
    int mTrialNumStepsPerformed;
    int mTotalNumStepsPerformed;
    int mStepsPerRunLoopStep;
    int mIterationsPerStep;
    bool mRunning;
    std::vector<double> mTestTrialResults;
};

enum class SurfaceType
{
    Window,
    WindowWithVSync,
    Offscreen,
};

struct RenderTestParams : public angle::PlatformParameters
{
    virtual ~RenderTestParams() {}

    virtual std::string backend() const;
    virtual std::string story() const;
    std::string backendAndStory() const;

    EGLint windowWidth             = 64;
    EGLint windowHeight            = 64;
    unsigned int iterationsPerStep = 0;
    bool trackGpuTime              = false;
    SurfaceType surfaceType        = SurfaceType::Window;
};

class ANGLERenderTest : public ANGLEPerfTest
{
  public:
    ANGLERenderTest(const std::string &name,
                    const RenderTestParams &testParams,
                    const char *units = "ns");
    ~ANGLERenderTest() override;

    void addExtensionPrerequisite(const char *extensionName);

    virtual void initializeBenchmark() {}
    virtual void destroyBenchmark() {}

    virtual void drawBenchmark() = 0;

    bool popEvent(Event *event);

    OSWindow *getWindow();
    GLWindowBase *getGLWindow();

    std::vector<TraceEvent> &getTraceEventBuffer();

    virtual void overrideWorkaroundsD3D(angle::FeaturesD3D *featuresD3D) {}
    void onErrorMessage(const char *errorMessage);

    uint32_t getCurrentThreadSerial();

  protected:
    const RenderTestParams &mTestParams;

    void setWebGLCompatibilityEnabled(bool webglCompatibility);
    void setRobustResourceInit(bool enabled);

    void startGpuTimer();
    void stopGpuTimer();

    void beginInternalTraceEvent(const char *name);
    void endInternalTraceEvent(const char *name);
    void beginGLTraceEvent(const char *name, double hostTimeSec);
    void endGLTraceEvent(const char *name, double hostTimeSec);

    void disableTestHarnessSwap() { mSwapEnabled = false; }

    bool mIsTimestampQueryAvailable;

  private:
    void SetUp() override;
    void TearDown() override;

    void step() override;
    void startTest() override;
    void finishTest() override;
    void computeGPUTime() override;

    bool areExtensionPrerequisitesFulfilled() const;

    GLWindowBase *mGLWindow;
    OSWindow *mOSWindow;
    std::vector<const char *> mExtensionPrerequisites;
    angle::PlatformMethods mPlatformMethods;
    ConfigParameters mConfigParams;
    bool mSwapEnabled;

    struct TimestampSample
    {
        GLuint beginQuery;
        GLuint endQuery;
    };

    GLuint mCurrentTimestampBeginQuery = 0;
    std::vector<TimestampSample> mTimestampQueries;

    // Trace event record that can be output.
    std::vector<TraceEvent> mTraceEventBuffer;

    // Handle to the entry point binding library.
    std::unique_ptr<angle::Library> mEntryPointsLib;

    std::vector<std::thread::id> mThreadIDs;
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
