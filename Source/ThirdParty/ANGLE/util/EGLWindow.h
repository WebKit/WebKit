//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef UTIL_EGLWINDOW_H_
#define UTIL_EGLWINDOW_H_

#include <list>
#include <memory>
#include <stdint.h>
#include <string>

#include <export.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "common/angleutils.h"
#include "common/Optional.h"

class OSWindow;

// A hidden define used in some renderers (currently D3D-only)
// to init a no-op renderer. Useful for performance testing.
#ifndef EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE 0x6AC0
#endif

namespace angle
{
struct PlatformMethods;
}

struct ANGLE_EXPORT EGLPlatformParameters
{
    EGLint renderer;
    EGLint majorVersion;
    EGLint minorVersion;
    EGLint deviceType;
    EGLint presentPath;

    EGLPlatformParameters();
    explicit EGLPlatformParameters(EGLint renderer);
    EGLPlatformParameters(EGLint renderer, EGLint majorVersion, EGLint minorVersion, EGLint deviceType);
    EGLPlatformParameters(EGLint renderer,
                          EGLint majorVersion,
                          EGLint minorVersion,
                          EGLint deviceType,
                          EGLint presentPath);
};

ANGLE_EXPORT bool operator<(const EGLPlatformParameters &a, const EGLPlatformParameters &b);
ANGLE_EXPORT bool operator==(const EGLPlatformParameters &a, const EGLPlatformParameters &b);

class ANGLE_EXPORT EGLWindow : angle::NonCopyable
{
  public:
    EGLWindow(EGLint glesMajorVersion,
              EGLint glesMinorVersion,
              const EGLPlatformParameters &platform);

    ~EGLWindow();

    void setConfigRedBits(int bits) { mRedBits = bits; }
    void setConfigGreenBits(int bits) { mGreenBits = bits; }
    void setConfigBlueBits(int bits) { mBlueBits = bits; }
    void setConfigAlphaBits(int bits) { mAlphaBits = bits; }
    void setConfigDepthBits(int bits) { mDepthBits = bits; }
    void setConfigStencilBits(int bits) { mStencilBits = bits; }
    void setConfigComponentType(EGLenum componentType) { mComponentType = componentType; }
    void setMultisample(bool multisample) { mMultisample = multisample; }
    void setSamples(EGLint samples) { mSamples = samples; }
    void setDebugEnabled(bool debug) { mDebug = debug; }
    void setNoErrorEnabled(bool noError) { mNoError = noError; }
    void setWebGLCompatibilityEnabled(bool webglCompatibility)
    {
        mWebGLCompatibility = webglCompatibility;
    }
    void setBindGeneratesResource(bool bindGeneratesResource)
    {
        mBindGeneratesResource = bindGeneratesResource;
    }
    void setDebugLayersEnabled(bool enabled) { mDebugLayersEnabled = enabled; }
    void setClientArraysEnabled(bool enabled) { mClientArraysEnabled = enabled; }
    void setRobustAccess(bool enabled) { mRobustAccess = enabled; }
    void setRobustResourceInit(bool enabled) { mRobustResourceInit = enabled; }
    void setSwapInterval(EGLint swapInterval) { mSwapInterval = swapInterval; }
    void setPlatformMethods(angle::PlatformMethods *platformMethods)
    {
        mPlatformMethods = platformMethods;
    }
    void setContextProgramCacheEnabled(bool enabled) { mContextProgramCacheEnabled = enabled; }

    static EGLBoolean FindEGLConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *config);

    void swap();

    EGLint getClientMajorVersion() const { return mClientMajorVersion; }
    EGLint getClientMinorVersion() const { return mClientMinorVersion; }
    const EGLPlatformParameters &getPlatform() const { return mPlatform; }
    EGLConfig getConfig() const;
    EGLDisplay getDisplay() const;
    EGLSurface getSurface() const;
    EGLContext getContext() const;
    int getConfigRedBits() const { return mRedBits; }
    int getConfigGreenBits() const { return mGreenBits; }
    int getConfigBlueBits() const { return mBlueBits; }
    int getConfigAlphaBits() const { return mAlphaBits; }
    int getConfigDepthBits() const { return mDepthBits; }
    int getConfigStencilBits() const { return mStencilBits; }
    bool isMultisample() const { return mMultisample; }
    bool isDebugEnabled() const { return mDebug; }
    EGLint getSwapInterval() const { return mSwapInterval; }
    const angle::PlatformMethods *getPlatformMethods() const { return mPlatformMethods; }

    // Internally initializes the Display, Surface and Context.
    bool initializeGL(OSWindow *osWindow);

    // Only initializes the Display and Surface.
    bool initializeDisplayAndSurface(OSWindow *osWindow);

    // Only initializes the Context.
    bool initializeContext();

    void destroyGL();
    bool isGLInitialized() const;

    void makeCurrent();

    static bool ClientExtensionEnabled(const std::string &extName);

  private:
    EGLConfig mConfig;
    EGLDisplay mDisplay;
    EGLSurface mSurface;
    EGLContext mContext;

    EGLint mClientMajorVersion;
    EGLint mClientMinorVersion;
    EGLint mEGLMajorVersion;
    EGLint mEGLMinorVersion;
    EGLPlatformParameters mPlatform;
    int mRedBits;
    int mGreenBits;
    int mBlueBits;
    int mAlphaBits;
    int mDepthBits;
    int mStencilBits;
    EGLenum mComponentType;
    bool mMultisample;
    bool mDebug;
    bool mNoError;
    bool mWebGLCompatibility;
    bool mBindGeneratesResource;
    bool mClientArraysEnabled;
    bool mRobustAccess;
    Optional<bool> mRobustResourceInit;
    EGLint mSwapInterval;
    EGLint mSamples;
    Optional<bool> mDebugLayersEnabled;
    Optional<bool> mContextProgramCacheEnabled;
    angle::PlatformMethods *mPlatformMethods;
};

ANGLE_EXPORT bool CheckExtensionExists(const char *allExtensions, const std::string &extName);

#endif // UTIL_EGLWINDOW_H_
