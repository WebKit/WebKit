//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// egl_ext_stubs.cpp: Stubs for EXT extension entry points.
//

#include "libGLESv2/egl_ext_stubs_autogen.h"

#include "libANGLE/Device.h"
#include "libANGLE/Display.h"
#include "libANGLE/EGLSync.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Thread.h"
#include "libANGLE/capture/capture_egl.h"
#include "libANGLE/capture/frame_capture_utils_autogen.h"
#include "libANGLE/entry_points_utils.h"
#include "libANGLE/queryutils.h"
#include "libANGLE/renderer/DisplayImpl.h"
#include "libANGLE/validationEGL.h"
#include "libANGLE/validationEGL_autogen.h"
#include "libGLESv2/global_state.h"

namespace egl
{
EGLint ClientWaitSyncKHR(Thread *thread,
                         Display *display,
                         Sync *syncObject,
                         EGLint flags,
                         EGLTimeKHR timeout)
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

EGLImageKHR CreateImageKHR(Thread *thread,
                           Display *display,
                           gl::Context *context,
                           EGLenum target,
                           EGLClientBuffer buffer,
                           const AttributeMap &attributes)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglCreateImageKHR",
                         GetDisplayIfValid(display), EGL_NO_IMAGE);
    Image *image = nullptr;
    ANGLE_EGL_TRY_RETURN(thread, display->createImage(context, target, buffer, attributes, &image),
                         "", GetDisplayIfValid(display), EGL_NO_IMAGE);

    ANGLE_CAPTURE_EGL(EGLCreateImage, thread, context, target, buffer, attributes, image);

    thread->setSuccess();
    return static_cast<EGLImage>(image);
}

EGLClientBuffer CreateNativeClientBufferANDROID(Thread *thread, const AttributeMap &attribMap)
{
    EGLClientBuffer eglClientBuffer = nullptr;
    ANGLE_EGL_TRY_RETURN(thread,
                         egl::Display::CreateNativeClientBuffer(attribMap, &eglClientBuffer),
                         "eglCreateNativeClientBufferANDROID", nullptr, nullptr);

    ANGLE_CAPTURE_EGL(CreateNativeClientBufferANDROID, thread, attribMap, eglClientBuffer);

    thread->setSuccess();
    return eglClientBuffer;
}

EGLSurface CreatePlatformPixmapSurfaceEXT(Thread *thread,
                                          Display *display,
                                          Config *configPacked,
                                          void *native_pixmap,
                                          const AttributeMap &attributes)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglCreatePlatformPixmapSurfaceEXT",
                         GetDisplayIfValid(display), EGL_NO_SURFACE);
    thread->setError(EGL_BAD_DISPLAY, "eglCreatePlatformPixmapSurfaceEXT",
                     GetDisplayIfValid(display), "CreatePlatformPixmapSurfaceEXT unimplemented.");
    return EGL_NO_SURFACE;
}

EGLSurface CreatePlatformWindowSurfaceEXT(Thread *thread,
                                          Display *display,
                                          Config *configPacked,
                                          void *native_window,
                                          const AttributeMap &attributes)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglCreatePlatformWindowSurfaceEXT",
                         GetDisplayIfValid(display), EGL_NO_SURFACE);
    Surface *surface = nullptr;

    // In X11, eglCreatePlatformWindowSurfaceEXT expects the native_window argument to be a pointer
    // to a Window while the EGLNativeWindowType for X11 is its actual value.
    // https://www.khronos.org/registry/EGL/extensions/KHR/EGL_KHR_platform_x11.txt
    void *actualNativeWindow         = display->getImplementation()->isX11()
                                           ? *reinterpret_cast<void **>(native_window)
                                           : native_window;
    EGLNativeWindowType nativeWindow = reinterpret_cast<EGLNativeWindowType>(actualNativeWindow);

    ANGLE_EGL_TRY_RETURN(
        thread, display->createWindowSurface(configPacked, nativeWindow, attributes, &surface),
        "eglPlatformCreateWindowSurfaceEXT", GetDisplayIfValid(display), EGL_NO_SURFACE);

    return static_cast<EGLSurface>(surface);
}

