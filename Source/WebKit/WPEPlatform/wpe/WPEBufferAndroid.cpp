/*
 * Copyright (C) 2025 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEBufferAndroid.h"

#if OS(ANDROID)
#include <android/hardware_buffer.h>
#include <drm/drm_fourcc.h>
#include <epoxy/egl.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/unix/UnixFileDescriptor.h>

/**
 * WPEBufferAndroid:
 *
 */
struct _WPEBufferAndroidPrivate {
    AHardwareBuffer* ahb;
    EGLImage eglImage { EGL_NO_IMAGE };
};
WEBKIT_DEFINE_FINAL_TYPE(WPEBufferAndroid, wpe_buffer_android, WPE_TYPE_BUFFER, WPEBuffer)

static std::function<EGLImage(EGLDisplay, AHardwareBuffer*)> s_createImage = nullptr;
static PFNEGLDESTROYIMAGEKHRPROC s_eglDestroyImage = nullptr;
static PFNEGLCREATEIMAGEKHRPROC s_eglCreateImageKHR = nullptr;
static PFNEGLCREATEIMAGEPROC s_eglCreateImage = nullptr;

static EGLImage createImageEGL15(EGLDisplay display, AHardwareBuffer * ahb)
{
    static constexpr std::array<EGLAttrib, 3> attributes = { EGL_IMAGE_PRESERVED, EGL_TRUE, EGL_NONE };
    return s_eglCreateImage(display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, ahb, attributes.data());
}

static EGLImage createImageKHRImageBase(EGLDisplay display, AHardwareBuffer* ahb)
{
    static constexpr std::array<EGLint, 3> attributes = { EGL_IMAGE_PRESERVED, EGL_TRUE, EGL_NONE };
    return s_eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, ahb, attributes.data());
}

static void wpeBufferAndroidDisposeEGLImageIfNeeded(WPEBufferAndroid* androidBuffer)
{
    RELEASE_ASSERT(s_eglDestroyImage);

    auto* priv = androidBuffer->priv;
    if (priv->eglImage == EGL_NO_IMAGE)
        return;

    auto* eglImage = std::exchange(priv->eglImage, EGL_NO_IMAGE);
    auto* display = wpe_buffer_get_display(WPE_BUFFER(androidBuffer));
    if (!display)
        return;

    if (auto* eglDisplay = wpe_display_get_egl_display(display, nullptr))
        s_eglDestroyImage(eglDisplay, eglImage);
}

static void wpeBufferAndroidDispose(GObject* object)
{
    auto* androidBuffer = WPE_BUFFER_ANDROID(object);

    wpeBufferAndroidDisposeEGLImageIfNeeded(androidBuffer);
    g_clear_pointer(&androidBuffer->priv->ahb, AHardwareBuffer_release);

    G_OBJECT_CLASS(wpe_buffer_android_parent_class)->dispose(object);
}

static gpointer wpeBufferAndroidImportToEGLImage(WPEBuffer* buffer, GError** error)
{
    auto* priv = WPE_BUFFER_ANDROID(buffer)->priv;
    auto* display = wpe_buffer_get_display(buffer);
    if (!display) {
        priv->eglImage = EGL_NO_IMAGE;
        g_set_error_literal(error, WPE_BUFFER_ERROR, WPE_BUFFER_ERROR_IMPORT_FAILED, "The WPE display of the buffer has already been closed");
        return nullptr;
    }

    if (priv->eglImage)
        return priv->eglImage;

    GUniqueOutPtr<GError> eglError;
    auto* eglDisplay = wpe_display_get_egl_display(display, &eglError.outPtr());
    if (eglDisplay == EGL_NO_DISPLAY) {
        g_set_error(error, WPE_BUFFER_ERROR, WPE_BUFFER_ERROR_IMPORT_FAILED, "Failed to get EGLDisplay when importing buffer to EGL image: %s", eglError->message);
        return nullptr;
    }

    if (!s_createImage) {
        if (epoxy_egl_version(eglDisplay) >= 15) {
            s_eglCreateImage = reinterpret_cast<PFNEGLCREATEIMAGEPROC>(epoxy_eglGetProcAddress("eglCreateImage"));
            s_eglDestroyImage = reinterpret_cast<PFNEGLDESTROYIMAGEPROC>(epoxy_eglGetProcAddress("eglDestroyImage"));
            if (s_eglCreateImage && s_eglDestroyImage)
                s_createImage = createImageEGL15;
        }
        if (!s_createImage && epoxy_has_egl_extension(eglDisplay, "EGL_KHR_image_base")) {
            s_eglCreateImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(epoxy_eglGetProcAddress("eglCreateImageKHR"));
            s_eglDestroyImage = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(epoxy_eglGetProcAddress("eglDestroyImageKHR"));
            if (s_eglCreateImageKHR && s_eglDestroyImage)
                s_createImage = createImageKHRImageBase;
        }
        if (!s_eglCreateImage) {
            g_set_error_literal(error, WPE_BUFFER_ERROR, WPE_BUFFER_ERROR_IMPORT_FAILED, "No EGLImage support, EGL 1.5 or EGL_KHR_image needed");
            return nullptr;
        }
    }
    RELEASE_ASSERT(s_createImage);
    RELEASE_ASSERT(s_eglDestroyImage);

    priv->eglImage = s_createImage(eglDisplay, priv->ahb);
    if (!priv->eglImage)
        g_set_error(error, WPE_BUFFER_ERROR, WPE_BUFFER_ERROR_IMPORT_FAILED, "Failed to import buffer to EGL image: eglCreateImageKHR failed with error %#04x", eglGetError());
    return priv->eglImage;
}

