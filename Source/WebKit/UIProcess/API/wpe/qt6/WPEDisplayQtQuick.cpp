/*
 * Copyright (C) 2024 Igalia S.L.
 * Copyright (C) 2026 Savoir-faire Linux, Inc.
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
#include "WPEDisplayQtQuick.h"

#include "WPEToplevelQtQuick.h"
#include "WPEViewQtQuick.h"

#include <epoxy/egl.h>

#include <QGuiApplication>
#include <QOpenGLFunctions>
#include <qpa/qplatformnativeinterface.h>
#include <wpe/GRefPtrWPE.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/unix/UnixFileDescriptor.h>

#ifndef EGL_DRM_RENDER_NODE_FILE_EXT
#define EGL_DRM_RENDER_NODE_FILE_EXT 0x3377
#endif

/**
 * WPEDisplayQtQuick:
 *
 * A #WPEDisplay implementation for Qt Quick applications that uses the EGL display provided by Qt.
 */
struct _WPEDisplayQtQuickPrivate {
    EGLDisplay eglDisplay;
    GRefPtr<WPEDRMDevice> drmDevice;
    bool usesEGL15 { false };
    bool hasNativeFenceSync { false };
    bool hasKHRWaitSync { false };
};
WEBKIT_DEFINE_FINAL_TYPE(WPEDisplayQtQuick, wpe_display_qtquick, WPE_TYPE_DISPLAY, WPEDisplay)

static gboolean wpeDisplayQtQuickConnect(WPEDisplay* display, GError** error)
{
    auto* priv = WPE_DISPLAY_QTQUICK(display)->priv;

    auto eglDisplay = static_cast<EGLDisplay>(QGuiApplication::platformNativeInterface()->nativeResourceForIntegration("eglDisplay"));
    if (!eglDisplay) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to initialize rendering: Cannot access EGL display via Qt");
        return FALSE;
    }

    priv->eglDisplay = eglDisplay;

    if (!epoxy_has_egl_extension(eglDisplay, "EGL_EXT_device_query")) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to initialize rendering: 'EGL_EXT_device_query' not available");
        return FALSE;
    }

    priv->usesEGL15 = epoxy_egl_version(eglDisplay) >= 15;
    priv->hasNativeFenceSync = epoxy_has_egl_extension(eglDisplay, "EGL_ANDROID_native_fence_sync")
        && (priv->usesEGL15 || epoxy_has_egl_extension(eglDisplay, "EGL_KHR_fence_sync"));
    priv->hasKHRWaitSync = epoxy_has_egl_extension(eglDisplay, "EGL_KHR_wait_sync");

    EGLDeviceEXT eglDevice;
    if (!eglQueryDisplayAttribEXT(eglDisplay, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&eglDevice))) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to initialize rendering: 'EGLDeviceEXT' not available");
        return FALSE;
    }

    const char* drmDevice = nullptr;
    const char* extensions = eglQueryDeviceStringEXT(eglDevice, EGL_EXTENSIONS);
    if (epoxy_extension_in_string(extensions, "EGL_EXT_device_drm"))
        drmDevice = eglQueryDeviceStringEXT(eglDevice, EGL_DRM_DEVICE_FILE_EXT);
    else {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to initialize rendering: 'EGL_EXT_device_drm' not available");
        return FALSE;
    }

    const char* drmRenderNode = nullptr;
    if (epoxy_extension_in_string(extensions, "EGL_EXT_device_drm_render_node"))
        drmRenderNode = eglQueryDeviceStringEXT(eglDevice, EGL_DRM_RENDER_NODE_FILE_EXT);
    else {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to initialize rendering: 'EGL_EXT_device_drm_render_node' not available");
        return FALSE;
    }

    priv->drmDevice = adoptGRef(wpe_drm_device_new(drmDevice, drmRenderNode));
    return TRUE;
}

static WPEView* wpeDisplayQtQuickCreateView(WPEDisplay* display)
{
    auto* displayQt = WPE_DISPLAY_QTQUICK(display);
    auto* view = wpe_view_qtquick_new(displayQt);

    GRefPtr<WPEToplevel> toplevel = adoptGRef(wpe_toplevel_qtquick_new(displayQt));
    wpe_view_set_toplevel(view, toplevel.get());

    return view;
}