EGLStreamKHR CreateStreamKHR(Thread *thread, Display *display, const AttributeMap &attributes)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglCreateStreamKHR",
                         GetDisplayIfValid(display), EGL_NO_STREAM_KHR);
    Stream *stream;
    ANGLE_EGL_TRY_RETURN(thread, display->createStream(attributes, &stream), "eglCreateStreamKHR",
                         GetDisplayIfValid(display), EGL_NO_STREAM_KHR);

    thread->setSuccess();
    return static_cast<EGLStreamKHR>(stream);
}

EGLSyncKHR CreateSyncKHR(Thread *thread,
                         Display *display,
                         EGLenum type,
                         const AttributeMap &attributes)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglCreateSyncKHR",
                         GetDisplayIfValid(display), EGL_NO_SYNC);
    egl::Sync *syncObject = nullptr;
    ANGLE_EGL_TRY_RETURN(thread,
                         display->createSync(thread->getContext(), type, attributes, &syncObject),
                         "eglCreateSyncKHR", GetDisplayIfValid(display), EGL_NO_SYNC);

    thread->setSuccess();
    return static_cast<EGLSync>(syncObject);
}

EGLint DebugMessageControlKHR(Thread *thread,
                              EGLDEBUGPROCKHR callback,
                              const AttributeMap &attributes)
{
    Debug *debug = GetDebug();
    debug->setCallback(callback, attributes);

    thread->setSuccess();
    return EGL_SUCCESS;
}

EGLBoolean DestroyImageKHR(Thread *thread, Display *display, Image *img)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglDestroyImageKHR",
                         GetDisplayIfValid(display), EGL_FALSE);
    display->destroyImage(img);

    ANGLE_CAPTURE_EGL(EGLDestroyImage, thread, display, img);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean DestroyStreamKHR(Thread *thread, Display *display, Stream *streamObject)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglDestroyStreamKHR",
                         GetDisplayIfValid(display), EGL_FALSE);
    display->destroyStream(streamObject);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean DestroySyncKHR(Thread *thread, Display *display, Sync *syncObject)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglDestroySync",
                         GetDisplayIfValid(display), EGL_FALSE);
    display->destroySync(syncObject);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLint DupNativeFenceFDANDROID(Thread *thread, Display *display, Sync *syncObject)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglDupNativeFenceFDANDROID",
                         GetDisplayIfValid(display), EGL_NO_NATIVE_FENCE_FD_ANDROID);
    EGLint result = EGL_NO_NATIVE_FENCE_FD_ANDROID;
    ANGLE_EGL_TRY_RETURN(thread, syncObject->dupNativeFenceFD(display, &result),
                         "eglDupNativeFenceFDANDROID", GetSyncIfValid(display, syncObject),
                         EGL_NO_NATIVE_FENCE_FD_ANDROID);

    thread->setSuccess();
    return result;
}

EGLClientBuffer GetNativeClientBufferANDROID(Thread *thread, const struct AHardwareBuffer *buffer)
{
    thread->setSuccess();
    return egl::Display::GetNativeClientBuffer(buffer);
}

EGLDisplay GetPlatformDisplayEXT(Thread *thread,
                                 EGLenum platform,
                                 void *native_display,
                                 const AttributeMap &attribMap)
{
    switch (platform)
    {
        case EGL_PLATFORM_ANGLE_ANGLE:
        case EGL_PLATFORM_GBM_KHR:
        case EGL_PLATFORM_WAYLAND_EXT:
        {
            return egl::Display::GetDisplayFromNativeDisplay(
                platform, gl::bitCast<EGLNativeDisplayType>(native_display), attribMap);
        }
        case EGL_PLATFORM_DEVICE_EXT:
        {
            Device *eglDevice = static_cast<Device *>(native_display);
            return egl::Display::GetDisplayFromDevice(eglDevice, attribMap);
        }
        default:
        {
            UNREACHABLE();
            return EGL_NO_DISPLAY;
        }
    }
}

