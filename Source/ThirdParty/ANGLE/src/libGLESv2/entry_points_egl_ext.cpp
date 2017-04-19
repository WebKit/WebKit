//
// Copyright(c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// entry_points_ext.cpp : Implements the EGL extension entry points.

#include "libGLESv2/entry_points_egl_ext.h"
#include "libGLESv2/global_state.h"

#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Device.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Stream.h"
#include "libANGLE/Thread.h"
#include "libANGLE/validationEGL.h"

#include "common/debug.h"

namespace egl
{

// EGL_ANGLE_query_surface_pointer
EGLBoolean EGLAPIENTRY QuerySurfacePointerANGLE(EGLDisplay dpy, EGLSurface surface, EGLint attribute, void **value)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLint attribute = %d, void **value = 0x%0.8p)",
          dpy, surface, attribute, value);
    Thread *thread = GetCurrentThread();

    Display *display = static_cast<Display*>(dpy);
    Surface *eglSurface = static_cast<Surface*>(surface);

    Error error = ValidateSurface(display, eglSurface);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    if (!display->getExtensions().querySurfacePointer)
    {
        thread->setError(Error(EGL_SUCCESS));
        return EGL_FALSE;
    }

    if (surface == EGL_NO_SURFACE)
    {
        thread->setError(Error(EGL_BAD_SURFACE));
        return EGL_FALSE;
    }

    // validate the attribute parameter
    switch (attribute)
    {
      case EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE:
        if (!display->getExtensions().surfaceD3DTexture2DShareHandle)
        {
            thread->setError(Error(EGL_BAD_ATTRIBUTE));
            return EGL_FALSE;
        }
        break;
      case EGL_DXGI_KEYED_MUTEX_ANGLE:
        if (!display->getExtensions().keyedMutex)
        {
            thread->setError(Error(EGL_BAD_ATTRIBUTE));
            return EGL_FALSE;
        }
        break;
      default:
          thread->setError(Error(EGL_BAD_ATTRIBUTE));
          return EGL_FALSE;
    }

    error = eglSurface->querySurfacePointerANGLE(attribute, value);
    thread->setError(error);
    return (error.isError() ? EGL_FALSE : EGL_TRUE);
}


// EGL_NV_post_sub_buffer
EGLBoolean EGLAPIENTRY PostSubBufferNV(EGLDisplay dpy, EGLSurface surface, EGLint x, EGLint y, EGLint width, EGLint height)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLint x = %d, EGLint y = %d, EGLint width = %d, EGLint height = %d)", dpy, surface, x, y, width, height);
    Thread *thread = GetCurrentThread();

    if (x < 0 || y < 0 || width < 0 || height < 0)
    {
        thread->setError(Error(EGL_BAD_PARAMETER));
        return EGL_FALSE;
    }

    Display *display = static_cast<Display*>(dpy);
    Surface *eglSurface = static_cast<Surface*>(surface);

    Error error = ValidateSurface(display, eglSurface);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    if (display->testDeviceLost())
    {
        thread->setError(Error(EGL_CONTEXT_LOST));
        return EGL_FALSE;
    }

    if (surface == EGL_NO_SURFACE)
    {
        thread->setError(Error(EGL_BAD_SURFACE));
        return EGL_FALSE;
    }

    if (!display->getExtensions().postSubBuffer)
    {
        // Spec is not clear about how this should be handled.
        thread->setError(Error(EGL_SUCCESS));
        return EGL_TRUE;
    }

    error = eglSurface->postSubBuffer(x, y, width, height);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    thread->setError(Error(EGL_SUCCESS));
    return EGL_TRUE;
}

