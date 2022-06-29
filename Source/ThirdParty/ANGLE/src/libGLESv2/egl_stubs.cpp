//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// egl_stubs.cpp: Stubs for EGL entry points.
//

#include "libGLESv2/egl_stubs_autogen.h"

#include "common/angle_version_info.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/EGLSync.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Thread.h"
#include "libANGLE/capture/capture_egl.h"
#include "libANGLE/capture/frame_capture_utils_autogen.h"
#include "libANGLE/capture/gl_enum_utils_autogen.h"
#include "libANGLE/queryutils.h"
#include "libANGLE/validationEGL.h"
#include "libGLESv2/global_state.h"
#include "libGLESv2/proc_table_egl.h"

namespace egl
{
namespace
{

bool CompareProc(const ProcEntry &a, const char *b)
{
    return strcmp(a.first, b) < 0;
}

void ClipConfigs(const std::vector<const Config *> &filteredConfigs,
                 EGLConfig *outputConfigs,
                 EGLint configSize,
                 EGLint *numConfigs)
{
    EGLint resultSize = static_cast<EGLint>(filteredConfigs.size());
    if (outputConfigs)
    {
        resultSize = std::max(std::min(resultSize, configSize), 0);
        for (EGLint i = 0; i < resultSize; i++)
        {
            outputConfigs[i] = const_cast<Config *>(filteredConfigs[i]);
        }
    }
    *numConfigs = resultSize;
}
}  // anonymous namespace

EGLBoolean BindAPI(Thread *thread, EGLenum api)
{
    thread->setAPI(api);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean BindTexImage(Thread *thread, Display *display, Surface *eglSurface, EGLint buffer)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglBindTexImage",
                         GetDisplayIfValid(display), EGL_FALSE);

    gl::Context *context = thread->getContext();
    if (context && !context->isContextLost())
    {
        gl::TextureType type =
            egl_gl::EGLTextureTargetToTextureType(eglSurface->getTextureTarget());
        gl::Texture *textureObject = context->getTextureByType(type);
        ANGLE_EGL_TRY_RETURN(thread, eglSurface->bindTexImage(context, textureObject, buffer),
                             "eglBindTexImage", GetSurfaceIfValid(display, eglSurface), EGL_FALSE);
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean ChooseConfig(Thread *thread,
                        Display *display,
                        const AttributeMap &attribMap,
                        EGLConfig *configs,
                        EGLint config_size,
                        EGLint *num_config)
{
    ClipConfigs(display->chooseConfig(attribMap), configs, config_size, num_config);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLint ClientWaitSync(Thread *thread,
                      Display *display,
                      Sync *syncObject,
                      EGLint flags,
                      EGLTime timeout)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglClientWaitSync",
                         GetDisplayIfValid(display), EGL_FALSE);
    gl::Context *currentContext = thread->getContext();
    EGLint syncStatus           = EGL_FALSE;
    ANGLE_EGL_TRY_RETURN(
        thread, syncObject->clientWait(display, currentContext, flags, timeout, &syncStatus),
        "eglClientWaitSync", GetSyncIfValid(display, syncObject), EGL_FALSE);

    thread->setSuccess();
    return syncStatus;
}

EGLBoolean CopyBuffers(Thread *thread,
                       Display *display,
                       Surface *eglSurface,
                       EGLNativePixmapType target)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglCopyBuffers",
                         GetDisplayIfValid(display), EGL_FALSE);
    UNIMPLEMENTED();  // FIXME

    thread->setSuccess();
    return 0;
}

EGLContext CreateContext(Thread *thread,
                         Display *display,
                         Config *configuration,
                         gl::Context *sharedGLContext,
                         const AttributeMap &attributes)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglCreateContext",
                         GetDisplayIfValid(display), EGL_NO_CONTEXT);
    gl::Context *context = nullptr;
    ANGLE_EGL_TRY_RETURN(thread,
                         display->createContext(configuration, sharedGLContext, thread->getAPI(),
                                                attributes, &context),
                         "eglCreateContext", GetDisplayIfValid(display), EGL_NO_CONTEXT);

    thread->setSuccess();
    return static_cast<EGLContext>(context);
}

EGLImage CreateImage(Thread *thread,
                     Display *display,
                     gl::Context *context,
                     EGLenum target,
                     EGLClientBuffer buffer,
                     const AttributeMap &attributes)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglCreateImage",
                         GetDisplayIfValid(display), EGL_FALSE);

    Image *image = nullptr;
    Error error  = display->createImage(context, target, buffer, attributes, &image);
    if (error.isError())
    {
        thread->setError(error, "eglCreateImage", GetDisplayIfValid(display));
        return EGL_NO_IMAGE;
    }

    ANGLE_CAPTURE_EGL(EGLCreateImage, thread, target, buffer, attributes, image);

    thread->setSuccess();
    return static_cast<EGLImage>(image);
}