EGLBoolean GetSyncAttribKHR(Thread *thread,
                            Display *display,
                            Sync *syncObject,
                            EGLint attribute,
                            EGLint *value)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglGetSyncAttrib",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, GetSyncAttrib(display, syncObject, attribute, value),
                         "eglGetSyncAttrib", GetSyncIfValid(display, syncObject), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLint LabelObjectKHR(Thread *thread,
                      Display *display,
                      ObjectType objectTypePacked,
                      EGLObjectKHR object,
                      EGLLabelKHR label)
{
    LabeledObject *labeledObject =
        GetLabeledObjectIfValid(thread, display, objectTypePacked, object);
    ASSERT(labeledObject != nullptr);
    labeledObject->setLabel(label);

    thread->setSuccess();
    return EGL_SUCCESS;
}

EGLBoolean PostSubBufferNV(Thread *thread,
                           Display *display,
                           Surface *eglSurface,
                           EGLint x,
                           EGLint y,
                           EGLint width,
                           EGLint height)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglPostSubBufferNV",
                         GetDisplayIfValid(display), EGL_FALSE);
    Error error = eglSurface->postSubBuffer(thread->getContext(), x, y, width, height);
    if (error.isError())
    {
        thread->setError(error, "eglPostSubBufferNV", GetSurfaceIfValid(display, eglSurface));
        return EGL_FALSE;
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean PresentationTimeANDROID(Thread *thread,
                                   Display *display,
                                   Surface *eglSurface,
                                   EGLnsecsANDROID time)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglPresentationTimeANDROID",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, eglSurface->setPresentationTime(time),
                         "eglPresentationTimeANDROID", GetSurfaceIfValid(display, eglSurface),
                         EGL_FALSE);

    return EGL_TRUE;
}

EGLBoolean GetCompositorTimingSupportedANDROID(Thread *thread,
                                               Display *display,
                                               Surface *eglSurface,
                                               CompositorTiming nameInternal)
{
    thread->setSuccess();
    return eglSurface->getSupportedCompositorTimings().test(nameInternal);
}

EGLBoolean GetCompositorTimingANDROID(Thread *thread,
                                      Display *display,
                                      Surface *eglSurface,
                                      EGLint numTimestamps,
                                      const EGLint *names,
                                      EGLnsecsANDROID *values)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglGetCompositorTimingANDROIDD",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, eglSurface->getCompositorTiming(numTimestamps, names, values),
                         "eglGetCompositorTimingANDROIDD", GetSurfaceIfValid(display, eglSurface),
                         EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean GetNextFrameIdANDROID(Thread *thread,
                                 Display *display,
                                 Surface *eglSurface,
                                 EGLuint64KHR *frameId)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglGetNextFrameIdANDROID",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, eglSurface->getNextFrameId(frameId), "eglGetNextFrameIdANDROID",
                         GetSurfaceIfValid(display, eglSurface), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean GetFrameTimestampSupportedANDROID(Thread *thread,
                                             Display *display,
                                             Surface *eglSurface,
                                             Timestamp timestampInternal)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglQueryTimestampSupportedANDROID",
                         GetDisplayIfValid(display), EGL_FALSE);
    thread->setSuccess();
    return eglSurface->getSupportedTimestamps().test(timestampInternal);
}

