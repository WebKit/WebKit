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
#include <unordered_map>
#include <vector>

#include "platform/Platform.h"
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

    TraceEvent(char phaseIn, const char *categoryNameIn, const char *nameIn, double timestampIn)
        : phase(phaseIn), categoryName(categoryNameIn), name(nameIn), timestamp(timestampIn)
    {}

    char phase               = 0;
    const char *categoryName = nullptr;
    const char *name         = nullptr;
    double timestamp         = 0;
};

class ANGLEPerfTest : public testing::Test, angle::NonCopyable
{
  public:
    ANGLEPerfTest(const std::string &name,
                  const std::string &backend,
                  const std::string &story,
                  unsigned int iterationsPerStep);
    virtual ~ANGLEPerfTest();

    virtual void step() = 0;

    // Called right after the timer starts to let the test initialize other metrics if necessary
    virtual void startTest() {}
    // Called right before timer is stopped to let the test wait for asynchronous operations.
    virtual void finishTest() {}

  protected:
    void run();
    void SetUp() override;
    void TearDown() override;

    // Normalize a time value according to the number of test loop iterations (mFrameCount)
    double normalizedTime(size_t value) const;

    // Call if the test step was aborted and the test should stop running.
    void abortTest() { mRunning = false; }

    unsigned int getNumStepsPerformed() const { return mNumStepsPerformed; }
    void doRunLoop(double maxRunTime);

    std::string mName;
    std::string mBackend;
    std::string mStory;
    Timer mTimer;
    uint64_t mGPUTimeNs;
    bool mSkipTest;
    std::unique_ptr<perf_test::PerfResultReporter> mReporter;

  private:
    double printResults();

    unsigned int mStepsToRun;
    unsigned int mNumStepsPerformed;
    unsigned int mIterationsPerStep;
    bool mRunning;
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
};

class ANGLERenderTest : public ANGLEPerfTest
{
  public:
    ANGLERenderTest(const std::string &name, const RenderTestParams &testParams);
    ~ANGLERenderTest();

    void addExtensionPrerequisite(const char *extensionName);

    virtual void initializeBenchmark() {}
    virtual void destroyBenchmark() {}

    virtual void drawBenchmark() = 0;

    bool popEvent(Event *event);

    OSWindow *getWindow();
    GLWindowBase *getGLWindow();

    std::vector<TraceEvent> &getTraceEventBuffer();

    virtual void overrideWorkaroundsD3D(angle::FeaturesD3D *featuresD3D) {}

  protected:
    const RenderTestParams &mTestParams;

    void setWebGLCompatibilityEnabled(bool webglCompatibility);
    void setRobustResourceInit(bool enabled);

    void startGpuTimer();
    void stopGpuTimer();

    void beginInternalTraceEvent(const char *name);
    void endInternalTraceEvent(const char *name);

  private:
    void SetUp() override;
    void TearDown() override;

    void step() override;
    void startTest() override;
    void finishTest() override;

    bool areExtensionPrerequisitesFulfilled() const;

    GLWindowBase *mGLWindow;
    OSWindow *mOSWindow;
    std::vector<const char *> mExtensionPrerequisites;
    angle::PlatformMethods mPlatformMethods;
    ConfigParameters mConfigParams;

    bool mIsTimestampQueryAvailable;
    GLuint mTimestampQuery;

    // Trace event record that can be output.
    std::vector<TraceEvent> mTraceEventBuffer;

    // Handle to the entry point binding library.
    std::unique_ptr<angle::Library> mEntryPointsLib;
};

// Mixins.
namespace params
{
template <typename ParamsT>
ParamsT Offscreen(const ParamsT &input)
{
    ParamsT output   = input;
    output.offscreen = true;
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
}  // namespace params

#endif  // PERF_TESTS_ANGLE_PERF_TEST_H_
