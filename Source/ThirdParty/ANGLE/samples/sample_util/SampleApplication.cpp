//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "SampleApplication.h"

#include "util/EGLWindow.h"
#include "util/gles_loader_autogen.h"
#include "util/random_utils.h"
#include "util/test_utils.h"

#include <string.h>
#include <iostream>
#include <utility>

namespace
{
const char *kUseAngleArg = "--use-angle=";

using DisplayTypeInfo = std::pair<const char *, EGLint>;

const DisplayTypeInfo kDisplayTypes[] = {
    {"d3d9", EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE},
    {"d3d11", EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE},
    {"gl", EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE},
    {"gles", EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE},
    {"metal", EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE},
    {"null", EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE},
    {"swiftshader", EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE},
    {"vulkan", EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE},
};

EGLint GetDisplayTypeFromArg(const char *displayTypeArg)
{
    for (const auto &displayTypeInfo : kDisplayTypes)
    {
        if (strcmp(displayTypeInfo.first, displayTypeArg) == 0)
        {
            std::cout << "Using ANGLE back-end API: " << displayTypeInfo.first << std::endl;
            return displayTypeInfo.second;
        }
    }

    std::cout << "Unknown ANGLE back-end API: " << displayTypeArg << std::endl;
    return EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE;
}

EGLint GetDeviceTypeFromArg(const char *displayTypeArg)
{
    if (strcmp(displayTypeArg, "swiftshader") == 0)
    {
        return EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE;
    }
    else
    {
        return EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE;
    }
}
}  // anonymous namespace

SampleApplication::SampleApplication(std::string name,
                                     int argc,
                                     char **argv,
                                     EGLint glesMajorVersion,
                                     EGLint glesMinorVersion,
                                     uint32_t width,
                                     uint32_t height)
    : mName(std::move(name)),
      mWidth(width),
      mHeight(height),
      mRunning(false),
      mEGLWindow(nullptr),
      mOSWindow(nullptr)
{
    mPlatformParams.renderer = EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE;

    if (argc > 1 && strncmp(argv[1], kUseAngleArg, strlen(kUseAngleArg)) == 0)
    {
        const char *arg            = argv[1] + strlen(kUseAngleArg);
        mPlatformParams.renderer   = GetDisplayTypeFromArg(arg);
        mPlatformParams.deviceType = GetDeviceTypeFromArg(arg);
    }

    // Load EGL library so we can initialize the display.
    mEntryPointsLib.reset(
        angle::OpenSharedLibrary(ANGLE_EGL_LIBRARY_NAME, angle::SearchType::ApplicationDir));

    mEGLWindow = EGLWindow::New(glesMajorVersion, glesMinorVersion);
    mOSWindow  = OSWindow::New();
}

SampleApplication::~SampleApplication()
{
    EGLWindow::Delete(&mEGLWindow);
    OSWindow::Delete(&mOSWindow);
}

bool SampleApplication::initialize()
{
    return true;
}

void SampleApplication::destroy() {}

void SampleApplication::step(float dt, double totalTime) {}

void SampleApplication::draw() {}

void SampleApplication::swap()
{
    mEGLWindow->swap();
}

OSWindow *SampleApplication::getWindow() const
{
    return mOSWindow;
}

EGLConfig SampleApplication::getConfig() const
{
    return mEGLWindow->getConfig();
}

EGLDisplay SampleApplication::getDisplay() const
{
    return mEGLWindow->getDisplay();
}

EGLSurface SampleApplication::getSurface() const
{
    return mEGLWindow->getSurface();
}

EGLContext SampleApplication::getContext() const
{
    return mEGLWindow->getContext();
}

int SampleApplication::run()
{
    if (!mOSWindow->initialize(mName, mWidth, mHeight))
    {
        return -1;
    }

    mOSWindow->setVisible(true);

    ConfigParameters configParams;
    configParams.redBits     = 8;
    configParams.greenBits   = 8;
    configParams.blueBits    = 8;
    configParams.alphaBits   = 8;
    configParams.depthBits   = 24;
    configParams.stencilBits = 8;

    if (!mEGLWindow->initializeGL(mOSWindow, mEntryPointsLib.get(), angle::GLESDriverType::AngleEGL,
                                  mPlatformParams, configParams))
    {
        return -1;
    }

    // Disable vsync
    if (!mEGLWindow->setSwapInterval(0))
    {
        return -1;
    }

    angle::LoadGLES(eglGetProcAddress);

    mRunning   = true;
    int result = 0;

    if (!initialize())
    {
        mRunning = false;
        result   = -1;
    }

    mTimer.start();
    double prevTime = 0.0;

    while (mRunning)
    {
        double elapsedTime = mTimer.getElapsedTime();
        double deltaTime   = elapsedTime - prevTime;

        step(static_cast<float>(deltaTime), elapsedTime);

        // Clear events that the application did not process from this frame
        Event event;
        while (popEvent(&event))
        {
            // If the application did not catch a close event, close now
            switch (event.Type)
            {
                case Event::EVENT_CLOSED:
                    exit();
                    break;
                case Event::EVENT_KEY_RELEASED:
                    onKeyUp(event.Key);
                    break;
                case Event::EVENT_KEY_PRESSED:
                    onKeyDown(event.Key);
                    break;
                default:
                    break;
            }
        }

        if (!mRunning)
        {
            break;
        }

        draw();
        swap();

        mOSWindow->messageLoop();

        prevTime = elapsedTime;
    }

    destroy();
    mEGLWindow->destroyGL();
    mOSWindow->destroy();

    return result;
}

void SampleApplication::exit()
{
    mRunning = false;
}

bool SampleApplication::popEvent(Event *event)
{
    return mOSWindow->popEvent(event);
}

void SampleApplication::onKeyUp(const Event::KeyEvent &keyEvent)
{
    // Default no-op.
}

void SampleApplication::onKeyDown(const Event::KeyEvent &keyEvent)
{
    // Default no-op.
}