EGLBoolean GetFrameTimestampsANDROID(Thread *thread,
                                     Display *display,
                                     Surface *eglSurface,
                                     EGLuint64KHR frameId,
                                     EGLint numTimestamps,
                                     const EGLint *timestamps,
                                     EGLnsecsANDROID *values)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglGetFrameTimestampsANDROID",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(
        thread, eglSurface->getFrameTimestamps(frameId, numTimestamps, timestamps, values),
        "eglGetFrameTimestampsANDROID", GetSurfaceIfValid(display, eglSurface), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean QueryDebugKHR(Thread *thread, EGLint attribute, EGLAttrib *value)
{
    Debug *debug = GetDebug();
    switch (attribute)
    {
        case EGL_DEBUG_MSG_CRITICAL_KHR:
        case EGL_DEBUG_MSG_ERROR_KHR:
        case EGL_DEBUG_MSG_WARN_KHR:
        case EGL_DEBUG_MSG_INFO_KHR:
            *value = debug->isMessageTypeEnabled(FromEGLenum<MessageType>(attribute)) ? EGL_TRUE
                                                                                      : EGL_FALSE;
            break;
        case EGL_DEBUG_CALLBACK_KHR:
            *value = reinterpret_cast<EGLAttrib>(debug->getCallback());
            break;

        default:
            UNREACHABLE();
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean QueryDeviceAttribEXT(Thread *thread, Device *dev, EGLint attribute, EGLAttrib *value)
{
    ANGLE_EGL_TRY_RETURN(thread, dev->getAttribute(attribute, value), "eglQueryDeviceAttribEXT",
                         GetDeviceIfValid(dev), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

const char *QueryDeviceStringEXT(Thread *thread, Device *dev, EGLint name)
{
    egl::Display *owningDisplay = dev->getOwningDisplay();
    ANGLE_EGL_TRY_RETURN(thread, owningDisplay->prepareForCall(), "eglQueryDeviceStringEXT",
                         GetDisplayIfValid(owningDisplay), EGL_FALSE);
    const char *result;
    switch (name)
    {
        case EGL_EXTENSIONS:
            result = dev->getExtensionString().c_str();
            break;
        default:
            thread->setError(EglBadDevice(), "eglQueryDeviceStringEXT", GetDeviceIfValid(dev));
            return nullptr;
    }

    thread->setSuccess();
    return result;
}

EGLBoolean QueryDisplayAttribEXT(Thread *thread,
                                 Display *display,
                                 EGLint attribute,
                                 EGLAttrib *value)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglQueryDisplayAttribEXT",
                         GetDisplayIfValid(display), EGL_FALSE);
    *value = display->queryAttrib(attribute);
    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean QueryStreamKHR(Thread *thread,
                          Display *display,
                          Stream *streamObject,
                          EGLenum attribute,
                          EGLint *value)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglQueryStreamKHR",
                         GetDisplayIfValid(display), EGL_FALSE);
    switch (attribute)
    {
        case EGL_STREAM_STATE_KHR:
            *value = streamObject->getState();
            break;
        case EGL_CONSUMER_LATENCY_USEC_KHR:
            *value = streamObject->getConsumerLatency();
            break;
        case EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR:
            *value = streamObject->getConsumerAcquireTimeout();
            break;
        default:
            UNREACHABLE();
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean QueryStreamu64KHR(Thread *thread,
                             Display *display,
                             Stream *streamObject,
                             EGLenum attribute,
                             EGLuint64KHR *value)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglQueryStreamu64KHR",
                         GetDisplayIfValid(display), EGL_FALSE);
    switch (attribute)
    {
        case EGL_PRODUCER_FRAME_KHR:
            *value = streamObject->getProducerFrame();
            break;
        case EGL_CONSUMER_FRAME_KHR:
            *value = streamObject->getConsumerFrame();
            break;
        default:
            UNREACHABLE();
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean QuerySurfacePointerANGLE(Thread *thread,
                                    Display *display,
                                    Surface *eglSurface,
                                    EGLint attribute,
                                    void **value)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglQuerySurfacePointerANGLE",
                         GetDisplayIfValid(display), EGL_FALSE);
    Error error = eglSurface->querySurfacePointerANGLE(attribute, value);
    if (error.isError())
    {
        thread->setError(error, "eglQuerySurfacePointerANGLE",
                         GetSurfaceIfValid(display, eglSurface));
        return EGL_FALSE;
    }

    thread->setSuccess();
    return EGL_TRUE;
}

void SetBlobCacheFuncsANDROID(Thread *thread,
                              Display *display,
                              EGLSetBlobFuncANDROID set,
                              EGLGetBlobFuncANDROID get)
{
    ANGLE_EGL_TRY(thread, display->prepareForCall(), "eglSetBlobCacheFuncsANDROID",
                  GetDisplayIfValid(display));
    thread->setSuccess();
    display->setBlobCacheFuncs(set, get);
}

EGLBoolean SignalSyncKHR(Thread *thread, Display *display, Sync *syncObject, EGLenum mode)
{
    gl::Context *currentContext = thread->getContext();
    ANGLE_EGL_TRY_RETURN(thread, syncObject->signal(display, currentContext, mode),
                         "eglSignalSyncKHR", GetSyncIfValid(display, syncObject), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean StreamAttribKHR(Thread *thread,
                           Display *display,
                           Stream *streamObject,
                           EGLenum attribute,
                           EGLint value)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglStreamAttribKHR",
                         GetDisplayIfValid(display), EGL_FALSE);
    switch (attribute)
    {
        case EGL_CONSUMER_LATENCY_USEC_KHR:
            streamObject->setConsumerLatency(value);
            break;
        case EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR:
            streamObject->setConsumerAcquireTimeout(value);
            break;
        default:
            UNREACHABLE();
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean StreamConsumerAcquireKHR(Thread *thread, Display *display, Stream *streamObject)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglStreamConsumerAcquireKHR",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, streamObject->consumerAcquire(thread->getContext()),
                         "eglStreamConsumerAcquireKHR", GetStreamIfValid(display, streamObject),
                         EGL_FALSE);
    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean StreamConsumerGLTextureExternalKHR(Thread *thread,
                                              Display *display,
                                              Stream *streamObject)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglStreamConsumerGLTextureExternalKHR",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(
        thread, streamObject->createConsumerGLTextureExternal(AttributeMap(), thread->getContext()),
        "eglStreamConsumerGLTextureExternalKHR", GetStreamIfValid(display, streamObject),
        EGL_FALSE);
    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean StreamConsumerGLTextureExternalAttribsNV(Thread *thread,
                                                    Display *display,
                                                    Stream *streamObject,
                                                    const AttributeMap &attributes)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(),
                         "eglStreamConsumerGLTextureExternalAttribsNV", GetDisplayIfValid(display),
                         EGL_FALSE);

    gl::Context *context = gl::GetValidGlobalContext();
    ANGLE_EGL_TRY_RETURN(thread, streamObject->createConsumerGLTextureExternal(attributes, context),
                         "eglStreamConsumerGLTextureExternalAttribsNV",
                         GetStreamIfValid(display, streamObject), EGL_FALSE);
    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean StreamConsumerReleaseKHR(Thread *thread, Display *display, Stream *streamObject)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglStreamConsumerReleaseKHR",
                         GetDisplayIfValid(display), EGL_FALSE);

    gl::Context *context = gl::GetValidGlobalContext();
    ANGLE_EGL_TRY_RETURN(thread, streamObject->consumerRelease(context),
                         "eglStreamConsumerReleaseKHR", GetStreamIfValid(display, streamObject),
                         EGL_FALSE);
    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean SwapBuffersWithDamageKHR(Thread *thread,
                                    Display *display,
                                    Surface *eglSurface,
                                    const EGLint *rects,
                                    EGLint n_rects)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglSwapBuffersWithDamageEXT",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, eglSurface->swapWithDamage(thread->getContext(), rects, n_rects),
                         "eglSwapBuffersWithDamageEXT", GetSurfaceIfValid(display, eglSurface),
                         EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean PrepareSwapBuffersANGLE(EGLDisplay dpy, EGLSurface surface)