// EGL_EXT_platform_base
EGLDisplay EGLAPIENTRY GetPlatformDisplayEXT(EGLenum platform, void *native_display, const EGLint *attrib_list)
{
    EVENT("(EGLenum platform = %d, void* native_display = 0x%0.8p, const EGLint* attrib_list = 0x%0.8p)",
          platform, native_display, attrib_list);
    Thread *thread = GetCurrentThread();

    const ClientExtensions &clientExtensions = Display::getClientExtensions();

    switch (platform)
    {
      case EGL_PLATFORM_ANGLE_ANGLE:
        if (!clientExtensions.platformANGLE)
        {
            thread->setError(Error(EGL_BAD_PARAMETER));
            return EGL_NO_DISPLAY;
        }
        break;
      case EGL_PLATFORM_DEVICE_EXT:
          if (!clientExtensions.platformDevice)
          {
              thread->setError(Error(EGL_BAD_PARAMETER, "Platform Device extension is not active"));
              return EGL_NO_DISPLAY;
          }
          break;
      default:
          thread->setError(Error(EGL_BAD_CONFIG));
          return EGL_NO_DISPLAY;
    }

    if (platform == EGL_PLATFORM_ANGLE_ANGLE)
    {
        EGLint platformType          = EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE;
        EGLint deviceType            = EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE;
        bool enableAutoTrimSpecified = false;
        bool deviceTypeSpecified     = false;
        bool presentPathSpecified    = false;

        Optional<EGLint> majorVersion;
        Optional<EGLint> minorVersion;

        if (attrib_list)
        {
            for (const EGLint *curAttrib = attrib_list; curAttrib[0] != EGL_NONE; curAttrib += 2)
            {
                switch (curAttrib[0])
                {
                    case EGL_PLATFORM_ANGLE_TYPE_ANGLE:
                    {
                        egl::Error error = ValidatePlatformType(clientExtensions, curAttrib[1]);
                        if (error.isError())
                        {
                            thread->setError(error);
                            return EGL_NO_DISPLAY;
                        }
                        platformType = curAttrib[1];
                        break;
                    }

                    case EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE:
                        if (curAttrib[1] != EGL_DONT_CARE)
                        {
                            majorVersion = curAttrib[1];
                        }
                        break;

                    case EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE:
                        if (curAttrib[1] != EGL_DONT_CARE)
                        {
                            minorVersion = curAttrib[1];
                        }
                        break;

                    case EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE:
                        switch (curAttrib[1])
                    {
                        case EGL_TRUE:
                        case EGL_FALSE:
                            break;
                        default:
                            thread->setError(Error(EGL_BAD_ATTRIBUTE));
                            return EGL_NO_DISPLAY;
                    }
                    enableAutoTrimSpecified = true;
                    break;

                    case EGL_EXPERIMENTAL_PRESENT_PATH_ANGLE:
                        if (!clientExtensions.experimentalPresentPath)
                        {
                            thread->setError(
                                Error(EGL_BAD_ATTRIBUTE,
                                      "EGL_ANGLE_experimental_present_path extension not active"));
                            return EGL_NO_DISPLAY;
                        }

                        switch (curAttrib[1])
                        {
                            case EGL_EXPERIMENTAL_PRESENT_PATH_FAST_ANGLE:
                            case EGL_EXPERIMENTAL_PRESENT_PATH_COPY_ANGLE:
                                break;
                            default:
                                thread->setError(
                                    Error(EGL_BAD_ATTRIBUTE,
                                          "Invalid value for EGL_EXPERIMENTAL_PRESENT_PATH_ANGLE"));
                                return EGL_NO_DISPLAY;
                        }
                        presentPathSpecified = true;
                        break;

                    case EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE:
                        switch (curAttrib[1])
                        {
                            case EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE:
                            case EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE:
                            case EGL_PLATFORM_ANGLE_DEVICE_TYPE_REFERENCE_ANGLE:
                                deviceTypeSpecified = true;
                                break;

                            case EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE:
                                // This is a hidden option, accepted by the OpenGL back-end.
                                break;

                            default:
                                thread->setError(Error(EGL_BAD_ATTRIBUTE,
                                                       "Invalid value for "
                                                       "EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE "
                                                       "attrib"));
                                return EGL_NO_DISPLAY;
                        }
                        deviceType = curAttrib[1];
                    break;

                    case EGL_PLATFORM_ANGLE_ENABLE_VALIDATION_LAYER_ANGLE:
                        if (!clientExtensions.platformANGLEVulkan)
                        {
                            thread->setError(
                                Error(EGL_BAD_ATTRIBUTE,
                                      "EGL_ANGLE_platform_angle_vulkan extension not active"));
                            return EGL_NO_DISPLAY;
                        }
                        if (platformType != EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE)
                        {
                            thread->setError(
                                Error(EGL_BAD_ATTRIBUTE,
                                      "Validation can only be enabled for the Vulkan back-end."));
                            return EGL_NO_DISPLAY;
                        }
                        if (curAttrib[1] != EGL_TRUE && curAttrib[1] != EGL_FALSE)
                        {
                            thread->setError(
                                Error(EGL_BAD_ATTRIBUTE,
                                      "Validation layer attribute must be EGL_TRUE or EGL_FALSE."));
                            return EGL_NO_DISPLAY;
                        }
                        break;

                    default:
                        break;
                }
            }
        }

        if (!majorVersion.valid() && minorVersion.valid())
        {
            thread->setError(Error(EGL_BAD_ATTRIBUTE));
            return EGL_NO_DISPLAY;
        }

        if (deviceType == EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE &&
            platformType != EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
        {
            thread->setError(
                Error(EGL_BAD_ATTRIBUTE,
                      "EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE requires a device type of "
                      "EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE."));
            return EGL_NO_DISPLAY;
        }

        if (enableAutoTrimSpecified && platformType != EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
        {
            thread->setError(
                Error(EGL_BAD_ATTRIBUTE,
                      "EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE requires a device type of "
                      "EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE."));
            return EGL_NO_DISPLAY;
        }

        if (presentPathSpecified && platformType != EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
        {
            thread->setError(Error(EGL_BAD_ATTRIBUTE,
                                   "EGL_EXPERIMENTAL_PRESENT_PATH_ANGLE requires a device type of "
                                   "EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE."));
            return EGL_NO_DISPLAY;
        }

        if (deviceTypeSpecified && platformType != EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE &&
            platformType != EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
        {
            thread->setError(
                Error(EGL_BAD_ATTRIBUTE,
                      "EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE requires a device type of "
                      "EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE or EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE."));
            return EGL_NO_DISPLAY;
        }

        if (platformType == EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE)
        {
            if ((majorVersion.valid() && majorVersion.value() != 1) ||
                (minorVersion.valid() && minorVersion.value() != 0))
            {
                thread->setError(Error(
                    EGL_BAD_ATTRIBUTE,
                    "EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE currently only supports Vulkan 1.0."));
                return EGL_NO_DISPLAY;
            }
        }

        thread->setError(Error(EGL_SUCCESS));
        return Display::GetDisplayFromNativeDisplay(
            gl::bitCast<EGLNativeDisplayType>(native_display),
            AttributeMap::CreateFromIntArray(attrib_list));
    }
    else if (platform == EGL_PLATFORM_DEVICE_EXT)
    {
        Device *eglDevice = reinterpret_cast<Device *>(native_display);
        if (eglDevice == nullptr || !Device::IsValidDevice(eglDevice))
        {
            thread->setError(Error(EGL_BAD_ATTRIBUTE,
                                   "native_display should be a valid EGL device if platform equals "
                                   "EGL_PLATFORM_DEVICE_EXT"));
            return EGL_NO_DISPLAY;
        }

        thread->setError(Error(EGL_SUCCESS));
        return Display::GetDisplayFromDevice(eglDevice);
    }
    else
    {
        UNREACHABLE();
        return EGL_NO_DISPLAY;
    }
}

// EGL_EXT_device_query
EGLBoolean EGLAPIENTRY QueryDeviceAttribEXT(EGLDeviceEXT device, EGLint attribute, EGLAttrib *value)
{
    EVENT("(EGLDeviceEXT device = 0x%0.8p, EGLint attribute = %d, EGLAttrib *value = 0x%0.8p)",
          device, attribute, value);
    Thread *thread = GetCurrentThread();

    Device *dev = static_cast<Device*>(device);
    if (dev == EGL_NO_DEVICE_EXT || !Device::IsValidDevice(dev))
    {
        thread->setError(Error(EGL_BAD_ACCESS));
        return EGL_FALSE;
    }

    // If the device was created by (and is owned by) a display, and that display doesn't support
    // device querying, then this call should fail
    Display *owningDisplay = dev->getOwningDisplay();
    if (owningDisplay != nullptr && !owningDisplay->getExtensions().deviceQuery)
    {
        thread->setError(Error(EGL_BAD_ACCESS,
                               "Device wasn't created using eglCreateDeviceANGLE, and the Display "
                               "that created it doesn't support device querying"));
        return EGL_FALSE;
    }

    Error error(EGL_SUCCESS);

    // validate the attribute parameter
    switch (attribute)
    {
      case EGL_D3D11_DEVICE_ANGLE:
      case EGL_D3D9_DEVICE_ANGLE:
        if (!dev->getExtensions().deviceD3D || dev->getType() != attribute)
        {
            thread->setError(Error(EGL_BAD_ATTRIBUTE));
            return EGL_FALSE;
        }
        error = dev->getDevice(value);
        break;
      default:
          thread->setError(Error(EGL_BAD_ATTRIBUTE));
          return EGL_FALSE;
    }

    thread->setError(error);
    return (error.isError() ? EGL_FALSE : EGL_TRUE);
}

// EGL_EXT_device_query
const char * EGLAPIENTRY QueryDeviceStringEXT(EGLDeviceEXT device, EGLint name)
{
    EVENT("(EGLDeviceEXT device = 0x%0.8p, EGLint name = %d)",
          device, name);
    Thread *thread = GetCurrentThread();

    Device *dev = static_cast<Device*>(device);
    if (dev == EGL_NO_DEVICE_EXT || !Device::IsValidDevice(dev))
    {
        thread->setError(Error(EGL_BAD_DEVICE_EXT));
        return nullptr;
    }

    const char *result;
    switch (name)
    {
      case EGL_EXTENSIONS:
        result = dev->getExtensionString().c_str();
        break;
      default:
          thread->setError(Error(EGL_BAD_DEVICE_EXT));
          return nullptr;
    }

    thread->setError(Error(EGL_SUCCESS));
    return result;
}

// EGL_EXT_device_query
EGLBoolean EGLAPIENTRY QueryDisplayAttribEXT(EGLDisplay dpy, EGLint attribute, EGLAttrib *value)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLint attribute = %d, EGLAttrib *value = 0x%0.8p)",
          dpy, attribute, value);
    Thread *thread = GetCurrentThread();

    Display *display = static_cast<Display*>(dpy);

    Error error = ValidateDisplay(display);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    if (!display->getExtensions().deviceQuery)
    {
        thread->setError(Error(EGL_BAD_ACCESS));
        return EGL_FALSE;
    }

    // validate the attribute parameter
    switch (attribute)
    {
      case EGL_DEVICE_EXT:
        *value = reinterpret_cast<EGLAttrib>(display->getDevice());
        break;

      default:
          thread->setError(Error(EGL_BAD_ATTRIBUTE));
          return EGL_FALSE;
    }

    thread->setError(error);
    return (error.isError() ? EGL_FALSE : EGL_TRUE);
}

ANGLE_EXPORT EGLImageKHR EGLAPIENTRY CreateImageKHR(EGLDisplay dpy,
                                                    EGLContext ctx,
                                                    EGLenum target,
                                                    EGLClientBuffer buffer,
                                                    const EGLint *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLContext ctx = 0x%0.8p, EGLenum target = 0x%X, "
        "EGLClientBuffer buffer = 0x%0.8p, const EGLAttrib *attrib_list = 0x%0.8p)",
        dpy, ctx, target, buffer, attrib_list);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    gl::Context *context = static_cast<gl::Context *>(ctx);
    AttributeMap attributes = AttributeMap::CreateFromIntArray(attrib_list);

    Error error = ValidateCreateImageKHR(display, context, target, buffer, attributes);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_NO_IMAGE;
    }

    Image *image = nullptr;
    error = display->createImage(context, target, buffer, attributes, &image);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_NO_IMAGE;
    }

    return static_cast<EGLImage>(image);
}

