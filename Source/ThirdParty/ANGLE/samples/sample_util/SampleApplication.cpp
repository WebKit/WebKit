//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "SampleApplication.h"

#include "common/debug.h"
#include "util/EGLWindow.h"
#include "util/gles_loader_autogen.h"
#include "util/random_utils.h"
#include "util/shader_utils.h"
#include "util/test_utils.h"
#include "util/util_gl.h"

#include <string.h>
#include <iostream>
#include <utility>

#if defined(ANGLE_PLATFORM_WINDOWS)
#    include "util/windows/WGLWindow.h"
#endif  // defined(ANGLE_PLATFORM_WINDOWS)

namespace
{
const char *kUseAngleArg = "--use-angle=";
const char *kUseGlArg    = "--use-gl=native";

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

bool IsGLExtensionEnabled(const std::string &extName)
{
    return angle::CheckExtensionExists(reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)),
                                       extName);
}

SampleApplication::SampleApplication(std::string name,
                                     int argc,
                                     char **argv,
                                     ClientType clientType,
                                     uint32_t width,
                                     uint32_t height)
    : mName(std::move(name)),
      mWidth(width),
      mHeight(height),
      mRunning(false),
      mFrameCount(0),
      mGLWindow(nullptr),
      mEGLWindow(nullptr),
      mOSWindow(nullptr),
      mDriverType(angle::GLESDriverType::AngleEGL)
{
    mPlatformParams.renderer = EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE;
    bool useNativeGL         = false;

    for (int argIndex = 1; argIndex < argc; argIndex++)
    {
        if (strncmp(argv[argIndex], kUseAngleArg, strlen(kUseAngleArg)) == 0)
        {
            const char *arg            = argv[argIndex] + strlen(kUseAngleArg);
            mPlatformParams.renderer   = GetDisplayTypeFromArg(arg);
            mPlatformParams.deviceType = GetDeviceTypeFromArg(arg);
        }

        if (strncmp(argv[argIndex], kUseGlArg, strlen(kUseGlArg)) == 0)
        {
            useNativeGL = true;
        }
    }

    EGLenum eglClientType = EGL_OPENGL_ES_API;
    EGLint glMajorVersion = 2;
    EGLint glMinorVersion = 0;
    EGLint profileMask    = 0;

    switch (clientType)
    {
        case ClientType::ES1:
            glMajorVersion = 1;
            break;
        case ClientType::ES2:
            break;
        case ClientType::ES3_0:
            glMajorVersion = 3;
            break;
        case ClientType::ES3_1:
            glMajorVersion = 3;
            glMinorVersion = 1;
            break;
        case ClientType::GL3_3_CORE:
            eglClientType  = EGL_OPENGL_API;
            glMajorVersion = 3;
            glMinorVersion = 3;
            profileMask    = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
            break;
        case ClientType::GL3_3_COMPATIBILITY:
            eglClientType  = EGL_OPENGL_API;
            glMajorVersion = 3;
            glMinorVersion = 3;
            profileMask    = EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT;
            break;
        case ClientType::GL4_6_CORE:
            eglClientType  = EGL_OPENGL_API;
            glMajorVersion = 4;
            glMinorVersion = 6;
            profileMask    = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
            break;
        case ClientType::GL4_6_COMPATIBILITY:
            eglClientType  = EGL_OPENGL_API;
            glMajorVersion = 4;
            glMinorVersion = 6;
            profileMask    = EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT;
            break;
        default:
            UNREACHABLE();
    }

    mOSWindow = OSWindow::New();

    // Load EGL library so we can initialize the display.
    if (useNativeGL)
    {
#if defined(ANGLE_PLATFORM_WINDOWS)
        mGLWindow = WGLWindow::New(eglClientType, glMajorVersion, glMinorVersion, profileMask);
        mEntryPointsLib.reset(angle::OpenSharedLibrary("opengl32", angle::SearchType::SystemDir));
        mDriverType = angle::GLESDriverType::SystemWGL;
#else
        mGLWindow = EGLWindow::New(eglClientType, glMajorVersion, glMinorVersion, profileMask);
        mEntryPointsLib.reset(angle::OpenSharedLibraryWithExtension(
            angle::GetNativeEGLLibraryNameWithExtension(), angle::SearchType::SystemDir));
        mDriverType = angle::GLESDriverType::SystemEGL;
#endif  // defined(ANGLE_PLATFORM_WINDOWS)
    }
    else
    {
#if defined(ANGLE_EXPOSE_WGL_ENTRY_POINTS)
        mGLWindow = WGLWindow::New(eglClientType, glMajorVersion, glMinorVersion, profileMask);
        mEntryPointsLib.reset(angle::OpenSharedLibrary("opengl32", angle::SearchType::ModuleDir));
        mDriverType = angle::GLESDriverType::SystemWGL;
#else
        mGLWindow   = mEGLWindow =
            EGLWindow::New(eglClientType, glMajorVersion, glMinorVersion, profileMask);
        mEntryPointsLib.reset(
            angle::OpenSharedLibrary(ANGLE_EGL_LIBRARY_NAME, angle::SearchType::ModuleDir));
#endif  // defined(ANGLE_EXPOSE_WGL_ENTRY_POINTS)
    }
}

SampleApplication::~SampleApplication()
{
    GLWindowBase::Delete(&mGLWindow);
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
    mGLWindow->swap();
}

OSWindow *SampleApplication::getWindow() const
{
    return mOSWindow;
}

EGLConfig SampleApplication::getConfig() const
{
    ASSERT(mEGLWindow);
    return mEGLWindow->getConfig();
}

EGLDisplay SampleApplication::getDisplay() const
{
    ASSERT(mEGLWindow);
    return mEGLWindow->getDisplay();
}

EGLSurface SampleApplication::getSurface() const
{
    ASSERT(mEGLWindow);
    return mEGLWindow->getSurface();
}

EGLContext SampleApplication::getContext() const
{
    ASSERT(mEGLWindow);
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

    if (!mGLWindow->initializeGL(mOSWindow, mEntryPointsLib.get(), mDriverType, mPlatformParams,
                                 configParams))
    {
        return -1;
    }

    // Disable vsync
    if (!mGLWindow->setSwapInterval(0))
    {
        return -1;
    }

    mRunning   = true;
    int result = 0;

#if defined(ANGLE_ENABLE_ASSERTS)
    if (IsGLExtensionEnabled("GL_KHR_debug"))
    {
        EnableDebugCallback(nullptr, nullptr);
    }
#endif

    if (!initialize())
    {
        mRunning = false;
        result   = -1;
    }

    mTimer.start();
    double prevTime = 0.0;

    while (mRunning)
    {
        double elapsedTime = mTimer.getElapsedWallClockTime();
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

        mFrameCount++;

        if (mFrameCount % 100 == 0)
        {
            printf("Rate: %0.2lf frames / second\n",
                   static_cast<double>(mFrameCount) / mTimer.getElapsedWallClockTime());
        }
    }

    destroy();
    mGLWindow->destroyGL();
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