{
    ANGLE_SCOPED_GLOBAL_SURFACE_LOCK();

    egl::Display *dpyPacked = PackParam<egl::Display *>(dpy);
    Surface *surfacePacked  = PackParam<Surface *>(surface);
    Thread *thread          = egl::GetCurrentThread();
    {
        ANGLE_SCOPED_GLOBAL_LOCK();

        EGL_EVENT(PrepareSwapBuffersANGLE, "dpy = 0x%016" PRIxPTR ", surface = 0x%016" PRIxPTR "",
                  (uintptr_t)dpy, (uintptr_t)surface);

        ANGLE_EGL_VALIDATE(thread, PrepareSwapBuffersANGLE, GetDisplayIfValid(dpyPacked),
                           EGLBoolean, dpyPacked, surfacePacked);

        ANGLE_EGL_TRY_RETURN(thread, dpyPacked->prepareForCall(), "eglPrepareSwapBuffersANGLE",
                             GetDisplayIfValid(dpyPacked), EGL_FALSE);
    }
    ANGLE_EGL_TRY_RETURN(thread, surfacePacked->prepareSwap(thread->getContext()), "prepareSwap",
                         GetSurfaceIfValid(dpyPacked, surfacePacked), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLint WaitSyncKHR(Thread *thread, Display *display, Sync *syncObject, EGLint flags)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglWaitSync",
                         GetDisplayIfValid(display), EGL_FALSE);
    gl::Context *currentContext = thread->getContext();
    ANGLE_EGL_TRY_RETURN(thread, syncObject->serverWait(display, currentContext, flags),
                         "eglWaitSync", GetSyncIfValid(display, syncObject), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLDeviceEXT CreateDeviceANGLE(Thread *thread,
                               EGLint device_type,
                               void *native_device,
                               const EGLAttrib *attrib_list)
{
    Device *device = nullptr;
    ANGLE_EGL_TRY_RETURN(thread, Device::CreateDevice(device_type, native_device, &device),
                         "eglCreateDeviceANGLE", GetThreadIfValid(thread), EGL_NO_DEVICE_EXT);

    thread->setSuccess();
    return device;
}

EGLBoolean ReleaseDeviceANGLE(Thread *thread, Device *dev)
{
    SafeDelete(dev);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean CreateStreamProducerD3DTextureANGLE(Thread *thread,
                                               Display *display,
                                               Stream *streamObject,
                                               const AttributeMap &attributes)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(),
                         "eglCreateStreamProducerD3DTextureANGLE", GetDisplayIfValid(display),
                         EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, streamObject->createProducerD3D11Texture(attributes),
                         "eglCreateStreamProducerD3DTextureANGLE",
                         GetStreamIfValid(display, streamObject), EGL_FALSE);
    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean StreamPostD3DTextureANGLE(Thread *thread,
                                     Display *display,
                                     Stream *streamObject,
                                     void *texture,
                                     const AttributeMap &attributes)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglStreamPostD3DTextureANGLE",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, streamObject->postD3D11Texture(texture, attributes),
                         "eglStreamPostD3DTextureANGLE", GetStreamIfValid(display, streamObject),
                         EGL_FALSE);
    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean GetMscRateANGLE(Thread *thread,
                           Display *display,
                           Surface *eglSurface,
                           EGLint *numerator,
                           EGLint *denominator)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglGetMscRateANGLE",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, eglSurface->getMscRate(numerator, denominator),
                         "eglGetMscRateANGLE", GetSurfaceIfValid(display, eglSurface), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean GetSyncValuesCHROMIUM(Thread *thread,
                                 Display *display,
                                 Surface *eglSurface,
                                 EGLuint64KHR *ust,
                                 EGLuint64KHR *msc,
                                 EGLuint64KHR *sbc)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglGetSyncValuesCHROMIUM",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, eglSurface->getSyncValues(ust, msc, sbc),
                         "eglGetSyncValuesCHROMIUM", GetSurfaceIfValid(display, eglSurface),
                         EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLint ProgramCacheGetAttribANGLE(Thread *thread, Display *display, EGLenum attrib)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglProgramCacheGetAttribANGLE",
                         GetDisplayIfValid(display), 0);
    thread->setSuccess();
    return display->programCacheGetAttrib(attrib);
}

