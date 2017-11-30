//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <string.h>
#include <cassert>
#include <iostream>
#include <vector>

#include "EGLWindow.h"
#include "OSWindow.h"
#include "common/debug.h"

// TODO(jmadill): Clean this up at some point.
#define EGL_PLATFORM_ANGLE_PLATFORM_METHODS_ANGLEX 0x9999

EGLPlatformParameters::EGLPlatformParameters()
    : renderer(EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE),
      majorVersion(EGL_DONT_CARE),
      minorVersion(EGL_DONT_CARE),
      deviceType(EGL_DONT_CARE),
      presentPath(EGL_DONT_CARE)
{
}

EGLPlatformParameters::EGLPlatformParameters(EGLint renderer)
    : renderer(renderer),
      majorVersion(EGL_DONT_CARE),
      minorVersion(EGL_DONT_CARE),
      deviceType(EGL_DONT_CARE),
      presentPath(EGL_DONT_CARE)
{
    if (renderer == EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE ||
        renderer == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
    {
        deviceType = EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE;
    }
}

EGLPlatformParameters::EGLPlatformParameters(EGLint renderer,
                                             EGLint majorVersion,
                                             EGLint minorVersion,
                                             EGLint useWarp)
    : renderer(renderer),
      majorVersion(majorVersion),
      minorVersion(minorVersion),
      deviceType(useWarp),
      presentPath(EGL_DONT_CARE)
{
}

EGLPlatformParameters::EGLPlatformParameters(EGLint renderer,
                                             EGLint majorVersion,
                                             EGLint minorVersion,
                                             EGLint useWarp,
                                             EGLint presentPath)
    : renderer(renderer),
      majorVersion(majorVersion),
      minorVersion(minorVersion),
      deviceType(useWarp),
      presentPath(presentPath)
{
}

bool operator<(const EGLPlatformParameters &a, const EGLPlatformParameters &b)
{
    if (a.renderer != b.renderer)
    {
        return a.renderer < b.renderer;
    }

    if (a.majorVersion != b.majorVersion)
    {
        return a.majorVersion < b.majorVersion;
    }

    if (a.minorVersion != b.minorVersion)
    {
        return a.minorVersion < b.minorVersion;
    }

    if (a.deviceType != b.deviceType)
    {
        return a.deviceType < b.deviceType;
    }

    return a.presentPath < b.presentPath;
}

bool operator==(const EGLPlatformParameters &a, const EGLPlatformParameters &b)
{
    return (a.renderer == b.renderer) && (a.majorVersion == b.majorVersion) &&
           (a.minorVersion == b.minorVersion) && (a.deviceType == b.deviceType) &&
           (a.presentPath == b.presentPath);
}

EGLWindow::EGLWindow(EGLint glesMajorVersion,
                     EGLint glesMinorVersion,
                     const EGLPlatformParameters &platform)
    : mDisplay(EGL_NO_DISPLAY),
      mSurface(EGL_NO_SURFACE),
      mContext(EGL_NO_CONTEXT),
      mClientMajorVersion(glesMajorVersion),
      mClientMinorVersion(glesMinorVersion),
      mEGLMajorVersion(0),
      mEGLMinorVersion(0),
      mPlatform(platform),
      mRedBits(-1),
      mGreenBits(-1),
      mBlueBits(-1),
      mAlphaBits(-1),
      mDepthBits(-1),
      mStencilBits(-1),
      mComponentType(EGL_COLOR_COMPONENT_TYPE_FIXED_EXT),
      mMultisample(false),
      mDebug(false),
      mNoError(false),
      mWebGLCompatibility(false),
      mBindGeneratesResource(true),
      mClientArraysEnabled(true),
      mRobustAccess(false),
      mRobustResourceInit(),
      mSwapInterval(-1),
      mSamples(-1),
      mDebugLayersEnabled(),
      mContextProgramCacheEnabled(),
      mPlatformMethods(nullptr)
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
    if (!initializeDisplayAndSurface(osWindow))
        return false;
    return initializeContext();
}

