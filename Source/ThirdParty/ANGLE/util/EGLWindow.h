//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef UTIL_EGLWINDOW_H_
#define UTIL_EGLWINDOW_H_

#define GL_GLEXT_PROTOTYPES

#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <string>
#include <list>
#include <cstdint>
#include <memory>

#include "common/angleutils.h"

class OSWindow;

// A hidden define used in some renderers (currently D3D-only)
// to init a no-op renderer. Useful for performance testing.
#ifndef EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE 0x6AC0
#endif

struct EGLPlatformParameters
{
    EGLint renderer;
    EGLint majorVersion;
    EGLint minorVersion;
    EGLint deviceType;

    EGLPlatformParameters();
    explicit EGLPlatformParameters(EGLint renderer);
    EGLPlatformParameters(EGLint renderer, EGLint majorVersion, EGLint minorVersion, EGLint deviceType);
};

class EGLWindow : angle::NonCopyable
{
  public:
    EGLWindow(size_t width, size_t height, EGLint glesMajorVersion, const EGLPlatformParameters &platform);

    ~EGLWindow();

    void setClientVersion(EGLint glesMajorVersion) { mClientVersion = glesMajorVersion; }
    void setWidth(size_t width) { mWidth = width; }
    void setHeight(size_t height) { mHeight = height; }
    void setConfigRedBits(int bits) { mRedBits = bits; }
    void setConfigGreenBits(int bits) { mGreenBits = bits; }
    void setConfigBlueBits(int bits) { mBlueBits = bits; }
    void setConfigAlphaBits(int bits) { mAlphaBits = bits; }
    void setConfigDepthBits(int bits) { mDepthBits = bits; }
    void setConfigStencilBits(int bits) { mStencilBits = bits; }
    void setMultisample(bool multisample) { mMultisample = multisample; }
    void setSwapInterval(EGLint swapInterval) { mSwapInterval = swapInterval; }

    void swap();

    EGLint getClientVersion() const { return mClientVersion; }
    const EGLPlatformParameters &getPlatform() const { return mPlatform; }
    EGLConfig getConfig() const;
    EGLDisplay getDisplay() const;
    EGLSurface getSurface() const;
    EGLContext getContext() const;
    size_t getWidth() const { return mWidth; }
    size_t getHeight() const { return mHeight; }
    int getConfigRedBits() const { return mRedBits; }
    int getConfigGreenBits() const { return mGreenBits; }
    int getConfigBlueBits() const { return mBlueBits; }
    int getConfigAlphaBits() const { return mAlphaBits; }
    int getConfigDepthBits() const { return mDepthBits; }
    int getConfigStencilBits() const { return mStencilBits; }
    bool isMultisample() const { return mMultisample; }
    EGLint getSwapInterval() const { return mSwapInterval; }

    bool initializeGL(OSWindow *osWindow);
    void destroyGL();
    bool isGLInitialized() const;

  private:
    EGLConfig mConfig;
    EGLDisplay mDisplay;
    EGLSurface mSurface;
    EGLContext mContext;

    EGLint mClientVersion;
    EGLPlatformParameters mPlatform;
    size_t mWidth;
    size_t mHeight;
    int mRedBits;
    int mGreenBits;
    int mBlueBits;
    int mAlphaBits;
    int mDepthBits;
    int mStencilBits;
    bool mMultisample;
    EGLint mSwapInterval;
};

#endif // UTIL_EGLWINDOW_H_