void ProgramCacheQueryANGLE(Thread *thread,
                            Display *display,
                            EGLint index,
                            void *key,
                            EGLint *keysize,
                            void *binary,
                            EGLint *binarysize)
{
    ANGLE_EGL_TRY(thread, display->prepareForCall(), "eglProgramCacheQueryANGLE",
                  GetDisplayIfValid(display));
    ANGLE_EGL_TRY(thread, display->programCacheQuery(index, key, keysize, binary, binarysize),
                  "eglProgramCacheQueryANGLE", GetDisplayIfValid(display));

    thread->setSuccess();
}

void ProgramCachePopulateANGLE(Thread *thread,
                               Display *display,
                               const void *key,
                               EGLint keysize,
                               const void *binary,
                               EGLint binarysize)
{
    ANGLE_EGL_TRY(thread, display->prepareForCall(), "eglProgramCachePopulateANGLE",
                  GetDisplayIfValid(display));
    ANGLE_EGL_TRY(thread, display->programCachePopulate(key, keysize, binary, binarysize),
                  "eglProgramCachePopulateANGLE", GetDisplayIfValid(display));

    thread->setSuccess();
}

EGLint ProgramCacheResizeANGLE(Thread *thread, Display *display, EGLint limit, EGLint mode)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglProgramCacheResizeANGLE",
                         GetDisplayIfValid(display), 0);
    thread->setSuccess();
    return display->programCacheResize(limit, mode);
}

