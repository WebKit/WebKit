/*
 * Copyright (C) 2026 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEBufferBroadcomNexus.h"

#if USE(NEXUS)

#include "WPEDisplayPrivate.h"
#include <array>
#include <epoxy/egl.h>
#include <nexus_platform.h>
#include <nexus_surface.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

struct _WPEBufferBroadcomNexusPrivate {
    GBytes* surfaceMemory;
    EGLImage eglImage;
    uintptr_t surfaceHandle;
};

WEBKIT_DEFINE_FINAL_TYPE(WPEBufferBroadcomNexus, wpe_buffer_broadcom_nexus, WPE_TYPE_BUFFER, WPEBuffer)

static void wpeBufferNexusDisposeEGLImageIfNeeded(WPEBufferBroadcomNexus* buffer)
{
    auto* priv = buffer->priv;
    if (!priv->eglImage)
        return;

    auto* eglImage = std::exchange(priv->eglImage, nullptr);
    auto* display = wpe_buffer_get_display(WPE_BUFFER(buffer));
    if (!display)
        return;

    if (auto* eglDisplay = wpe_display_get_egl_display(display, nullptr)) {
        static PFNEGLDESTROYIMAGEPROC s_eglDestroyImageKHR;
        if (!s_eglDestroyImageKHR)
            s_eglDestroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEPROC>(epoxy_eglGetProcAddress("eglDestroyImageKHR"));
        s_eglDestroyImageKHR(eglDisplay, eglImage);
    }
}

static void wpeBufferBroadcomNexusDispose(GObject* object)
{
    WPEBufferBroadcomNexus* buffer = WPE_BUFFER_BROADCOM_NEXUS(object);

    wpeBufferNexusDisposeEGLImageIfNeeded(WPE_BUFFER_BROADCOM_NEXUS(object));
    NEXUS_Surface_Destroy(reinterpret_cast<NEXUS_SurfaceHandle>(buffer->priv->surfaceHandle));

    G_OBJECT_CLASS(wpe_buffer_broadcom_nexus_parent_class)->dispose(object);
}

static gpointer wpeBufferNexusImportToEGLImage(WPEBuffer* buffer, GError** error)
{
    auto* priv = WPE_BUFFER_BROADCOM_NEXUS(buffer)->priv;
    auto* display = wpe_buffer_get_display(buffer);
    if (!display) {
        priv->eglImage = nullptr;
        g_set_error_literal(error, WPE_BUFFER_ERROR, WPE_BUFFER_ERROR_IMPORT_FAILED, "The WPE display of buffer has already been closed");
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

    // Epoxy requires a current context for the symbol resolver to work automatically.
    static PFNEGLCREATEIMAGEKHRPROC s_eglCreateImageKHR;
    if (!s_eglCreateImageKHR) {
        if (epoxy_has_egl_extension(eglDisplay, "EGL_KHR_image_base"))
            s_eglCreateImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(epoxy_eglGetProcAddress("eglCreateImageKHR"));
    }
    if (!s_eglCreateImageKHR) {
        g_set_error_literal(error, WPE_BUFFER_ERROR, WPE_BUFFER_ERROR_IMPORT_FAILED, "Failed to import buffer to EGL image: eglCreateImageKHR not found");
        return nullptr;
    }

    std::array<EGLint, 1> attributes { EGL_NONE };

    priv->eglImage = s_eglCreateImageKHR(eglDisplay, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR, reinterpret_cast<EGLClientBuffer>(priv->surfaceHandle), attributes.data());
    if (!priv->eglImage)
        g_set_error(error, WPE_BUFFER_ERROR, WPE_BUFFER_ERROR_IMPORT_FAILED, "Failed to import buffer to EGL image: eglCreateImageKHR failed with error %#04x", eglGetError());
    return priv->eglImage;
}

static void wpe_buffer_broadcom_nexus_class_init(WPEBufferBroadcomNexusClass* bufferBroadcomNexusClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(bufferBroadcomNexusClass);
    objectClass->dispose = wpeBufferBroadcomNexusDispose;

    WPEBufferClass* bufferClass = WPE_BUFFER_CLASS(bufferBroadcomNexusClass);
    bufferClass->import_to_egl_image = wpeBufferNexusImportToEGLImage;
}

WPEBufferBroadcomNexus* wpe_buffer_broadcom_nexus_new(WPEDisplay* display, uintptr_t surfaceHandle)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);

    auto* buffer = WPE_BUFFER_BROADCOM_NEXUS(g_object_new(WPE_TYPE_BUFFER_BROADCOM_NEXUS,
        "display", display,
        nullptr));

    buffer->priv->surfaceHandle = surfaceHandle;

    return buffer;
}

uintptr_t wpe_buffer_broadcom_nexus_get_surface_handle(WPEBufferBroadcomNexus* buffer)
{
    return buffer->priv->surfaceHandle;
}

GBytes* wpe_buffer_broadcom_nexus_get_memory(WPEBufferBroadcomNexus* buffer, unsigned short* width, unsigned short* height, unsigned* stride)
{
    auto surfaceHandle = reinterpret_cast<NEXUS_SurfaceHandle>(buffer->priv->surfaceHandle);
    bool colorSwap = false;

    NEXUS_SurfaceCreateSettings createSettings;
    NEXUS_Surface_GetCreateSettings(surfaceHandle, &createSettings);

    switch (createSettings.pixelFormat) {
    case NEXUS_PixelFormat_eA8_R8_G8_B8:
    case NEXUS_PixelFormat_eX8_R8_G8_B8:
        // Can be used as is
        break;
    case NEXUS_PixelFormat_eA8_B8_G8_R8:
    case NEXUS_PixelFormat_eX8_B8_G8_R8:
        // Need to swap red and blue channels
        colorSwap = true;
        break;
    default:
        // Buffer can't be used for wayland shm
        return nullptr;
    }

    NEXUS_SurfaceMemory surfaceMemory;
    NEXUS_Surface_GetMemory(surfaceHandle, &surfaceMemory);

    if (width)
        *width = createSettings.width;
    if (height)
        *height = createSettings.height;
    if (stride)
        *stride = surfaceMemory.pitch;

    auto* bufMem = static_cast<unsigned char*>(surfaceMemory.buffer);

    if (colorSwap) {
        // A gross (slow) hack to turn ABGR data into ARGB for wayland shm buffers
        for (size_t i = 0; i < surfaceMemory.bufferSize; i += 4)
            std::swap(bufMem[i], bufMem[i + 2]);
    }

    buffer->priv->surfaceMemory = g_bytes_new_static(surfaceMemory.buffer, surfaceMemory.bufferSize);
    return buffer->priv->surfaceMemory;
}

void wpe_buffer_broadcom_nexus_surface_flush(WPEBufferBroadcomNexus *buffer)
{
    g_bytes_unref(buffer->priv->surfaceMemory);
    NEXUS_Surface_Flush(reinterpret_cast<NEXUS_SurfaceHandle>(buffer->priv->surfaceHandle));
}

#endif
