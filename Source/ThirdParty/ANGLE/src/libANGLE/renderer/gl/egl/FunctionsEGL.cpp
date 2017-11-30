//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FunctionsEGL.cpp: Implements the FunctionsEGL class.

#include "libANGLE/renderer/gl/egl/FunctionsEGL.h"

#include <algorithm>

#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/egl/functionsegl_typedefs.h"
#include "common/string_utils.h"

namespace
{

template <typename T>
bool SetPtr(T *dst, void *src)
{
    if (src)
    {
        *dst = reinterpret_cast<T>(src);
        return true;
    }
    return false;
}
}  // namespace

namespace rx
{

struct FunctionsEGL::EGLDispatchTable
{
    EGLDispatchTable()
        : bindAPIPtr(nullptr),
          chooseConfigPtr(nullptr),
          createContextPtr(nullptr),
          createPbufferSurfacePtr(nullptr),
          createWindowSurfacePtr(nullptr),
          destroyContextPtr(nullptr),
          destroySurfacePtr(nullptr),
          getConfigAttribPtr(nullptr),
          getDisplayPtr(nullptr),
          getErrorPtr(nullptr),
          initializePtr(nullptr),
          makeCurrentPtr(nullptr),
          queryStringPtr(nullptr),
          querySurfacePtr(nullptr),
          swapBuffersPtr(nullptr),
          terminatePtr(nullptr),

          bindTexImagePtr(nullptr),
          releaseTexImagePtr(nullptr),
          swapIntervalPtr(nullptr),

          createImageKHRPtr(nullptr),
          destroyImageKHRPtr(nullptr),

          clientWaitSyncKHRPtr(nullptr),
          createSyncKHRPtr(nullptr),
          destroySyncKHRPtr(nullptr),
          getSyncAttribKHRPtr(nullptr)
    {
    }

    // 1.0
    PFNEGLBINDAPIPROC bindAPIPtr;
    PFNEGLCHOOSECONFIGPROC chooseConfigPtr;
    PFNEGLCREATECONTEXTPROC createContextPtr;
    PFNEGLCREATEPBUFFERSURFACEPROC createPbufferSurfacePtr;
    PFNEGLCREATEWINDOWSURFACEPROC createWindowSurfacePtr;
    PFNEGLDESTROYCONTEXTPROC destroyContextPtr;
    PFNEGLDESTROYSURFACEPROC destroySurfacePtr;
    PFNEGLGETCONFIGATTRIBPROC getConfigAttribPtr;
    PFNEGLGETDISPLAYPROC getDisplayPtr;
    PFNEGLGETERRORPROC getErrorPtr;
    PFNEGLINITIALIZEPROC initializePtr;
    PFNEGLMAKECURRENTPROC makeCurrentPtr;
    PFNEGLQUERYSTRINGPROC queryStringPtr;
    PFNEGLQUERYSURFACEPROC querySurfacePtr;
    PFNEGLSWAPBUFFERSPROC swapBuffersPtr;
    PFNEGLTERMINATEPROC terminatePtr;

    // 1.1
    PFNEGLBINDTEXIMAGEPROC bindTexImagePtr;
    PFNEGLRELEASETEXIMAGEPROC releaseTexImagePtr;
    PFNEGLSWAPINTERVALPROC swapIntervalPtr;

    // EGL_KHR_image
    PFNEGLCREATEIMAGEKHRPROC createImageKHRPtr;
    PFNEGLDESTROYIMAGEKHRPROC destroyImageKHRPtr;