const char *QueryStringiANGLE(Thread *thread, Display *display, EGLint name, EGLint index)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglQueryStringiANGLE",
                         GetDisplayIfValid(display), nullptr);
    thread->setSuccess();
    return display->queryStringi(name, index);
}

EGLBoolean SwapBuffersWithFrameTokenANGLE(Thread *thread,
                                          Display *display,
                                          Surface *eglSurface,
                                          EGLFrameTokenANGLE frametoken)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglSwapBuffersWithFrameTokenANGLE",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, eglSurface->swapWithFrameToken(thread->getContext(), frametoken),
                         "eglSwapBuffersWithFrameTokenANGLE", GetDisplayIfValid(display),
                         EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

void ReleaseHighPowerGPUANGLE(Thread *thread, Display *display, gl::Context *context)
{
    ANGLE_EGL_TRY(thread, display->prepareForCall(), "eglReleaseHighPowerGPUANGLE",
                  GetDisplayIfValid(display));
    ANGLE_EGL_TRY(thread, context->releaseHighPowerGPU(), "eglReleaseHighPowerGPUANGLE",
                  GetDisplayIfValid(display));

    thread->setSuccess();
}

void ReacquireHighPowerGPUANGLE(Thread *thread, Display *display, gl::Context *context)
{
    ANGLE_EGL_TRY(thread, display->prepareForCall(), "eglReacquireHighPowerGPUANGLE",
                  GetDisplayIfValid(display));
    ANGLE_EGL_TRY(thread, context->reacquireHighPowerGPU(), "eglReacquireHighPowerGPUANGLE",
                  GetDisplayIfValid(display));

    thread->setSuccess();
}

void HandleGPUSwitchANGLE(Thread *thread, Display *display)
{
    ANGLE_EGL_TRY(thread, display->prepareForCall(), "eglHandleGPUSwitchANGLE",
                  GetDisplayIfValid(display));
    ANGLE_EGL_TRY(thread, display->handleGPUSwitch(), "eglHandleGPUSwitchANGLE",
                  GetDisplayIfValid(display));

    thread->setSuccess();
}

void ForceGPUSwitchANGLE(Thread *thread, Display *display, EGLint gpuIDHigh, EGLint gpuIDLow)
{
    ANGLE_EGL_TRY(thread, display->prepareForCall(), "eglForceGPUSwitchANGLE",
                  GetDisplayIfValid(display));
    ANGLE_EGL_TRY(thread, display->forceGPUSwitch(gpuIDHigh, gpuIDLow), "eglForceGPUSwitchANGLE",
                  GetDisplayIfValid(display));

    thread->setSuccess();
}

