/*
 * Copyright (C) 2024 Igalia S.L.
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
#include <qpa/qplatformnativeinterface.h>
#include <wpe/GRefPtrWPE.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

#ifndef EGL_DRM_RENDER_NODE_FILE_EXT
#define EGL_DRM_RENDER_NODE_FILE_EXT 0x3377
#endif

/**
 * WPEDisplayQtQuick:
 *
 */
struct _WPEDisplayQtQuickPrivate {
    EGLDisplay eglDisplay;
    GRefPtr<WPEDRMDevice> drmDevice;
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

    EGLDeviceEXT eglDevice;
    if (!eglQueryDisplayAttribEXT(eglDisplay, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&eglDevice))) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to initialize rendering: 'EGLDeviceEXT' not available");
        return FALSE;
    }

    CString drmDevice;
    const char* extensions = eglQueryDeviceStringEXT(eglDevice, EGL_EXTENSIONS);
    if (epoxy_extension_in_string(extensions, "EGL_EXT_device_drm"))
        drmDevice = eglQueryDeviceStringEXT(eglDevice, EGL_DRM_DEVICE_FILE_EXT);
    else {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to initialize rendering: 'EGL_EXT_device_drm' not available");
        return FALSE;
    }

    CString drmRenderNode;
    if (epoxy_extension_in_string(extensions, "EGL_EXT_device_drm_render_node"))
        drmRenderNode = eglQueryDeviceStringEXT(eglDevice, EGL_DRM_RENDER_NODE_FILE_EXT);
    else {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to initialize rendering: 'EGL_EXT_device_drm_render_node' not available");
        return FALSE;
    }

    priv->drmDevice = adoptGRef(wpe_drm_device_new(drmDevice.data(), drmRenderNode.data()));
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

static void wpe_display_qtquick_class_init(WPEDisplayQtQuickClass* displayQtQuickClass)
{
    WPEDisplayClass* displayClass = WPE_DISPLAY_CLASS(displayQtQuickClass);
    displayClass->connect = wpeDisplayQtQuickConnect;
    displayClass->create_view = wpeDisplayQtQuickCreateView;
    displayClass->get_egl_display = wpeDisplayQtQuickGetEGLDisplay;
    displayClass->get_drm_device = wpeDisplayQtQuickGetDRMDevice;
}

WPEDisplay* wpe_display_qtquick_new(void)
{
    return WPE_DISPLAY(g_object_new(WPE_TYPE_DISPLAY_QTQUICK, nullptr));
}
