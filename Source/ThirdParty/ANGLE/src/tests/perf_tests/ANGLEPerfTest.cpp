//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ANGLEPerfTests:
//   Base class for google test performance tests
//

#include "ANGLEPerfTest.h"

#include "third_party/perf/perf_test.h"

#include <cassert>
#include <cmath>
#include <iostream>

ANGLEPerfTest::ANGLEPerfTest(const std::string &name, const std::string &suffix)
    : mName(name),
      mSuffix(suffix),
      mTimer(nullptr),
      mRunTimeSeconds(5.0),
      mNumStepsPerformed(0),
      mRunning(true)
{
    mTimer = CreateTimer();
}

ANGLEPerfTest::~ANGLEPerfTest()
{
    SafeDelete(mTimer);
}

void ANGLEPerfTest::run()
{
    mTimer->start();
    while (mRunning)
    {
        step();
        if (mRunning)
        {
            ++mNumStepsPerformed;
        }
        if (mTimer->getElapsedTime() > mRunTimeSeconds)
        {
            mRunning = false;
        }
    }
    finishTest();
    mTimer->stop();
}

void ANGLEPerfTest::printResult(const std::string &trace, double value, const std::string &units, bool important) const
{
    perf_test::PrintResult(mName, mSuffix, trace, value, units, important);
}

void ANGLEPerfTest::printResult(const std::string &trace, size_t value, const std::string &units, bool important) const
{
    perf_test::PrintResult(mName, mSuffix, trace, value, units, important);
}

void ANGLEPerfTest::SetUp()
{
}

void ANGLEPerfTest::TearDown()
{
    double relativeScore = static_cast<double>(mNumStepsPerformed) / mTimer->getElapsedTime();
    printResult("score", static_cast<size_t>(std::round(relativeScore)), "score", true);
}

double ANGLEPerfTest::normalizedTime(size_t value) const
{
    return static_cast<double>(value) / static_cast<double>(mNumStepsPerformed);
}

std::string RenderTestParams::suffix() const
{
    switch (getRenderer())
    {
        case EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE:
            return "_d3d11";
        case EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE:
            return "_d3d9";
        case EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE:
            return "_gl";
        case EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE:
            return "_gles";
        case EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE:
            return "_default";
        default:
            assert(0);
            return "_unk";
    }
}

ANGLERenderTest::ANGLERenderTest(const std::string &name, const RenderTestParams &testParams)
    : ANGLEPerfTest(name, testParams.suffix()),
      mTestParams(testParams),
      mEGLWindow(nullptr),
      mOSWindow(nullptr)
{
}

ANGLERenderTest::~ANGLERenderTest()
{
    SafeDelete(mOSWindow);
    SafeDelete(mEGLWindow);
}

void ANGLERenderTest::SetUp()
{
    mOSWindow = CreateOSWindow();
    mEGLWindow = new EGLWindow(mTestParams.majorVersion, mTestParams.minorVersion,
                               mTestParams.eglParameters);
    mEGLWindow->setSwapInterval(0);

    if (!mOSWindow->initialize(mName, mTestParams.windowWidth, mTestParams.windowHeight))
    {
        FAIL() << "Failed initializing OSWindow";
        return;
    }

    if (!mEGLWindow->initializeGL(mOSWindow))
    {
        FAIL() << "Failed initializing EGLWindow";
        return;
    }

    initializeBenchmark();

    ANGLEPerfTest::SetUp();
}

void ANGLERenderTest::TearDown()
{
    ANGLEPerfTest::TearDown();

    destroyBenchmark();

    mEGLWindow->destroyGL();
    mOSWindow->destroy();
}

void ANGLERenderTest::step()
{
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
        // Swap is needed so that the GPU driver will occasionally flush its internal command queue
        // to the GPU. The null device benchmarks are only testing CPU overhead, so they don't need
        // to swap.
        if (mTestParams.eglParameters.deviceType != EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE)
        {
            mEGLWindow->swap();
        }
        mOSWindow->messageLoop();
    }
}

void ANGLERenderTest::finishTest()
{
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
