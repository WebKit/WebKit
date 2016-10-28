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

#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "common/angleutils.h"
#include "common/debug.h"
#include "EGLWindow.h"
#include "OSWindow.h"
#include "test_utils/angle_test_configs.h"
#include "test_utils/angle_test_instantiate.h"
#include "Timer.h"

class Event;

#ifndef ASSERT_GL_NO_ERROR
#define ASSERT_GL_NO_ERROR() ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError())
#endif

class ANGLEPerfTest : public testing::Test, angle::NonCopyable
{
  public:
    ANGLEPerfTest(const std::string &name, const std::string &suffix);
    virtual ~ANGLEPerfTest();

    virtual void step() = 0;

    // Called right before timer is stopped to let the test wait for asynchronous operations.
    virtual void finishTest() {}

  protected:
    void run();
    void printResult(const std::string &trace, double value, const std::string &units, bool important) const;
    void printResult(const std::string &trace, size_t value, const std::string &units, bool important) const;
    void SetUp() override;
    void TearDown() override;

    // Normalize a time value according to the number of test loop iterations (mFrameCount)
    double normalizedTime(size_t value) const;

    // Call if the test step was aborted and the test should stop running.
    void abortTest() { mRunning = false; }

    unsigned int getNumStepsPerformed() const { return mNumStepsPerformed; }

    std::string mName;
    std::string mSuffix;
    Timer *mTimer;
    double mRunTimeSeconds;

  private:
    unsigned int mNumStepsPerformed;
    bool mRunning;
};

struct RenderTestParams : public angle::PlatformParameters
{
    virtual std::string suffix() const;

    EGLint windowWidth;
    EGLint windowHeight;
};

class ANGLERenderTest : public ANGLEPerfTest
{
  public:
    ANGLERenderTest(const std::string &name, const RenderTestParams &testParams);
    ~ANGLERenderTest();

    virtual void initializeBenchmark() { }
    virtual void destroyBenchmark() { }

    virtual void drawBenchmark() = 0;

    bool popEvent(Event *event);

    OSWindow *getWindow();

  protected:
    const RenderTestParams &mTestParams;

  private:
    void SetUp() override;
    void TearDown() override;

    void step() override;
    void finishTest() override;

    EGLWindow *mEGLWindow;
    OSWindow *mOSWindow;
};

#endif // PERF_TESTS_ANGLE_PERF_TEST_H_