static gpointer wpeDisplayQtQuickGetEGLDisplay(WPEDisplay* display, GError**)
{
    return WPE_DISPLAY_QTQUICK(display)->priv->eglDisplay;
}

static WPEDRMDevice* wpeDisplayQtQuickGetDRMDevice(WPEDisplay* display)
{
    return WPE_DISPLAY_QTQUICK(display)->priv->drmDevice.get();
}

static gboolean wpeDisplayQtQuickUseExplicitSync(WPEDisplay* display)
{
    return WPE_DISPLAY_QTQUICK(display)->priv->hasNativeFenceSync;
}

static EGLSync createNativeFenceSync(EGLDisplay display, EGLint fd, bool usesEGL15)
{
    if (usesEGL15) {
        const EGLAttrib attributes[] = { EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fd, EGL_NONE };
        return eglCreateSync(display, EGL_SYNC_NATIVE_FENCE_ANDROID, attributes);
    }

    const EGLint attributes[] = { EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fd, EGL_NONE };
    return eglCreateSyncKHR(display, EGL_SYNC_NATIVE_FENCE_ANDROID, attributes);
}

static void destroyNativeFenceSync(EGLDisplay display, EGLSync sync, bool usesEGL15)
{
    usesEGL15 ? eglDestroySync(display, sync) : eglDestroySyncKHR(display, sync);
}

static bool waitNativeFenceSync(EGLDisplay display, EGLSync sync, bool usesEGL15, bool hasKHRWaitSync)
{
    if (usesEGL15)
        return eglWaitSync(display, sync, 0) == EGL_TRUE;
    if (hasKHRWaitSync)
        return eglWaitSyncKHR(display, sync, 0) == EGL_TRUE;
    return eglClientWaitSyncKHR(display, sync, 0, EGL_FOREVER_KHR) == EGL_CONDITION_SATISFIED_KHR;
}

static void wpe_display_qtquick_class_init(WPEDisplayQtQuickClass* displayQtQuickClass)
{
    WPEDisplayClass* displayClass = WPE_DISPLAY_CLASS(displayQtQuickClass);
    displayClass->connect = wpeDisplayQtQuickConnect;
    displayClass->create_view = wpeDisplayQtQuickCreateView;
    displayClass->get_egl_display = wpeDisplayQtQuickGetEGLDisplay;
    displayClass->get_drm_device = wpeDisplayQtQuickGetDRMDevice;
    displayClass->use_explicit_sync = wpeDisplayQtQuickUseExplicitSync;
}

WPEDisplay* wpe_display_qtquick_new(void)
{
    return WPE_DISPLAY(g_object_new(WPE_TYPE_DISPLAY_QTQUICK, nullptr));
}

gboolean wpe_display_qtquick_wait_rendering_fence(WPEDisplayQtQuick* display, WPEBuffer* buffer, GError** error)
{
    WTF::UnixFileDescriptor fd {
        wpe_buffer_take_rendering_fence(buffer), WTF::UnixFileDescriptor::Adopt
    };
    if (!fd)
        return true;

    auto* priv = display->priv;
    auto sync = createNativeFenceSync(priv->eglDisplay, fd.value(), priv->usesEGL15);
    if (sync == EGL_NO_SYNC) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED,
            "Failed to import WPE rendering fence");
        return false;
    }
    (void)fd.release();

    bool waited = waitNativeFenceSync(priv->eglDisplay, sync, priv->usesEGL15, priv->hasKHRWaitSync);
    destroyNativeFenceSync(priv->eglDisplay, sync, priv->usesEGL15);

    if (!waited) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to wait for WPE rendering fence");
        return false;
    }
    return true;
}

int wpe_display_qtquick_create_release_fence(WPEDisplayQtQuick* display, QOpenGLFunctions* gl)
{
    auto* priv = display->priv;
    if (!priv->hasNativeFenceSync)
        return -1;

    auto sync = createNativeFenceSync(priv->eglDisplay, EGL_NO_NATIVE_FENCE_FD_ANDROID, priv->usesEGL15);
    if (sync == EGL_NO_SYNC)
        return -1;

    gl->glFlush();

    auto fd = eglDupNativeFenceFDANDROID(priv->eglDisplay, sync);
    destroyNativeFenceSync(priv->eglDisplay, sync, priv->usesEGL15);
    return fd;
}