EGLBoolean QueryDisplayAttribANGLE(Thread *thread,
                                   Display *display,
                                   EGLint attribute,
                                   EGLAttrib *value)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglQueryDisplayAttribEXT",
                         GetDisplayIfValid(display), EGL_FALSE);
    *value = display->queryAttrib(attribute);
    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean LockSurfaceKHR(Thread *thread,
                          egl::Display *display,
                          Surface *surface,
                          const AttributeMap &attributes)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglLockSurfaceKHR",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, surface->lockSurfaceKHR(display, attributes), "eglLockSurfaceKHR",
                         GetSurfaceIfValid(display, surface), EGL_FALSE);
    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean UnlockSurfaceKHR(Thread *thread, egl::Display *display, Surface *surface)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglUnlockSurfaceKHR",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, surface->unlockSurfaceKHR(display), "eglQuerySurface64KHR",
                         GetSurfaceIfValid(display, surface), EGL_FALSE);
    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean QuerySurface64KHR(Thread *thread,
                             egl::Display *display,
                             Surface *surface,
                             EGLint attribute,
                             EGLAttribKHR *value)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglQuerySurface64KHR",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(
        thread, QuerySurfaceAttrib64KHR(display, thread->getContext(), surface, attribute, value),
        "eglQuerySurface64KHR", GetSurfaceIfValid(display, surface), EGL_FALSE);
    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean ExportVkImageANGLE(Thread *thread,
                              egl::Display *display,
                              Image *image,
                              void *vk_image,
                              void *vk_image_create_info)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglExportVkImageANGLE",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, image->exportVkImage(vk_image, vk_image_create_info),
                         "eglExportVkImageANGLE", GetImageIfValid(display, image), EGL_FALSE);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean SetDamageRegionKHR(Thread *thread,
                              egl::Display *display,
                              egl::Surface *surface,
                              EGLint *rects,
                              EGLint n_rects)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglSetDamageRegionKHR",
                         GetDisplayIfValid(display), EGL_FALSE);
    surface->setDamageRegion(rects, n_rects);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean QueryDmaBufFormatsEXT(Thread *thread,
                                 egl::Display *display,
                                 EGLint max_formats,
                                 EGLint *formats,
                                 EGLint *num_formats)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglQueryDmaBufFormatsEXT",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, display->queryDmaBufFormats(max_formats, formats, num_formats),
                         "eglQueryDmaBufFormatsEXT", GetDisplayIfValid(display), EGL_FALSE);
    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean QueryDmaBufModifiersEXT(Thread *thread,
                                   egl::Display *display,
                                   EGLint format,
                                   EGLint max_modifiers,
                                   EGLuint64KHR *modifiers,
                                   EGLBoolean *external_only,
                                   EGLint *num_modifiers)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglQueryDmaBufModifiersEXT",
                         GetDisplayIfValid(display), EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread,
                         display->queryDmaBufModifiers(format, max_modifiers, modifiers,
                                                       external_only, num_modifiers),
                         "eglQueryDmaBufModifiersEXT", GetDisplayIfValid(display), EGL_FALSE);
    thread->setSuccess();
    return EGL_TRUE;
}

void *CopyMetalSharedEventANGLE(Thread *thread, Display *display, Sync *syncObject)
{
    ANGLE_EGL_TRY_RETURN(thread, display->prepareForCall(), "eglCopyMetalSharedEventANGLE",
                         GetDisplayIfValid(display), nullptr);
    void *result = nullptr;
    ANGLE_EGL_TRY_RETURN(thread, syncObject->copyMetalSharedEventANGLE(display, &result),
                         "eglCopyMetalSharedEventANGLE", GetSyncIfValid(display, syncObject),
                         nullptr);

    thread->setSuccess();
    return result;
}

}  // namespace egl
