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

namespace
{
void EmptyPlatformMethod(angle::PlatformMethods *, const char *)
{
}

void OverrideWorkaroundsD3D(angle::PlatformMethods *platform, angle::WorkaroundsD3D *workaroundsD3D)
{
    auto *angleRenderTest = static_cast<ANGLERenderTest *>(platform->context);
    angleRenderTest->overrideWorkaroundsD3D(workaroundsD3D);
}
}  // namespace

bool g_OnlyOneRunFrame = false;

ANGLEPerfTest::ANGLEPerfTest(const std::string &name, const std::string &suffix)
    : mName(name),
      mSuffix(suffix),
      mTimer(CreateTimer()),
      mRunTimeSeconds(5.0),
      mSkipTest(false),
      mNumStepsPerformed(0),
      mRunning(true)
{
}

ANGLEPerfTest::~ANGLEPerfTest()
{
    SafeDelete(mTimer);
}

void ANGLEPerfTest::run()
{
    if (mSkipTest)
    {
        return;
    }

    mTimer->start();
    while (mRunning)
    {
        step();
        if (mRunning)
        {
            ++mNumStepsPerformed;
        }
        if (mTimer->getElapsedTime() > mRunTimeSeconds || g_OnlyOneRunFrame)
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
    if (mSkipTest)
    {
        return;
    }
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
        case EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE:
            return "_vulkan";
        default:
            assert(0);
            return "_unk";
    }
}

ANGLERenderTest::ANGLERenderTest(const std::string &name, const RenderTestParams &testParams)
    : ANGLEPerfTest(name, testParams.suffix()),
      mTestParams(testParams),
      mEGLWindow(createEGLWindow(testParams)),
      mOSWindow(nullptr)
{
}

ANGLERenderTest::ANGLERenderTest(const std::string &name,
                                 const RenderTestParams &testParams,
                                 const std::vector<std::string> &extensionPrerequisites)
    : ANGLEPerfTest(name, testParams.suffix()),
      mTestParams(testParams),
      mEGLWindow(createEGLWindow(testParams)),
      mOSWindow(nullptr),
      mExtensionPrerequisites(extensionPrerequisites)
{
}

ANGLERenderTest::~ANGLERenderTest()
{
    SafeDelete(mOSWindow);
    SafeDelete(mEGLWindow);
}

void ANGLERenderTest::SetUp()
{
    ANGLEPerfTest::SetUp();

    mOSWindow = CreateOSWindow();
    ASSERT(mEGLWindow != nullptr);
    mEGLWindow->setSwapInterval(0);

    mPlatformMethods.overrideWorkaroundsD3D = OverrideWorkaroundsD3D;
    mPlatformMethods.logError               = EmptyPlatformMethod;
    mPlatformMethods.logWarning             = EmptyPlatformMethod;
    mPlatformMethods.logInfo                = EmptyPlatformMethod;
    mPlatformMethods.context                = this;
    mEGLWindow->setPlatformMethods(&mPlatformMethods);

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

    if (!areExtensionPrerequisitesFulfilled())
    {
        mSkipTest = true;
    }

    if (mSkipTest)
    {
        std::cout << "Test skipped due to missing extension." << std::endl;
        return;
    }

    initializeBenchmark();
}

void ANGLERenderTest::TearDown()
{
    destroyBenchmark();

    mEGLWindow->destroyGL();
    mOSWindow->destroy();

    ANGLEPerfTest::TearDown();
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

bool ANGLERenderTest::areExtensionPrerequisitesFulfilled() const
{
    for (const auto &extension : mExtensionPrerequisites)
    {
        if (!CheckExtensionExists(reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)),
                                  extension))
        {
            return false;
        }
    }
    return true;
}

void ANGLERenderTest::setWebGLCompatibilityEnabled(bool webglCompatibility)
{
    mEGLWindow->setWebGLCompatibilityEnabled(webglCompatibility);
}

void ANGLERenderTest::setRobustResourceInit(bool enabled)
{
    mEGLWindow->setRobustResourceInit(enabled);
}

// static
EGLWindow *ANGLERenderTest::createEGLWindow(const RenderTestParams &testParams)
{
    return new EGLWindow(testParams.majorVersion, testParams.minorVersion,
                         testParams.eglParameters);
}