static void wpe_buffer_android_class_init(WPEBufferAndroidClass* bufferAndroidClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(bufferAndroidClass);
    objectClass->dispose = wpeBufferAndroidDispose;

    // FIXME(297316): Implement import_to_pixels vfunc.
    WPEBufferClass* bufferClass = WPE_BUFFER_CLASS(bufferAndroidClass);
    bufferClass->import_to_egl_image = wpeBufferAndroidImportToEGLImage;
}

/**
 * wpe_buffer_android_new: (constructor):
 * @display: a #WPEDisplay
 * @ahb: an #AHardwareBuffer
 *
 * Create a new #WPEBufferAndroid for the given buffer.
 *
 * The reference count of the @ahb will be incremented using
 * %AHardwareBuffer_acquire().
 *
 * Returns: (transfer full): a #WPEBufferAndroid
 */
WPEBufferAndroid* wpe_buffer_android_new(WPEDisplay* display, AHardwareBuffer* ahb)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);
    g_return_val_if_fail(ahb != nullptr, nullptr);

    AHardwareBuffer_acquire(ahb);

    AHardwareBuffer_Desc description;
    AHardwareBuffer_describe(ahb, &description);

    auto* buffer = WPE_BUFFER_ANDROID(g_object_new(WPE_TYPE_BUFFER_ANDROID,
        "display", display,
        "width", description.width,
        "height", description.height,
        nullptr));

    buffer->priv->ahb = ahb;

    return buffer;
}

/**
 * wpe_buffer_android_get_hardware_buffer:
 * @buffer: a #WPEBufferAndroid
 *
 * Get the underlying #AHardwareBuffer for @buffer.
 * Note that the returned #AHardwareBuffer might be destroyed along with
 * the @buffer; and `AHardwareBuffer_acquire()` may be called on the
 * returned value to ensure that the underlying #AHardwareBuffer stays
 * valid.
 *
 * Returns: (transfer none): a valid #AHardwareBuffer pointer.
 */
AHardwareBuffer* wpe_buffer_android_get_hardware_buffer(WPEBufferAndroid* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER_ANDROID(buffer), nullptr);

    return buffer->priv->ahb;
}

/**
 * wpe_buffer_android_get_format:
 * @buffer: a #WPEBufferAndroid
 *
 * Get the pixel format of the @buffer as a DRM FourCC code.
 *
 * Returns: a DRM FourCC code.
 */
guint32 wpe_buffer_android_get_format(WPEBufferAndroid* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER_ANDROID(buffer), 0);

    AHardwareBuffer_Desc description;
    AHardwareBuffer_describe(buffer->priv->ahb, &description);

    switch (description.format) {
    case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
        return DRM_FORMAT_RGBA8888;
    case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM:
        return DRM_FORMAT_RGBX8888;
    case AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM:
        return DRM_FORMAT_RGB888;
    case AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM:
        return DRM_FORMAT_RGB565;
    case AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM:
        return DRM_FORMAT_RGBA1010102;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    }
}
#endif // OS(ANDROID)