    // EGL_KHR_fence_sync
    PFNEGLCLIENTWAITSYNCKHRPROC clientWaitSyncKHRPtr;
    PFNEGLCREATESYNCKHRPROC createSyncKHRPtr;
    PFNEGLDESTROYSYNCKHRPROC destroySyncKHRPtr;
    PFNEGLGETSYNCATTRIBKHRPROC getSyncAttribKHRPtr;
};

FunctionsEGL::FunctionsEGL()
    : majorVersion(0), minorVersion(0), mFnPtrs(new EGLDispatchTable()), mEGLDisplay(EGL_NO_DISPLAY)
{
}

FunctionsEGL::~FunctionsEGL()
{
    SafeDelete(mFnPtrs);
}

egl::Error FunctionsEGL::initialize(EGLNativeDisplayType nativeDisplay)
{
#define ANGLE_GET_PROC_OR_ERROR(MEMBER, NAME)                                       \
    if (!SetPtr(MEMBER, getProcAddress(#NAME)))                                     \
    {                                                                               \
        return egl::EglNotInitialized() << "Could not load EGL entry point " #NAME; \
    }

    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->bindAPIPtr, eglBindAPI);
    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->chooseConfigPtr, eglChooseConfig);
    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->createContextPtr, eglCreateContext);
    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->createPbufferSurfacePtr, eglCreatePbufferSurface);
    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->createWindowSurfacePtr, eglCreateWindowSurface);
    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->destroyContextPtr, eglDestroyContext);
    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->destroySurfacePtr, eglDestroySurface);
    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->getConfigAttribPtr, eglGetConfigAttrib);
    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->getDisplayPtr, eglGetDisplay);
    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->getErrorPtr, eglGetError);
    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->initializePtr, eglInitialize);
    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->makeCurrentPtr, eglMakeCurrent);
    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->queryStringPtr, eglQueryString);
    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->querySurfacePtr, eglQuerySurface);
    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->swapBuffersPtr, eglSwapBuffers);
    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->terminatePtr, eglTerminate);

    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->bindTexImagePtr, eglBindTexImage);
    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->releaseTexImagePtr, eglReleaseTexImage);
    ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->swapIntervalPtr, eglSwapInterval);

    mEGLDisplay = mFnPtrs->getDisplayPtr(nativeDisplay);
    if (mEGLDisplay == EGL_NO_DISPLAY)
    {
        return egl::EglNotInitialized() << "Failed to get system egl display";
    }
    if (mFnPtrs->initializePtr(mEGLDisplay, &majorVersion, &minorVersion) != EGL_TRUE)
    {
        return egl::Error(mFnPtrs->getErrorPtr(), "Failed to initialize system egl");
    }
    if (majorVersion < 1 || (majorVersion == 1 && minorVersion < 4))
    {
        return egl::EglNotInitialized() << "Unsupported EGL version (require at least 1.4).";
    }
    if (mFnPtrs->bindAPIPtr(EGL_OPENGL_ES_API) != EGL_TRUE)
    {
        return egl::Error(mFnPtrs->getErrorPtr(), "Failed to bind API in system egl");
    }

    const char *extensions = queryString(EGL_EXTENSIONS);
    if (!extensions)
    {
        return egl::Error(mFnPtrs->getErrorPtr(), "Faild to query extensions in system egl");
    }
    angle::SplitStringAlongWhitespace(extensions, &mExtensions);

    if (hasExtension("EGL_KHR_image_base"))
    {
        ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->createImageKHRPtr, eglCreateImageKHR);
        ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->destroyImageKHRPtr, eglDestroyImageKHR);
    }
    if (hasExtension("EGL_KHR_fence_sync"))
    {
        ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->clientWaitSyncKHRPtr, eglClientWaitSyncKHR);
        ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->createSyncKHRPtr, eglCreateSyncKHR);
        ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->destroySyncKHRPtr, eglDestroySyncKHR);
        ANGLE_GET_PROC_OR_ERROR(&mFnPtrs->getSyncAttribKHRPtr, eglGetSyncAttribKHR);
    }

#undef ANGLE_GET_PROC_OR_ERROR

    return egl::NoError();
}

egl::Error FunctionsEGL::terminate()
{
    if (mFnPtrs->terminatePtr == nullptr || mFnPtrs->terminatePtr(mEGLDisplay) == EGL_TRUE)
    {
        mEGLDisplay = nullptr;
        return egl::NoError();
    }
    return egl::Error(mFnPtrs->getErrorPtr());
}

class FunctionsGLEGL : public FunctionsGL
{
  public:
    FunctionsGLEGL(const FunctionsEGL &egl) : mEGL(egl) {}

    ~FunctionsGLEGL() override {}

  private:
    void *loadProcAddress(const std::string &function) const override
    {
        return mEGL.getProcAddress(function.c_str());
    }

    const FunctionsEGL &mEGL;
};

FunctionsGL *FunctionsEGL::makeFunctionsGL(void) const
{
    return new FunctionsGLEGL(*this);
}

bool FunctionsEGL::hasExtension(const char *extension) const
{
    return std::find(mExtensions.begin(), mExtensions.end(), extension) != mExtensions.end();
}

EGLDisplay FunctionsEGL::getDisplay() const
{
    return mEGLDisplay;
}

EGLint FunctionsEGL::getError() const
{
    return mFnPtrs->getErrorPtr();
}

