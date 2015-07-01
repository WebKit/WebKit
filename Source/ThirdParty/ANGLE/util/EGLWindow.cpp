//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <string.h>
#include <cassert>
#include <vector>

#include "EGLWindow.h"
#include "OSWindow.h"

#ifdef _WIN32
#include "win32/Win32Timer.h"
#include "win32/Win32Window.h"
#else
#error unsupported OS.
#endif

EGLPlatformParameters::EGLPlatformParameters()
    : renderer(EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE),
      majorVersion(EGL_DONT_CARE),
      minorVersion(EGL_DONT_CARE),
      deviceType(EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE)
{
}

EGLPlatformParameters::EGLPlatformParameters(EGLint renderer)
    : renderer(renderer),
      majorVersion(EGL_DONT_CARE),
      minorVersion(EGL_DONT_CARE),
      deviceType(EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE)
{
}

EGLPlatformParameters::EGLPlatformParameters(EGLint renderer, EGLint majorVersion, EGLint minorVersion, EGLint useWarp)
    : renderer(renderer),
      majorVersion(majorVersion),
      minorVersion(minorVersion),
      deviceType(useWarp)
{
}


EGLWindow::EGLWindow(size_t width, size_t height, EGLint glesMajorVersion, const EGLPlatformParameters &platform)
    : mSurface(EGL_NO_SURFACE),
      mContext(EGL_NO_CONTEXT),
      mDisplay(EGL_NO_DISPLAY),
      mClientVersion(glesMajorVersion),
      mPlatform(platform),
      mWidth(width),
      mHeight(height),
      mRedBits(-1),
      mGreenBits(-1),
      mBlueBits(-1),
      mAlphaBits(-1),
      mDepthBits(-1),
      mStencilBits(-1),
      mMultisample(false),
      mSwapInterval(-1)
{
}

EGLWindow::~EGLWindow()
{
    destroyGL();
}

void EGLWindow::swap()
{
    eglSwapBuffers(mDisplay, mSurface);
}

EGLConfig EGLWindow::getConfig() const
{
    return mConfig;
}

EGLDisplay EGLWindow::getDisplay() const
{
    return mDisplay;
}

EGLSurface EGLWindow::getSurface() const
{
    return mSurface;
}

EGLContext EGLWindow::getContext() const
{
    return mContext;
}

bool EGLWindow::initializeGL(OSWindow *osWindow)
{
    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (!eglGetPlatformDisplayEXT)
    {
        return false;
    }

    const EGLint displayAttributes[] =
    {
        EGL_PLATFORM_ANGLE_TYPE_ANGLE,              mPlatform.renderer,
        EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, mPlatform.majorVersion,
        EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, mPlatform.minorVersion,
        EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE,       mPlatform.deviceType,
        EGL_NONE,
    };

    mDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, osWindow->getNativeDisplay(), displayAttributes);
    if (mDisplay == EGL_NO_DISPLAY)
    {
        destroyGL();
        return false;
    }

    EGLint majorVersion, minorVersion;
    if (!eglInitialize(mDisplay, &majorVersion, &minorVersion))
    {
        destroyGL();
        return false;
    }

    eglBindAPI(EGL_OPENGL_ES_API);
    if (eglGetError() != EGL_SUCCESS)
    {
        destroyGL();
        return false;
    }

    const EGLint configAttributes[] =
    {
        EGL_RED_SIZE,       (mRedBits >= 0)     ? mRedBits     : EGL_DONT_CARE,
        EGL_GREEN_SIZE,     (mGreenBits >= 0)   ? mGreenBits   : EGL_DONT_CARE,
        EGL_BLUE_SIZE,      (mBlueBits >= 0)    ? mBlueBits    : EGL_DONT_CARE,
        EGL_ALPHA_SIZE,     (mAlphaBits >= 0)   ? mAlphaBits   : EGL_DONT_CARE,
        EGL_DEPTH_SIZE,     (mDepthBits >= 0)   ? mDepthBits   : EGL_DONT_CARE,
        EGL_STENCIL_SIZE,   (mStencilBits >= 0) ? mStencilBits : EGL_DONT_CARE,
        EGL_SAMPLE_BUFFERS, mMultisample ? 1 : 0,
        EGL_NONE
    };

    EGLint configCount;
    if (!eglChooseConfig(mDisplay, configAttributes, &mConfig, 1, &configCount) || (configCount != 1))
    {
        destroyGL();
        return false;
    }

    eglGetConfigAttrib(mDisplay, mConfig, EGL_RED_SIZE, &mRedBits);
    eglGetConfigAttrib(mDisplay, mConfig, EGL_GREEN_SIZE, &mGreenBits);
    eglGetConfigAttrib(mDisplay, mConfig, EGL_BLUE_SIZE, &mBlueBits);
    eglGetConfigAttrib(mDisplay, mConfig, EGL_ALPHA_SIZE, &mBlueBits);
    eglGetConfigAttrib(mDisplay, mConfig, EGL_DEPTH_SIZE, &mDepthBits);
    eglGetConfigAttrib(mDisplay, mConfig, EGL_STENCIL_SIZE, &mStencilBits);

    std::vector<EGLint> surfaceAttributes;
    if (strstr(eglQueryString(mDisplay, EGL_EXTENSIONS), "EGL_NV_post_sub_buffer") != nullptr)
    {
        surfaceAttributes.push_back(EGL_POST_SUB_BUFFER_SUPPORTED_NV);
        surfaceAttributes.push_back(EGL_TRUE);
    }

    surfaceAttributes.push_back(EGL_NONE);
    surfaceAttributes.push_back(EGL_NONE);

    mSurface = eglCreateWindowSurface(mDisplay, mConfig, osWindow->getNativeWindow(), &surfaceAttributes[0]);
    if (mSurface == EGL_NO_SURFACE)
    {
        eglGetError(); // Clear error and try again
        mSurface = eglCreateWindowSurface(mDisplay, mConfig, NULL, NULL);
    }

    if (eglGetError() != EGL_SUCCESS)
    {
        destroyGL();
        return false;
    }

    EGLint contextAttibutes[] =
    {
        EGL_CONTEXT_CLIENT_VERSION, mClientVersion,
        EGL_NONE
    };

    mContext = eglCreateContext(mDisplay, mConfig, NULL, contextAttibutes);
    if (eglGetError() != EGL_SUCCESS)
    {
        destroyGL();
        return false;
    }

    eglMakeCurrent(mDisplay, mSurface, mSurface, mContext);
    if (eglGetError() != EGL_SUCCESS)
    {
        destroyGL();
        return false;
    }

    if (mSwapInterval != -1)
    {
        eglSwapInterval(mDisplay, mSwapInterval);
    }

    return true;
}

void EGLWindow::destroyGL()
{
    if (mSurface != EGL_NO_SURFACE)
    {
        assert(mDisplay != EGL_NO_DISPLAY);
        eglDestroySurface(mDisplay, mSurface);
        mSurface = EGL_NO_SURFACE;
    }

    if (mContext != EGL_NO_CONTEXT)
    {
        assert(mDisplay != EGL_NO_DISPLAY);
        eglDestroyContext(mDisplay, mContext);
        mContext = EGL_NO_CONTEXT;
    }

    if (mDisplay != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglTerminate(mDisplay);
        mDisplay = EGL_NO_DISPLAY;
    }
}

bool EGLWindow::isGLInitialized() const
{
    return mSurface != EGL_NO_SURFACE &&
           mContext != EGL_NO_CONTEXT &&
           mDisplay != EGL_NO_DISPLAY;
}