ANGLE_EXPORT EGLBoolean EGLAPIENTRY DestroyImageKHR(EGLDisplay dpy, EGLImageKHR image)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLImage image = 0x%0.8p)", dpy, image);
    Thread *thread = GetCurrentThread();

    Display *display = static_cast<Display *>(dpy);
    Image *img       = static_cast<Image *>(image);

    Error error = ValidateDestroyImageKHR(display, img);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    display->destroyImage(img);

    return EGL_TRUE;
}

ANGLE_EXPORT EGLDeviceEXT EGLAPIENTRY CreateDeviceANGLE(EGLint device_type,
                                                        void *native_device,
                                                        const EGLAttrib *attrib_list)
{
    EVENT(
        "(EGLint device_type = %d, void* native_device = 0x%0.8p, const EGLAttrib* attrib_list = "
        "0x%0.8p)",
        device_type, native_device, attrib_list);
    Thread *thread = GetCurrentThread();

    Error error = ValidateCreateDeviceANGLE(device_type, native_device, attrib_list);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_NO_DEVICE_EXT;
    }

    Device *device = nullptr;
    error = Device::CreateDevice(native_device, device_type, &device);
    if (error.isError())
    {
        ASSERT(device == nullptr);
        thread->setError(error);
        return EGL_NO_DEVICE_EXT;
    }

    return device;
}

