//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
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
#include <vector>

#include "platform/Platform.h"
#include "test_utils/angle_test_configs.h"
#include "test_utils/angle_test_instantiate.h"
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
                  const std::string &suffix,
                  unsigned int iterationsPerStep);
    virtual ~ANGLEPerfTest();

    virtual void step() = 0;

    // Called right after the timer starts to let the test initialize other metrics if necessary
    virtual void startTest() {}
    // Called right before timer is stopped to let the test wait for asynchronous operations.
    virtual void finishTest() {}

    Timer *getTimer() const { return mTimer; }

  protected:
    void run();
    void printResult(const std::string &trace,
                     double value,
                     const std::string &units,
                     bool important) const;
    void printResult(const std::string &trace,
                     size_t value,
                     const std::string &units,
                     bool important) const;
    void SetUp() override;
    void TearDown() override;

    // Normalize a time value according to the number of test loop iterations (mFrameCount)
    double normalizedTime(size_t value) const;

    // Call if the test step was aborted and the test should stop running.
    void abortTest() { mRunning = false; }

    unsigned int getNumStepsPerformed() const { return mNumStepsPerformed; }
    void doRunLoop(double maxRunTime);

    std::string mName;
    std::string mSuffix;
    Timer *mTimer;
    uint64_t mGPUTimeNs;
    bool mSkipTest;

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

    virtual std::string suffix() const;

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

    std::vector<TraceEvent> &getTraceEventBuffer();

    virtual void overrideWorkaroundsD3D(angle::WorkaroundsD3D *workaroundsD3D) {}

  protected:
    const RenderTestParams &mTestParams;

    void setWebGLCompatibilityEnabled(bool webglCompatibility);
    void setRobustResourceInit(bool enabled);

    void startGpuTimer();
    void stopGpuTimer();

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

    GLuint mTimestampQuery;

    // Trace event record that can be output.
    std::vector<TraceEvent> mTraceEventBuffer;

    // Handle to the entry point binding library.
    std::unique_ptr<angle::Library> mEntryPointsLib;
};

#endif  // PERF_TESTS_ANGLE_PERF_TEST_H_