EGLBoolean FunctionsEGL::chooseConfig(EGLint const *attribList,
                                      EGLConfig *configs,
                                      EGLint configSize,
                                      EGLint *numConfig) const
{
    return mFnPtrs->chooseConfigPtr(mEGLDisplay, attribList, configs, configSize, numConfig);
}

EGLBoolean FunctionsEGL::getConfigAttrib(EGLConfig config, EGLint attribute, EGLint *value) const
{
    return mFnPtrs->getConfigAttribPtr(mEGLDisplay, config, attribute, value);
}

EGLContext FunctionsEGL::createContext(EGLConfig config,
                                       EGLContext share_context,
                                       EGLint const *attrib_list) const
{
    return mFnPtrs->createContextPtr(mEGLDisplay, config, share_context, attrib_list);
}

EGLSurface FunctionsEGL::createPbufferSurface(EGLConfig config, const EGLint *attrib_list) const
{
    return mFnPtrs->createPbufferSurfacePtr(mEGLDisplay, config, attrib_list);
}

EGLSurface FunctionsEGL::createWindowSurface(EGLConfig config,
                                             EGLNativeWindowType win,
                                             const EGLint *attrib_list) const
{
    return mFnPtrs->createWindowSurfacePtr(mEGLDisplay, config, win, attrib_list);
}

EGLBoolean FunctionsEGL::destroyContext(EGLContext context) const
{
    return mFnPtrs->destroyContextPtr(mEGLDisplay, context);
}

EGLBoolean FunctionsEGL::destroySurface(EGLSurface surface) const
{
    return mFnPtrs->destroySurfacePtr(mEGLDisplay, surface);
}

EGLBoolean FunctionsEGL::makeCurrent(EGLSurface surface, EGLContext context) const
{
    return mFnPtrs->makeCurrentPtr(mEGLDisplay, surface, surface, context);
}

char const *FunctionsEGL::queryString(EGLint name) const
{
    return mFnPtrs->queryStringPtr(mEGLDisplay, name);
}

EGLBoolean FunctionsEGL::querySurface(EGLSurface surface, EGLint attribute, EGLint *value) const
{
    return mFnPtrs->querySurfacePtr(mEGLDisplay, surface, attribute, value);
}

EGLBoolean FunctionsEGL::swapBuffers(EGLSurface surface) const
{
    return mFnPtrs->swapBuffersPtr(mEGLDisplay, surface);
}

EGLBoolean FunctionsEGL::bindTexImage(EGLSurface surface, EGLint buffer) const
{
    return mFnPtrs->bindTexImagePtr(mEGLDisplay, surface, buffer);
}

EGLBoolean FunctionsEGL::releaseTexImage(EGLSurface surface, EGLint buffer) const
{
    return mFnPtrs->releaseTexImagePtr(mEGLDisplay, surface, buffer);
}

EGLBoolean FunctionsEGL::swapInterval(EGLint interval) const
{
    return mFnPtrs->swapIntervalPtr(mEGLDisplay, interval);
}

EGLImageKHR FunctionsEGL::createImageKHR(EGLContext context,
                                         EGLenum target,
                                         EGLClientBuffer buffer,
                                         const EGLint *attrib_list) const
{
    return mFnPtrs->createImageKHRPtr(mEGLDisplay, context, target, buffer, attrib_list);
}

EGLBoolean FunctionsEGL::destroyImageKHR(EGLImageKHR image) const
{
    return mFnPtrs->destroyImageKHRPtr(mEGLDisplay, image);
}

EGLSyncKHR FunctionsEGL::createSyncKHR(EGLenum type, const EGLint *attrib_list)
{
    return mFnPtrs->createSyncKHRPtr(mEGLDisplay, type, attrib_list);
}

EGLBoolean FunctionsEGL::destroySyncKHR(EGLSyncKHR sync)
{
    return mFnPtrs->destroySyncKHRPtr(mEGLDisplay, sync);
}

EGLint FunctionsEGL::clientWaitSyncKHR(EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout)
{
    return mFnPtrs->clientWaitSyncKHRPtr(mEGLDisplay, sync, flags, timeout);
}

EGLBoolean FunctionsEGL::getSyncAttribKHR(EGLSyncKHR sync, EGLint attribute, EGLint *value)
{
    return mFnPtrs->getSyncAttribKHRPtr(mEGLDisplay, sync, attribute, value);
}
}  // namespace rx
