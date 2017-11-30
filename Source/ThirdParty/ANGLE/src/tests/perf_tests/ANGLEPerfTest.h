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

#include "EGLWindow.h"
#include "OSWindow.h"
#include "Timer.h"
#include "common/angleutils.h"
#include "common/debug.h"
#include "platform/Platform.h"
#include "test_utils/angle_test_configs.h"
#include "test_utils/angle_test_instantiate.h"

class Event;

#if !defined(ASSERT_GL_NO_ERROR)
#define ASSERT_GL_NO_ERROR() ASSERT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError())
#endif  // !defined(ASSERT_GL_NO_ERROR)

#if !defined(ASSERT_GLENUM_EQ)
#define ASSERT_GLENUM_EQ(expected, actual) \
    ASSERT_EQ(static_cast<GLenum>(expected), static_cast<GLenum>(actual))
#endif  // !defined(ASSERT_GLENUM_EQ)

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
    bool mSkipTest;

  private:
    unsigned int mNumStepsPerformed;
    bool mRunning;
};

struct RenderTestParams : public angle::PlatformParameters
{
    virtual std::string suffix() const;

    EGLint windowWidth  = 64;
    EGLint windowHeight = 64;
};

class ANGLERenderTest : public ANGLEPerfTest
{
  public:
    ANGLERenderTest(const std::string &name, const RenderTestParams &testParams);
    ANGLERenderTest(const std::string &name,
                    const RenderTestParams &testParams,
                    const std::vector<std::string> &extensionPrerequisites);
    ~ANGLERenderTest();

    virtual void initializeBenchmark() { }
    virtual void destroyBenchmark() { }

    virtual void drawBenchmark() = 0;

    bool popEvent(Event *event);

    OSWindow *getWindow();

    virtual void overrideWorkaroundsD3D(angle::WorkaroundsD3D *workaroundsD3D) {}

  protected:
    const RenderTestParams &mTestParams;

    void setWebGLCompatibilityEnabled(bool webglCompatibility);
    void setRobustResourceInit(bool enabled);

  private:
    void SetUp() override;
    void TearDown() override;

    void step() override;
    void finishTest() override;

    bool areExtensionPrerequisitesFulfilled() const;

    static EGLWindow *createEGLWindow(const RenderTestParams &testParams);

    EGLWindow *mEGLWindow;
    OSWindow *mOSWindow;
    std::vector<std::string> mExtensionPrerequisites;
    angle::PlatformMethods mPlatformMethods;
};

extern bool g_OnlyOneRunFrame;

#endif // PERF_TESTS_ANGLE_PERF_TEST_H_
