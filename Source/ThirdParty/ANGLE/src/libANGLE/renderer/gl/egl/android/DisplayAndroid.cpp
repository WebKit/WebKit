//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayAndroid.cpp: Android implementation of egl::Display

#include "libANGLE/renderer/gl/egl/android/DisplayAndroid.h"

#include <android/log.h>
#include <android/native_window.h>

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/RendererGL.h"
#include "libANGLE/renderer/gl/egl/ContextEGL.h"
#include "libANGLE/renderer/gl/egl/FunctionsEGLDL.h"
#include "libANGLE/renderer/gl/egl/RendererEGL.h"
#include "libANGLE/renderer/gl/egl/SurfaceEGL.h"
#include "libANGLE/renderer/gl/egl/android/NativeBufferImageSiblingAndroid.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"

namespace
{
const char *GetEGLPath()
{
#if defined(__LP64__)
    return "/system/lib64/libEGL.so";
#else
    return "/system/lib/libEGL.so";
#endif
}
}  // namespace

namespace rx
{

static constexpr bool kDefaultEGLVirtualizedContexts = true;

DisplayAndroid::DisplayAndroid(const egl::DisplayState &state)
    : DisplayEGL(state),
      mVirtualizedContexts(kDefaultEGLVirtualizedContexts),
      mSupportsSurfaceless(false),
      mDummyPbuffer(EGL_NO_SURFACE)
{}

DisplayAndroid::~DisplayAndroid() {}

egl::Error DisplayAndroid::initialize(egl::Display *display)
{
    mDisplayAttributes = display->getAttributeMap();
    mVirtualizedContexts =
        ShouldUseVirtualizedContexts(mDisplayAttributes, kDefaultEGLVirtualizedContexts);

    FunctionsEGLDL *egl = new FunctionsEGLDL();
    mEGL                = egl;
    void *eglHandle =
        reinterpret_cast<void *>(mDisplayAttributes.get(EGL_PLATFORM_ANGLE_EGL_HANDLE_ANGLE, 0));
    ANGLE_TRY(egl->initialize(display->getNativeDisplayId(), GetEGLPath(), eglHandle));

    gl::Version eglVersion(mEGL->majorVersion, mEGL->minorVersion);
    ASSERT(eglVersion >= gl::Version(1, 4));

    static_assert(EGL_OPENGL_ES3_BIT == EGL_OPENGL_ES3_BIT_KHR, "Extension define must match core");
    EGLint esBit = (eglVersion >= gl::Version(1, 5) || mEGL->hasExtension("EGL_KHR_create_context"))
                       ? EGL_OPENGL_ES3_BIT
                       : EGL_OPENGL_ES2_BIT;

    // clang-format off
    std::vector<EGLint> configAttribListBase =
    {
        EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
        // Android doesn't support pixmaps
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
        EGL_CONFIG_CAVEAT, EGL_NONE,
        EGL_CONFORMANT, esBit,
        EGL_RENDERABLE_TYPE, esBit,
    };
    // clang-format on

    if (mEGL->hasExtension("EGL_EXT_pixel_format_float"))
    {
        // Don't request floating point configs
        configAttribListBase.push_back(EGL_COLOR_COMPONENT_TYPE_EXT);
        configAttribListBase.push_back(EGL_COLOR_COMPONENT_TYPE_FIXED_EXT);
    }

    std::vector<EGLint> configAttribListWithFormat = configAttribListBase;
    // EGL1.5 spec Section 2.2 says that depth, multisample and stencil buffer depths
    // must match for contexts to be compatible.
    // Choose RGBA8888
    configAttribListWithFormat.push_back(EGL_RED_SIZE);
    configAttribListWithFormat.push_back(8);
    configAttribListWithFormat.push_back(EGL_GREEN_SIZE);
    configAttribListWithFormat.push_back(8);
    configAttribListWithFormat.push_back(EGL_BLUE_SIZE);
    configAttribListWithFormat.push_back(8);
    configAttribListWithFormat.push_back(EGL_ALPHA_SIZE);
    configAttribListWithFormat.push_back(8);
    // Choose DEPTH24_STENCIL8
    configAttribListWithFormat.push_back(EGL_DEPTH_SIZE);
    configAttribListWithFormat.push_back(24);
    configAttribListWithFormat.push_back(EGL_STENCIL_SIZE);
    configAttribListWithFormat.push_back(8);
    // Choose no multisampling
    configAttribListWithFormat.push_back(EGL_SAMPLE_BUFFERS);
    configAttribListWithFormat.push_back(0);

    // Complete the attrib lists
    configAttribListBase.push_back(EGL_NONE);
    configAttribListWithFormat.push_back(EGL_NONE);

    EGLint numConfig;
    EGLConfig configWithFormat;

    EGLBoolean success =
        mEGL->chooseConfig(configAttribListWithFormat.data(), &configWithFormat, 1, &numConfig);
    if (success == EGL_FALSE)
    {
        return egl::EglNotInitialized()
               << "eglChooseConfig failed with " << egl::Error(mEGL->getError());
    }

    // A dummy pbuffer is only needed if surfaceless contexts are not supported.
    mSupportsSurfaceless = mEGL->hasExtension("EGL_KHR_surfaceless_context");
    if (!mSupportsSurfaceless)
    {
        int dummyPbufferAttribs[] = {
            EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE,
        };
        mDummyPbuffer = mEGL->createPbufferSurface(configWithFormat, dummyPbufferAttribs);
        if (mDummyPbuffer == EGL_NO_SURFACE)
        {
            return egl::EglNotInitialized()
                   << "eglCreatePbufferSurface failed with " << egl::Error(mEGL->getError());
        }
    }

    // Create mDummyPbuffer with a normal config, but create a no_config mContext, if possible
    if (mEGL->hasExtension("EGL_KHR_no_config_context"))
    {
        mConfigAttribList = configAttribListBase;
        mConfig           = EGL_NO_CONFIG_KHR;
    }
    else
    {
        mConfigAttribList = configAttribListWithFormat;
        mConfig           = configWithFormat;
    }

    ANGLE_TRY(createRenderer(EGL_NO_CONTEXT, true, &mRenderer));

    const gl::Version &maxVersion = mRenderer->getMaxSupportedESVersion();
    if (maxVersion < gl::Version(2, 0))
    {
        return egl::EglNotInitialized() << "OpenGL ES 2.0 is not supportable.";
    }

    ANGLE_TRY(DisplayGL::initialize(display));

    std::string rendererDescription = mRenderer->getRendererDescription();
    __android_log_print(ANDROID_LOG_INFO, "ANGLE", "%s", rendererDescription.c_str());
    return egl::NoError();
}

void DisplayAndroid::terminate()
{
    DisplayGL::terminate();

    EGLBoolean success = mEGL->makeCurrent(EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (success == EGL_FALSE)
    {
        ERR() << "eglMakeCurrent error " << egl::Error(mEGL->getError());
    }

    if (mDummyPbuffer != EGL_NO_SURFACE)
    {
        success       = mEGL->destroySurface(mDummyPbuffer);
        mDummyPbuffer = EGL_NO_SURFACE;
        if (success == EGL_FALSE)
        {
            ERR() << "eglDestroySurface error " << egl::Error(mEGL->getError());
        }
    }

    mRenderer.reset();
    mCurrentNativeContext.clear();

    egl::Error result = mEGL->terminate();
    if (result.isError())
    {
        ERR() << "eglTerminate error " << result;
    }

    SafeDelete(mEGL);
}

ContextImpl *DisplayAndroid::createContext(const gl::State &state,
                                           gl::ErrorSet *errorSet,
                                           const egl::Config *configuration,
                                           const gl::Context *shareContext,
                                           const egl::AttributeMap &attribs)
{
    std::shared_ptr<RendererEGL> renderer;
    if (mVirtualizedContexts)
    {
        renderer = mRenderer;
    }
    else
    {
        EGLContext nativeShareContext = EGL_NO_CONTEXT;
        if (shareContext)
        {
            ContextEGL *shareContextEGL = GetImplAs<ContextEGL>(shareContext);
            nativeShareContext          = shareContextEGL->getContext();
        }

        // Create a new renderer for this context.  It only needs to share with the user's requested
        // share context because there are no internal resources in DisplayAndroid that are shared
        // at the GL level.
        egl::Error error = createRenderer(nativeShareContext, false, &renderer);
        if (error.isError())
        {
            ERR() << "Failed to create a shared renderer: " << error.getMessage();
            return nullptr;
        }
    }

    return new ContextEGL(state, errorSet, renderer);
}

bool DisplayAndroid::isValidNativeWindow(EGLNativeWindowType window) const
{
    return ANativeWindow_getFormat(window) >= 0;
}

egl::Error DisplayAndroid::validateImageClientBuffer(const gl::Context *context,
                                                     EGLenum target,
                                                     EGLClientBuffer clientBuffer,
                                                     const egl::AttributeMap &attribs) const
{
    switch (target)
    {
        case EGL_NATIVE_BUFFER_ANDROID:
            return egl::NoError();

        default:
            return DisplayEGL::validateImageClientBuffer(context, target, clientBuffer, attribs);
    }
}

ExternalImageSiblingImpl *DisplayAndroid::createExternalImageSibling(
    const gl::Context *context,
    EGLenum target,
    EGLClientBuffer buffer,
    const egl::AttributeMap &attribs)
{
    switch (target)
    {
        case EGL_NATIVE_BUFFER_ANDROID:
            return new NativeBufferImageSiblingAndroid(buffer);

        default:
            return DisplayEGL::createExternalImageSibling(context, target, buffer, attribs);
    }
}

egl::Error DisplayAndroid::makeCurrent(egl::Surface *drawSurface,
                                       egl::Surface *readSurface,
                                       gl::Context *context)
{
    CurrentNativeContext &currentContext = mCurrentNativeContext[std::this_thread::get_id()];

    EGLSurface newSurface = EGL_NO_SURFACE;
    if (drawSurface)
    {
        SurfaceEGL *drawSurfaceEGL = GetImplAs<SurfaceEGL>(drawSurface);
        newSurface                 = drawSurfaceEGL->getSurface();
    }

    EGLContext newContext = EGL_NO_CONTEXT;
    if (context)
    {
        ContextEGL *contextEGL = GetImplAs<ContextEGL>(context);
        newContext             = contextEGL->getContext();
    }

    // The context should never change when context virtualization is being used, even when a null
    // context is being bound.
    if (mVirtualizedContexts)
    {
        ASSERT(newContext == EGL_NO_CONTEXT || currentContext.context == EGL_NO_CONTEXT ||
               newContext == currentContext.context);

        newContext = mRenderer->getContext();

        // If we know that we're only running on one thread (mVirtualizedContexts == true) and
        // EGL_NO_SURFACE is going to be bound, we can optimize this case by not changing the
        // surface binding and emulate the surfaceless extension in the frontend.
        if (newSurface == EGL_NO_SURFACE)
        {
            newSurface = currentContext.surface;
        }

        // It's possible that no surface has been created yet and the driver doesn't support
        // surfaceless, bind the dummy pbuffer.
        if (newSurface == EGL_NO_SURFACE && !mSupportsSurfaceless)
        {
            newSurface = mDummyPbuffer;
            ASSERT(newSurface != EGL_NO_SURFACE);
        }
    }

    if (newSurface != currentContext.surface || newContext != currentContext.context)
    {
        if (mEGL->makeCurrent(newSurface, newContext) == EGL_FALSE)
        {
            return egl::Error(mEGL->getError(), "eglMakeCurrent failed");
        }
        currentContext.surface = newSurface;
        currentContext.context = newContext;
    }

    return DisplayGL::makeCurrent(drawSurface, readSurface, context);
}

void DisplayAndroid::destroyNativeContext(EGLContext context)
{
    DisplayEGL::destroyNativeContext(context);

    // If this context is current, remove it from the tracking of current contexts to make sure we
    // don't try to make it current again.
    for (auto &currentContext : mCurrentNativeContext)
    {
        if (currentContext.second.context == context)
        {
            currentContext.second.surface = EGL_NO_SURFACE;
            currentContext.second.context = EGL_NO_CONTEXT;
        }
    }
}

void DisplayAndroid::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    DisplayEGL::generateExtensions(outExtensions);

    // Surfaceless can be support if the native driver supports it or we know that we are running on
    // a single thread (mVirtualizedContexts == true)
    outExtensions->surfacelessContext = mSupportsSurfaceless || mVirtualizedContexts;
}

egl::Error DisplayAndroid::createRenderer(EGLContext shareContext,
                                          bool makeNewContextCurrent,
                                          std::shared_ptr<RendererEGL> *outRenderer)
{
    EGLContext context = EGL_NO_CONTEXT;
    native_egl::AttributeVector attribs;
    ANGLE_TRY(initializeContext(shareContext, mDisplayAttributes, &context, &attribs));

    if (mEGL->makeCurrent(mDummyPbuffer, context) == EGL_FALSE)
    {
        return egl::EglNotInitialized()
               << "eglMakeCurrent failed with " << egl::Error(mEGL->getError());
    }

    std::unique_ptr<FunctionsGL> functionsGL(mEGL->makeFunctionsGL());
    functionsGL->initialize(mDisplayAttributes);

    outRenderer->reset(
        new RendererEGL(std::move(functionsGL), mDisplayAttributes, this, context, attribs));

    CurrentNativeContext &currentContext = mCurrentNativeContext[std::this_thread::get_id()];
    if (makeNewContextCurrent)
    {
        currentContext.surface = mDummyPbuffer;
        currentContext.context = context;
    }
    else
    {
        // Reset the current context back to the previous state
        if (mEGL->makeCurrent(currentContext.surface, currentContext.context) == EGL_FALSE)
        {
            return egl::EglNotInitialized()
                   << "eglMakeCurrent failed with " << egl::Error(mEGL->getError());
        }
    }

    return egl::NoError();
}

class WorkerContextAndroid final : public WorkerContext
{
  public:
    WorkerContextAndroid(EGLContext context, FunctionsEGL *functions, EGLSurface pbuffer);
    ~WorkerContextAndroid() override;