ANGLE_EXPORT EGLBoolean EGLAPIENTRY ReleaseDeviceANGLE(EGLDeviceEXT device)
{
    EVENT("(EGLDeviceEXT device = 0x%0.8p)", device);
    Thread *thread = GetCurrentThread();

    Device *dev = static_cast<Device *>(device);

    Error error = ValidateReleaseDeviceANGLE(dev);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    SafeDelete(dev);

    return EGL_TRUE;
}

// EGL_KHR_stream
EGLStreamKHR EGLAPIENTRY CreateStreamKHR(EGLDisplay dpy, const EGLint *attrib_list)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, const EGLAttrib* attrib_list = 0x%0.8p)", dpy, attrib_list);
    Thread *thread = GetCurrentThread();

    Display *display = static_cast<Display *>(dpy);
    AttributeMap attributes = AttributeMap::CreateFromIntArray(attrib_list);

    Error error = ValidateCreateStreamKHR(display, attributes);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_NO_STREAM_KHR;
    }

    Stream *stream;
    error = display->createStream(attributes, &stream);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_NO_STREAM_KHR;
    }

    thread->setError(error);
    return static_cast<EGLStreamKHR>(stream);
}

EGLBoolean EGLAPIENTRY DestroyStreamKHR(EGLDisplay dpy, EGLStreamKHR stream)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR = 0x%0.8p)", dpy, stream);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    Stream *streamObject = static_cast<Stream *>(stream);

    Error error = ValidateDestroyStreamKHR(display, streamObject);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    display->destroyStream(streamObject);
    thread->setError(error);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY StreamAttribKHR(EGLDisplay dpy,
                                       EGLStreamKHR stream,
                                       EGLenum attribute,
                                       EGLint value)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR stream = 0x%0.8p, EGLenum attribute = 0x%X, "
        "EGLint value = 0x%X)",
        dpy, stream, attribute, value);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    Stream *streamObject = static_cast<Stream *>(stream);

    Error error = ValidateStreamAttribKHR(display, streamObject, attribute, value);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

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

    thread->setError(error);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY QueryStreamKHR(EGLDisplay dpy,
                                      EGLStreamKHR stream,
                                      EGLenum attribute,
                                      EGLint *value)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR stream = 0x%0.8p, EGLenum attribute = 0x%X, "
        "EGLint value = 0x%0.8p)",
        dpy, stream, attribute, value);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    Stream *streamObject = static_cast<Stream *>(stream);

    Error error = ValidateQueryStreamKHR(display, streamObject, attribute, value);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

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

    thread->setError(error);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY QueryStreamu64KHR(EGLDisplay dpy,
                                         EGLStreamKHR stream,
                                         EGLenum attribute,
                                         EGLuint64KHR *value)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR stream = 0x%0.8p, EGLenum attribute = 0x%X, "
        "EGLuint64KHR value = 0x%0.8p)",
        dpy, stream, attribute, value);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    Stream *streamObject = static_cast<Stream *>(stream);

    Error error = ValidateQueryStreamu64KHR(display, streamObject, attribute, value);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

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

    thread->setError(error);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY StreamConsumerGLTextureExternalKHR(EGLDisplay dpy, EGLStreamKHR stream)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR = 0x%0.8p)", dpy, stream);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    Stream *streamObject = static_cast<Stream *>(stream);
    gl::Context *context = gl::GetValidGlobalContext();

    Error error = ValidateStreamConsumerGLTextureExternalKHR(display, context, streamObject);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    error = streamObject->createConsumerGLTextureExternal(AttributeMap(), context);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    thread->setError(error);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY StreamConsumerAcquireKHR(EGLDisplay dpy, EGLStreamKHR stream)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR = 0x%0.8p)", dpy, stream);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    Stream *streamObject = static_cast<Stream *>(stream);
    gl::Context *context = gl::GetValidGlobalContext();

    Error error = ValidateStreamConsumerAcquireKHR(display, context, streamObject);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    error = streamObject->consumerAcquire();
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    thread->setError(error);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY StreamConsumerReleaseKHR(EGLDisplay dpy, EGLStreamKHR stream)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR = 0x%0.8p)", dpy, stream);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    Stream *streamObject = static_cast<Stream *>(stream);
    gl::Context *context = gl::GetValidGlobalContext();

    Error error = ValidateStreamConsumerReleaseKHR(display, context, streamObject);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    error = streamObject->consumerRelease();
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    thread->setError(error);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY StreamConsumerGLTextureExternalAttribsNV(EGLDisplay dpy,
                                                                EGLStreamKHR stream,
                                                                const EGLAttrib *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR stream = 0x%0.8p, EGLAttrib attrib_list = 0x%0.8p",
        dpy, stream, attrib_list);
    Thread *thread = GetCurrentThread();

    Display *display        = static_cast<Display *>(dpy);
    Stream *streamObject    = static_cast<Stream *>(stream);
    gl::Context *context    = gl::GetValidGlobalContext();
    AttributeMap attributes = AttributeMap::CreateFromAttribArray(attrib_list);

    Error error = ValidateStreamConsumerGLTextureExternalAttribsNV(display, context, streamObject,
                                                                   attributes);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    error = streamObject->createConsumerGLTextureExternal(attributes, context);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    thread->setError(error);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY CreateStreamProducerD3DTextureNV12ANGLE(EGLDisplay dpy,
                                                               EGLStreamKHR stream,
                                                               const EGLAttrib *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR stream = 0x%0.8p, EGLAttrib attrib_list = 0x%0.8p",
        dpy, stream, attrib_list);
    Thread *thread = GetCurrentThread();

    Display *display        = static_cast<Display *>(dpy);
    Stream *streamObject    = static_cast<Stream *>(stream);
    AttributeMap attributes = AttributeMap::CreateFromAttribArray(attrib_list);

    Error error =
        ValidateCreateStreamProducerD3DTextureNV12ANGLE(display, streamObject, attributes);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    error = streamObject->createProducerD3D11TextureNV12(attributes);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    thread->setError(error);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY StreamPostD3DTextureNV12ANGLE(EGLDisplay dpy,
                                                     EGLStreamKHR stream,
                                                     void *texture,
                                                     const EGLAttrib *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR stream = 0x%0.8p, void* texture = 0x%0.8p, "
        "EGLAttrib attrib_list = 0x%0.8p",
        dpy, stream, texture, attrib_list);
    Thread *thread = GetCurrentThread();

    Display *display        = static_cast<Display *>(dpy);
    Stream *streamObject    = static_cast<Stream *>(stream);
    AttributeMap attributes = AttributeMap::CreateFromAttribArray(attrib_list);

    Error error = ValidateStreamPostD3DTextureNV12ANGLE(display, streamObject, texture, attributes);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    error = streamObject->postD3D11NV12Texture(texture, attributes);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    thread->setError(error);
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY GetSyncValuesCHROMIUM(EGLDisplay dpy,
                                             EGLSurface surface,
                                             EGLuint64KHR *ust,
                                             EGLuint64KHR *msc,
                                             EGLuint64KHR *sbc)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLuint64KHR* ust = 0x%0.8p, "
        "EGLuint64KHR* msc = 0x%0.8p, EGLuint64KHR* sbc = 0x%0.8p",
        dpy, surface, ust, msc, sbc);
    Thread *thread = GetCurrentThread();

    Display *display    = static_cast<Display *>(dpy);
    Surface *eglSurface = static_cast<Surface *>(surface);

    Error error = ValidateGetSyncValuesCHROMIUM(display, eglSurface, ust, msc, sbc);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    error = eglSurface->getSyncValues(ust, msc, sbc);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    thread->setError(error);
    return EGL_TRUE;
}

ANGLE_EXPORT EGLBoolean SwapBuffersWithDamageEXT(EGLDisplay dpy,
                                                 EGLSurface surface,
                                                 EGLint *rects,
                                                 EGLint n_rects)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLint *rects = 0x%0.8p, EGLint "
        "n_rects = %d)",
        dpy, surface, rects, n_rects);
    Thread *thread = GetCurrentThread();

    Display *display    = static_cast<Display *>(dpy);
    Surface *eglSurface = static_cast<Surface *>(surface);

    Error error = ValidateSwapBuffersWithDamageEXT(display, eglSurface, rects, n_rects);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    error = eglSurface->swapWithDamage(rects, n_rects);
    if (error.isError())
    {
        thread->setError(error);
        return EGL_FALSE;
    }

    return EGL_TRUE;
}
}