bool EGLWindow::initializeDisplayAndSurface(OSWindow *osWindow)
{
    std::vector<EGLAttrib> displayAttributes;
    displayAttributes.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
    displayAttributes.push_back(mPlatform.renderer);
    displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE);
    displayAttributes.push_back(mPlatform.majorVersion);
    displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE);
    displayAttributes.push_back(mPlatform.minorVersion);

    if (mPlatform.deviceType != EGL_DONT_CARE)
    {
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
        displayAttributes.push_back(mPlatform.deviceType);
    }

    if (mPlatform.presentPath != EGL_DONT_CARE)
    {
        const char *extensionString =
            static_cast<const char *>(eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS));
        if (strstr(extensionString, "EGL_ANGLE_experimental_present_path") == nullptr)
        {
            destroyGL();
            return false;
        }

        displayAttributes.push_back(EGL_EXPERIMENTAL_PRESENT_PATH_ANGLE);
        displayAttributes.push_back(mPlatform.presentPath);
    }

    // Set debug layer settings if requested.
    if (mDebugLayersEnabled.valid())
    {
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_DEBUG_LAYERS_ENABLED_ANGLE);
        displayAttributes.push_back(mDebugLayersEnabled.value() ? EGL_TRUE : EGL_FALSE);
    }

    if (mPlatformMethods)
    {
        static_assert(sizeof(EGLAttrib) == sizeof(mPlatformMethods), "Unexpected pointer size");
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_PLATFORM_METHODS_ANGLEX);
        displayAttributes.push_back(reinterpret_cast<EGLAttrib>(mPlatformMethods));
    }

    displayAttributes.push_back(EGL_NONE);

    mDisplay = eglGetPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE,
                                     reinterpret_cast<void *>(osWindow->getNativeDisplay()),
                                     &displayAttributes[0]);
    if (mDisplay == EGL_NO_DISPLAY)
    {
        destroyGL();
        return false;
    }

    if (eglInitialize(mDisplay, &mEGLMajorVersion, &mEGLMinorVersion) == EGL_FALSE)
    {
        destroyGL();
        return false;
    }

    const char *displayExtensions = eglQueryString(mDisplay, EGL_EXTENSIONS);

    std::vector<EGLint> configAttributes = {
        EGL_RED_SIZE,       (mRedBits >= 0) ? mRedBits : EGL_DONT_CARE,
        EGL_GREEN_SIZE,     (mGreenBits >= 0) ? mGreenBits : EGL_DONT_CARE,
        EGL_BLUE_SIZE,      (mBlueBits >= 0) ? mBlueBits : EGL_DONT_CARE,
        EGL_ALPHA_SIZE,     (mAlphaBits >= 0) ? mAlphaBits : EGL_DONT_CARE,
        EGL_DEPTH_SIZE,     (mDepthBits >= 0) ? mDepthBits : EGL_DONT_CARE,
        EGL_STENCIL_SIZE,   (mStencilBits >= 0) ? mStencilBits : EGL_DONT_CARE,
        EGL_SAMPLE_BUFFERS, mMultisample ? 1 : 0,
        EGL_SAMPLES,        (mSamples >= 0) ? mSamples : EGL_DONT_CARE,
    };

    // Add dynamic attributes
    bool hasPixelFormatFloat = strstr(displayExtensions, "EGL_EXT_pixel_format_float") != nullptr;
    if (!hasPixelFormatFloat && mComponentType != EGL_COLOR_COMPONENT_TYPE_FIXED_EXT)
    {
        destroyGL();
        return false;
    }
    if (hasPixelFormatFloat)
    {
        configAttributes.push_back(EGL_COLOR_COMPONENT_TYPE_EXT);
        configAttributes.push_back(mComponentType);
    }

    // Finish the attribute list
    configAttributes.push_back(EGL_NONE);

    if (!FindEGLConfig(mDisplay, configAttributes.data(), &mConfig))
    {
        std::cout << "Could not find a suitable EGL config!" << std::endl;
        destroyGL();
        return false;
    }

    eglGetConfigAttrib(mDisplay, mConfig, EGL_RED_SIZE, &mRedBits);
    eglGetConfigAttrib(mDisplay, mConfig, EGL_GREEN_SIZE, &mGreenBits);
    eglGetConfigAttrib(mDisplay, mConfig, EGL_BLUE_SIZE, &mBlueBits);
    eglGetConfigAttrib(mDisplay, mConfig, EGL_ALPHA_SIZE, &mAlphaBits);
    eglGetConfigAttrib(mDisplay, mConfig, EGL_DEPTH_SIZE, &mDepthBits);
    eglGetConfigAttrib(mDisplay, mConfig, EGL_STENCIL_SIZE, &mStencilBits);
    eglGetConfigAttrib(mDisplay, mConfig, EGL_SAMPLES, &mSamples);

    std::vector<EGLint> surfaceAttributes;
    if (strstr(displayExtensions, "EGL_NV_post_sub_buffer") != nullptr)
    {
        surfaceAttributes.push_back(EGL_POST_SUB_BUFFER_SUPPORTED_NV);
        surfaceAttributes.push_back(EGL_TRUE);
    }

    bool hasRobustResourceInit =
        strstr(displayExtensions, "EGL_ANGLE_robust_resource_initialization") != nullptr;
    if (hasRobustResourceInit && mRobustResourceInit.valid())
    {
        surfaceAttributes.push_back(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE);
        surfaceAttributes.push_back(mRobustResourceInit.value() ? EGL_TRUE : EGL_FALSE);
    }

    surfaceAttributes.push_back(EGL_NONE);

    mSurface = eglCreateWindowSurface(mDisplay, mConfig, osWindow->getNativeWindow(), &surfaceAttributes[0]);
    if (eglGetError() != EGL_SUCCESS)
    {
        destroyGL();
        return false;
    }
    ASSERT(mSurface != EGL_NO_SURFACE);
    return true;
}