EGLSurface CreatePbufferFromClientBuffer(Thread *thread,
                                         Display *display,
                                         EGLenum buftype,
                                         EGLClientBuffer buffer,
                                         Config *configuration,
                                         const AttributeMap &attributes)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglCreatePbufferFromClientBuffer",
                         GetDisplayIfValid(display), EGL_NO_SURFACE);
    Surface *surface = nullptr;
    ANGLE_EGL_TRY_RETURN(thread,
                         display->createPbufferFromClientBuffer(configuration, buftype, buffer,
                                                                attributes, &surface),
                         "eglCreatePbufferFromClientBuffer", GetDisplayIfValid(display),
                         EGL_NO_SURFACE);

    return static_cast<EGLSurface>(surface);
}

EGLSurface CreatePbufferSurface(Thread *thread,
                                Display *display,
                                Config *configuration,
                                const AttributeMap &attributes)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglCreatePbufferSurface",
                         GetDisplayIfValid(display), EGL_NO_SURFACE);
    Surface *surface = nullptr;
    ANGLE_EGL_TRY_RETURN(thread, display->createPbufferSurface(configuration, attributes, &surface),
                         "eglCreatePbufferSurface", GetDisplayIfValid(display), EGL_NO_SURFACE);

    return static_cast<EGLSurface>(surface);
}

EGLSurface CreatePixmapSurface(Thread *thread,
                               Display *display,
                               Config *configuration,
                               EGLNativePixmapType pixmap,
                               const AttributeMap &attributes)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglCreatePixmapSurface",
                         GetDisplayIfValid(display), EGL_NO_SURFACE);
    Surface *surface = nullptr;
    ANGLE_EGL_TRY_RETURN(thread,
                         display->createPixmapSurface(configuration, pixmap, attributes, &surface),
                         "eglCreatePixmapSurface", GetDisplayIfValid(display), EGL_NO_SURFACE);

    thread->setSuccess();
    return static_cast<EGLSurface>(surface);
}

EGLSurface CreatePlatformPixmapSurface(Thread *thread,
                                       Display *display,
                                       Config *configuration,
                                       void *pixmap,
                                       const AttributeMap &attributes)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglCreatePlatformPixmapSurface",
                         GetDisplayIfValid(display), EGL_NO_SURFACE);
    Surface *surface                 = nullptr;
    EGLNativePixmapType nativePixmap = reinterpret_cast<EGLNativePixmapType>(pixmap);
    ANGLE_EGL_TRY_RETURN(
        thread, display->createPixmapSurface(configuration, nativePixmap, attributes, &surface),
        "eglCreatePlatformPixmapSurface", GetDisplayIfValid(display), EGL_NO_SURFACE);

    thread->setSuccess();
    return static_cast<EGLSurface>(surface);
}

EGLSurface CreatePlatformWindowSurface(Thread *thread,
                                       Display *display,
                                       Config *configuration,
                                       void *win,
                                       const AttributeMap &attributes)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglCreatePlatformWindowSurface",
                         GetDisplayIfValid(display), EGL_NO_SURFACE);
    Surface *surface                 = nullptr;
    EGLNativeWindowType nativeWindow = reinterpret_cast<EGLNativeWindowType>(win);
    ANGLE_EGL_TRY_RETURN(
        thread, display->createWindowSurface(configuration, nativeWindow, attributes, &surface),
        "eglPlatformCreateWindowSurface", GetDisplayIfValid(display), EGL_NO_SURFACE);

    return static_cast<EGLSurface>(surface);
}

EGLSync CreateSync(Thread *thread, Display *display, EGLenum type, const AttributeMap &attributes)
{
    gl::Context *currentContext = thread->getContext();

    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglCreateSync",
                         GetDisplayIfValid(display), EGL_FALSE);
    Sync *syncObject = nullptr;
    ANGLE_EGL_TRY_RETURN(thread, display->createSync(currentContext, type, attributes, &syncObject),
                         "eglCreateSync", GetDisplayIfValid(display), EGL_NO_SYNC);

    thread->setSuccess();
    return static_cast<EGLSync>(syncObject);
}