    bool makeCurrent() override;
    void unmakeCurrent() override;

  private:
    EGLContext mContext;
    FunctionsEGL *mFunctions;
    EGLSurface mPbuffer;
};

WorkerContextAndroid::WorkerContextAndroid(EGLContext context,
                                           FunctionsEGL *functions,
                                           EGLSurface pbuffer)
    : mContext(context), mFunctions(functions), mPbuffer(pbuffer)
{}

WorkerContextAndroid::~WorkerContextAndroid()
{
    mFunctions->destroyContext(mContext);
}

bool WorkerContextAndroid::makeCurrent()
{
    if (mFunctions->makeCurrent(mPbuffer, mContext) == EGL_FALSE)
    {
        ERR() << "Unable to make the EGL context current.";
        return false;
    }
    return true;
}

void WorkerContextAndroid::unmakeCurrent()
{
    mFunctions->makeCurrent(EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

WorkerContext *DisplayAndroid::createWorkerContext(std::string *infoLog,
                                                   EGLContext sharedContext,
                                                   const native_egl::AttributeVector workerAttribs)
{
    EGLContext context = mEGL->createContext(mConfig, sharedContext, workerAttribs.data());
    if (context == EGL_NO_CONTEXT)
    {
        *infoLog += "Unable to create the EGL context.";
        return nullptr;
    }
    return new WorkerContextAndroid(context, mEGL, mDummyPbuffer);
}

}  // namespace rx