bool EGLWindow::initializeContext()
{
    const char *displayExtensions = eglQueryString(mDisplay, EGL_EXTENSIONS);

    // EGL_KHR_create_context is required to request a ES3+ context.
    bool hasKHRCreateContext = strstr(displayExtensions, "EGL_KHR_create_context") != nullptr;
    if (mClientMajorVersion > 2 && !(mEGLMajorVersion > 1 || mEGLMinorVersion >= 5) &&
        !hasKHRCreateContext)
    {
        destroyGL();
        return false;
    }

    bool hasWebGLCompatibility =
        strstr(displayExtensions, "EGL_ANGLE_create_context_webgl_compatibility") != nullptr;
    if (mWebGLCompatibility && !hasWebGLCompatibility)
    {
        destroyGL();
        return false;
    }

    bool hasRobustness = strstr(displayExtensions, "EGL_EXT_create_context_robustness") != nullptr;
    if (mRobustAccess && !hasRobustness)
    {
        destroyGL();
        return false;
    }

    bool hasBindGeneratesResource =
        strstr(displayExtensions, "EGL_CHROMIUM_create_context_bind_generates_resource") != nullptr;
    if (!mBindGeneratesResource && !hasBindGeneratesResource)
    {
        destroyGL();
        return false;
    }

    bool hasClientArraysExtension =
        strstr(displayExtensions, "EGL_ANGLE_create_context_client_arrays") != nullptr;
    if (!mClientArraysEnabled && !hasClientArraysExtension)
    {
        // Non-default state requested without the extension present
        destroyGL();
        return false;
    }

    bool hasProgramCacheControlExtension =
        strstr(displayExtensions, "EGL_ANGLE_program_cache_control ") != nullptr;
    if (mContextProgramCacheEnabled.valid() && !hasProgramCacheControlExtension)
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

    std::vector<EGLint> contextAttributes;
    if (hasKHRCreateContext)
    {
        contextAttributes.push_back(EGL_CONTEXT_MAJOR_VERSION_KHR);
        contextAttributes.push_back(mClientMajorVersion);

        contextAttributes.push_back(EGL_CONTEXT_MINOR_VERSION_KHR);
        contextAttributes.push_back(mClientMinorVersion);

        contextAttributes.push_back(EGL_CONTEXT_OPENGL_DEBUG);
        contextAttributes.push_back(mDebug ? EGL_TRUE : EGL_FALSE);

        // TODO(jmadill): Check for the extension string.
        // bool hasKHRCreateContextNoError = strstr(displayExtensions,
        // "EGL_KHR_create_context_no_error") != nullptr;

        contextAttributes.push_back(EGL_CONTEXT_OPENGL_NO_ERROR_KHR);
        contextAttributes.push_back(mNoError ? EGL_TRUE : EGL_FALSE);

        if (hasWebGLCompatibility)
        {
            contextAttributes.push_back(EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE);
            contextAttributes.push_back(mWebGLCompatibility ? EGL_TRUE : EGL_FALSE);
        }

        if (hasRobustness)
        {
            contextAttributes.push_back(EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT);
            contextAttributes.push_back(mRobustAccess ? EGL_TRUE : EGL_FALSE);
        }

        if (hasBindGeneratesResource)
        {
            contextAttributes.push_back(EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM);
            contextAttributes.push_back(mBindGeneratesResource ? EGL_TRUE : EGL_FALSE);
        }

        if (hasClientArraysExtension)
        {
            contextAttributes.push_back(EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE);
            contextAttributes.push_back(mClientArraysEnabled ? EGL_TRUE : EGL_FALSE);
        }

        if (mContextProgramCacheEnabled.valid())
        {
            contextAttributes.push_back(EGL_CONTEXT_PROGRAM_BINARY_CACHE_ENABLED_ANGLE);
            contextAttributes.push_back(mContextProgramCacheEnabled.value() ? EGL_TRUE : EGL_FALSE);
        }

        bool hasRobustResourceInit =
            strstr(displayExtensions, "EGL_ANGLE_robust_resource_initialization") != nullptr;
        if (hasRobustResourceInit && mRobustResourceInit.valid())
        {
            contextAttributes.push_back(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE);
            contextAttributes.push_back(mRobustResourceInit.value() ? EGL_TRUE : EGL_FALSE);
        }
    }
    contextAttributes.push_back(EGL_NONE);

    mContext = eglCreateContext(mDisplay, mConfig, nullptr, &contextAttributes[0]);
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

// Find an EGLConfig that is an exact match for the specified attributes. EGL_FALSE is returned if
// the EGLConfig is found.  This indicates that the EGLConfig is not supported.
EGLBoolean EGLWindow::FindEGLConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *config)
{
    EGLint numConfigs = 0;
    eglGetConfigs(dpy, nullptr, 0, &numConfigs);
    std::vector<EGLConfig> allConfigs(numConfigs);
    eglGetConfigs(dpy, allConfigs.data(), static_cast<EGLint>(allConfigs.size()), &numConfigs);

    for (size_t i = 0; i < allConfigs.size(); i++)
    {
        bool matchFound = true;
        for (const EGLint *curAttrib = attrib_list; curAttrib[0] != EGL_NONE; curAttrib += 2)
        {
            if (curAttrib[1] == EGL_DONT_CARE)
            {
                continue;
            }

            EGLint actualValue = EGL_DONT_CARE;
            eglGetConfigAttrib(dpy, allConfigs[i], curAttrib[0], &actualValue);
            if (curAttrib[1] != actualValue)
            {
                matchFound = false;
                break;
            }
        }

        if (matchFound)
        {
            *config = allConfigs[i];
            return EGL_TRUE;
        }
    }

    return EGL_FALSE;
}

void EGLWindow::makeCurrent()
{
    eglMakeCurrent(mDisplay, mSurface, mSurface, mContext);
}

// static
bool EGLWindow::ClientExtensionEnabled(const std::string &extName)
{
    return CheckExtensionExists(eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS), extName);
}

bool CheckExtensionExists(const char *allExtensions, const std::string &extName)
{
    const std::string paddedExtensions = std::string(" ") + allExtensions + std::string(" ");
    return paddedExtensions.find(std::string(" ") + extName + std::string(" ")) !=
           std::string::npos;
}