EGLSurface CreateWindowSurface(Thread *thread,
                               Display *display,
                               Config *configuration,
                               EGLNativeWindowType win,
                               const AttributeMap &attributes)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglCreateWindowSurface",
                         GetDisplayIfValid(display), EGL_NO_SURFACE);

    Surface *surface = nullptr;
    ANGLE_EGL_TRY_RETURN(thread,
                         display->createWindowSurface(configuration, win, attributes, &surface),
                         "eglCreateWindowSurface", GetDisplayIfValid(display), EGL_NO_SURFACE);

    return static_cast<EGLSurface>(surface);
}

EGLBoolean DestroyContext(Thread *thread, Display *display, gl::Context *context)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglDestroyContext",
                         GetDisplayIfValid(display), EGL_FALSE);

    ScopedSyncCurrentContextFromThread scopedSyncCurrent(thread);

    ANGLE_EGL_TRY_RETURN(thread, display->destroyContext(thread, context), "eglDestroyContext",
                         GetContextIfValid(display, context), EGL_FALSE);
    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean DestroyImage(Thread *thread, Display *display, Image *img)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglDestroyImage",
                         GetDisplayIfValid(display), EGL_FALSE);
    display->destroyImage(img);

    ANGLE_CAPTURE_EGL(EGLDestroyImage, thread, display, img);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean DestroySurface(Thread *thread, Display *display, Surface *eglSurface)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglDestroySurface",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, display->destroySurface(eglSurface), "eglDestroySurface",
                         GetSurfaceIfValid(display, eglSurface), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean DestroySync(Thread *thread, Display *display, Sync *syncObject)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglDestroySync",
                         GetDisplayIfValid(display), EGL_FALSE);
    display->destroySync(syncObject);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean GetConfigAttrib(Thread *thread,
                           Display *display,
                           Config *configuration,
                           EGLint attribute,
                           EGLint *value)
{
    QueryConfigAttrib(configuration, attribute, value);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean GetConfigs(Thread *thread,
                      Display *display,
                      EGLConfig *configs,
                      EGLint config_size,
                      EGLint *num_config)
{
    ClipConfigs(display->getConfigs(AttributeMap()), configs, config_size, num_config);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLContext GetCurrentContext(Thread *thread)
{
    gl::Context *context = thread->getContext();

    thread->setSuccess();
    return static_cast<EGLContext>(context);
}

EGLDisplay GetCurrentDisplay(Thread *thread)
{
    thread->setSuccess();
    if (thread->getContext() != nullptr)
    {
        return thread->getContext()->getDisplay();
    }
    return EGL_NO_DISPLAY;
}

EGLSurface GetCurrentSurface(Thread *thread, EGLint readdraw)
{
    if (readdraw == EGL_READ)
    {
        thread->setSuccess();
        return thread->getCurrentReadSurface();
    }
    else if (readdraw == EGL_DRAW)
    {
        thread->setSuccess();
        return thread->getCurrentDrawSurface();
    }
    else
    {
        thread->setError(EglBadParameter(), "eglGetCurrentSurface", nullptr);
        return EGL_NO_SURFACE;
    }
}

EGLDisplay GetDisplay(Thread *thread, EGLNativeDisplayType display_id)
{
    return Display::GetDisplayFromNativeDisplay(EGL_PLATFORM_ANGLE_ANGLE, display_id,
                                                AttributeMap());
}

EGLint GetError(Thread *thread)
{
    EGLint error = thread->getError();
    thread->setSuccess();
    return error;
}

EGLDisplay GetPlatformDisplay(Thread *thread,
                              EGLenum platform,
                              void *native_display,
                              const AttributeMap &attribMap)
{
    switch (platform)
    {
        case EGL_PLATFORM_ANGLE_ANGLE:
        case EGL_PLATFORM_GBM_KHR:
        {
            return Display::GetDisplayFromNativeDisplay(
                platform, gl::bitCast<EGLNativeDisplayType>(native_display), attribMap);
        }
        case EGL_PLATFORM_DEVICE_EXT:
        {
            Device *eglDevice = static_cast<Device *>(native_display);
            return Display::GetDisplayFromDevice(eglDevice, attribMap);
        }
        default:
        {
            UNREACHABLE();
            return EGL_NO_DISPLAY;
        }
    }
}

__eglMustCastToProperFunctionPointerType GetProcAddress(Thread *thread, const char *procname)
{
    const ProcEntry *entry =
        std::lower_bound(&g_procTable[0], &g_procTable[g_numProcs], procname, CompareProc);

    thread->setSuccess();

    if (entry == &g_procTable[g_numProcs] || strcmp(entry->first, procname) != 0)
    {
        return nullptr;
    }

    return entry->second;
}

EGLBoolean GetSyncAttrib(Thread *thread,
                         Display *display,
                         Sync *syncObject,
                         EGLint attribute,
                         EGLAttrib *value)
{
    EGLint valueExt;
    ANGLE_EGL_TRY_RETURN(thread, GetSyncAttrib(display, syncObject, attribute, &valueExt),
                         "eglGetSyncAttrib", GetSyncIfValid(display, syncObject), EGL_FALSE);
    *value = valueExt;

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean Initialize(Thread *thread, Display *display, EGLint *major, EGLint *minor)
{
    ANGLE_EGL_TRY_RETURN(thread, display->initialize(), "eglInitialize", GetDisplayIfValid(display),
                         EGL_FALSE);

    if (major)
    {
        *major = kEglMajorVersion;
    }
    if (minor)
    {
        *minor = kEglMinorVersion;
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean MakeCurrent(Thread *thread,
                       Display *display,
                       Surface *drawSurface,
                       Surface *readSurface,
                       gl::Context *context)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglMakeCurrent",
                         GetDisplayIfValid(display), EGL_FALSE);
    ScopedSyncCurrentContextFromThread scopedSyncCurrent(thread);

    Surface *previousDraw        = thread->getCurrentDrawSurface();
    Surface *previousRead        = thread->getCurrentReadSurface();
    gl::Context *previousContext = thread->getContext();

    // Only call makeCurrent if the context or surfaces have changed.
    if (previousDraw != drawSurface || previousRead != readSurface || previousContext != context)
    {
        ANGLE_EGL_TRY_RETURN(
            thread,
            display->makeCurrent(thread, previousContext, drawSurface, readSurface, context),
            "eglMakeCurrent", GetContextIfValid(display, context), EGL_FALSE);
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLenum QueryAPI(Thread *thread)
{
    EGLenum API = thread->getAPI();

    thread->setSuccess();
    return API;
}

EGLBoolean QueryContext(Thread *thread,
                        Display *display,
                        gl::Context *context,
                        EGLint attribute,
                        EGLint *value)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglQueryContext",
                         GetDisplayIfValid(display), EGL_FALSE);
    QueryContextAttrib(context, attribute, value);

    thread->setSuccess();
    return EGL_TRUE;
}

const char *QueryString(Thread *thread, Display *display, EGLint name)
{
    if (display)
    {
        ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglQueryString",
                             GetDisplayIfValid(display), nullptr);
    }

    const char *result = nullptr;
    switch (name)
    {
        case EGL_CLIENT_APIS:
            result = "OpenGL_ES";
            break;
        case EGL_EXTENSIONS:
            if (display == EGL_NO_DISPLAY)
            {
                result = Display::GetClientExtensionString().c_str();
            }
            else
            {
                result = display->getExtensionString().c_str();
            }
            break;
        case EGL_VENDOR:
            result = display->getVendorString().c_str();
            break;
        case EGL_VERSION:
        {
            static const char *sVersionString =
                MakeStaticString(std::string("1.5 (ANGLE ") + angle::GetANGLEVersionString() + ")");
            result = sVersionString;
            break;
        }
        default:
            UNREACHABLE();
            break;
    }

    thread->setSuccess();
    return result;
}

EGLBoolean QuerySurface(Thread *thread,
                        Display *display,
                        Surface *eglSurface,
                        EGLint attribute,
                        EGLint *value)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglQuerySurface",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(
        thread, QuerySurfaceAttrib(display, thread->getContext(), eglSurface, attribute, value),
        "eglQuerySurface", GetSurfaceIfValid(display, eglSurface), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean ReleaseTexImage(Thread *thread, Display *display, Surface *eglSurface, EGLint buffer)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglReleaseTexImage",
                         GetDisplayIfValid(display), EGL_FALSE);
    gl::Context *context = thread->getContext();
    if (context && !context->isContextLost())
    {
        gl::Texture *texture = eglSurface->getBoundTexture();

        if (texture)
        {
            ANGLE_EGL_TRY_RETURN(thread, eglSurface->releaseTexImage(thread->getContext(), buffer),
                                 "eglReleaseTexImage", GetSurfaceIfValid(display, eglSurface),
                                 EGL_FALSE);
        }
    }
    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean ReleaseThread(Thread *thread)
{
    ScopedSyncCurrentContextFromThread scopedSyncCurrent(thread);

    Surface *previousDraw        = thread->getCurrentDrawSurface();
    Surface *previousRead        = thread->getCurrentReadSurface();
    gl::Context *previousContext = thread->getContext();
    Display *previousDisplay     = thread->getDisplay();

    if (previousDisplay != EGL_NO_DISPLAY)
    {
        ANGLE_EGL_TRY_RETURN(thread, previousDisplay->prepareForCall(), "eglReleaseThread",
                             GetDisplayIfValid(previousDisplay), EGL_FALSE);
        // Only call makeCurrent if the context or surfaces have changed.
        if (previousDraw != EGL_NO_SURFACE || previousRead != EGL_NO_SURFACE ||
            previousContext != EGL_NO_CONTEXT)
        {
            ANGLE_EGL_TRY_RETURN(
                thread,
                previousDisplay->makeCurrent(thread, previousContext, nullptr, nullptr, nullptr),
                "eglReleaseThread", nullptr, EGL_FALSE);
        }
        ANGLE_EGL_TRY_RETURN(thread, previousDisplay->releaseThread(), "eglReleaseThread",
                             GetDisplayIfValid(previousDisplay), EGL_FALSE);
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean SurfaceAttrib(Thread *thread,
                         Display *display,
                         Surface *eglSurface,
                         EGLint attribute,
                         EGLint value)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglSurfaceAttrib",
                         GetDisplayIfValid(display), EGL_FALSE);

    ANGLE_EGL_TRY_RETURN(thread, SetSurfaceAttrib(eglSurface, attribute, value), "eglSurfaceAttrib",
                         GetDisplayIfValid(display), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean SwapBuffers(Thread *thread, Display *display, Surface *eglSurface)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglSwapBuffers",
                         GetDisplayIfValid(display), EGL_FALSE);

    ANGLE_EGL_TRY_RETURN(thread, eglSurface->swap(thread->getContext()), "eglSwapBuffers",
                         GetSurfaceIfValid(display, eglSurface), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean SwapInterval(Thread *thread, Display *display, EGLint interval)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglSwapInterval",
                         GetDisplayIfValid(display), EGL_FALSE);

    Surface *drawSurface        = static_cast<Surface *>(thread->getCurrentDrawSurface());
    const Config *surfaceConfig = drawSurface->getConfig();
    EGLint clampedInterval      = std::min(std::max(interval, surfaceConfig->minSwapInterval),
                                           surfaceConfig->maxSwapInterval);

    drawSurface->setSwapInterval(clampedInterval);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean Terminate(Thread *thread, Display *display)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglTerminate",
                         GetDisplayIfValid(display), EGL_FALSE);

    ScopedSyncCurrentContextFromThread scopedSyncCurrent(thread);

    ANGLE_EGL_TRY_RETURN(thread, display->terminate(thread, Display::TerminateReason::Api),
                         "eglTerminate", GetDisplayIfValid(display), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean WaitClient(Thread *thread)
{
    Display *display = thread->getDisplay();
    if (display == nullptr)
    {
        // EGL spec says this about eglWaitClient -
        //    If there is no current context for the current rendering API,
        //    the function has no effect but still returns EGL_TRUE.
        return EGL_TRUE;
    }

    gl::Context *context = thread->getContext();

    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglWaitClient",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, display->waitClient(context), "eglWaitClient",
                         GetContextIfValid(display, context), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean WaitGL(Thread *thread)
{
    Display *display = thread->getDisplay();
    if (display == nullptr)
    {
        // EGL spec says this about eglWaitGL -
        //    eglWaitGL is ignored if there is no current EGL rendering context for OpenGL ES.
        return EGL_TRUE;
    }

    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglWaitGL", GetDisplayIfValid(display),
                         EGL_FALSE);

    // eglWaitGL like calling eglWaitClient with the OpenGL ES API bound. Since we only implement
    // OpenGL ES we can do the call directly.
    ANGLE_EGL_TRY_RETURN(thread, display->waitClient(thread->getContext()), "eglWaitGL",
                         GetDisplayIfValid(display), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean WaitNative(Thread *thread, EGLint engine)
{
    Display *display = thread->getDisplay();
    if (display == nullptr)
    {
        // EGL spec says this about eglWaitNative -
        //    eglWaitNative is ignored if there is no current EGL rendering context.
        return EGL_TRUE;
    }

    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglWaitNative",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, display->waitNative(thread->getContext(), engine), "eglWaitNative",
                         GetThreadIfValid(thread), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean WaitSync(Thread *thread, Display *display, Sync *syncObject, EGLint flags)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglWaitSync",
                         GetDisplayIfValid(display), EGL_FALSE);
    gl::Context *currentContext = thread->getContext();
    ANGLE_EGL_TRY_RETURN(thread, syncObject->serverWait(display, currentContext, flags),
                         "eglWaitSync", GetSyncIfValid(display, syncObject), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}
}  // namespace egl
