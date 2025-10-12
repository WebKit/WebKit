/*
 * Copyright (C) 2023 Igalia S.L.
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
#include "WPEDisplayHeadless.h"

#include "WPEBufferDMABufFormats.h"
#include "WPEDRMDevicePrivate.h"
#include "WPEExtensions.h"
#include "WPEToplevelHeadless.h"
#include "WPEViewHeadless.h"
#include <epoxy/egl.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <optional>
#include <unistd.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>
#include <wtf/unix/UnixFileDescriptor.h>

#if USE(GBM)
#include <gbm.h>
#endif

#if USE(LIBDRM)
#include <drm_fourcc.h>
#include <xf86drm.h>
#endif

/**
 * WPEDisplayHeadless:
 *
 */
struct _WPEDisplayHeadlessPrivate {
    std::optional<GRefPtr<WPEDRMDevice>> drmDevice;
#if USE(GBM)
    UnixFileDescriptor gbmDeviceFD;
    struct gbm_device* gbmDevice;
#endif
};
WEBKIT_DEFINE_FINAL_TYPE_WITH_CODE(WPEDisplayHeadless, wpe_display_headless, WPE_TYPE_DISPLAY, WPEDisplay,
    wpeEnsureExtensionPointsRegistered();
    g_io_extension_point_implement(WPE_DISPLAY_EXTENSION_POINT_NAME, g_define_type_id, "wpe-display-headless", -100))

static void wpeDisplayHeadlessDispose(GObject* object)
{
    auto* priv = WPE_DISPLAY_HEADLESS(object)->priv;

#if USE(GBM)
    g_clear_pointer(&priv->gbmDevice, gbm_device_destroy);
    priv->gbmDeviceFD = { };
#endif

    G_OBJECT_CLASS(wpe_display_headless_parent_class)->dispose(object);
}

static gboolean wpeDisplayHeadlessConnect(WPEDisplay*, GError**)
{
    return TRUE;
}

static WPEView* wpeDisplayHeadlessCreateView(WPEDisplay* display)
{
    auto* view = WPE_VIEW(g_object_new(WPE_TYPE_VIEW_HEADLESS, "display", display, nullptr));
    if (wpe_settings_get_boolean(wpe_display_get_settings(display), WPE_SETTING_CREATE_VIEWS_WITH_A_TOPLEVEL, nullptr)) {
        GRefPtr<WPEToplevel> toplevel = adoptGRef(wpe_toplevel_headless_new(WPE_DISPLAY_HEADLESS(display)));
        wpe_view_set_toplevel(view, toplevel.get());
    }
    return view;
}

static gpointer wpeDisplayHeadlessGetEGLDisplay(WPEDisplay* display, GError** error)
{
#if USE(GBM)
    if (auto* drmDevice = wpe_display_get_drm_device(display)) {
        if (!epoxy_has_egl_extension(nullptr, "EGL_KHR_platform_gbm")) {
            g_set_error_literal(error, WPE_EGL_ERROR, WPE_EGL_ERROR_NOT_AVAILABLE, "Can't get EGL display: GBM platform not supported");
            return nullptr;
        }

        const char* filename = wpe_drm_device_get_render_node(drmDevice);
        if (!filename)
            filename = wpe_drm_device_get_primary_node(drmDevice);

        auto fd = UnixFileDescriptor { open(filename, O_RDWR | O_CLOEXEC), UnixFileDescriptor::Adopt };
        if (!fd) {
            g_set_error(error, WPE_EGL_ERROR, WPE_EGL_ERROR_NOT_AVAILABLE, "Can't get EGL display: failed to open device %s", filename);
            return nullptr;
        }
        auto* device = gbm_create_device(fd.value());
        if (!device) {
            g_set_error(error, WPE_EGL_ERROR, WPE_EGL_ERROR_NOT_AVAILABLE, "Can't get EGL display: failed to create GBM device for %s", filename);
            return nullptr;
        }

        EGLDisplay eglDisplay = EGL_NO_DISPLAY;
        if (epoxy_has_egl_extension(nullptr, "EGL_EXT_platform_base"))
            eglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, device, nullptr);
        else if (epoxy_has_egl_extension(nullptr, "EGL_KHR_platform_base"))
            eglDisplay = eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR, device, nullptr);

        if (eglDisplay != EGL_NO_DISPLAY) {
            auto* priv = WPE_DISPLAY_HEADLESS(display)->priv;
            priv->gbmDeviceFD = WTFMove(fd);
            priv->gbmDevice = device;
            return eglDisplay;
        }

        gbm_device_destroy(device);
        g_set_error(error, WPE_EGL_ERROR, WPE_EGL_ERROR_NOT_AVAILABLE, "Can't get EGL display: failed to create GBM EGL display for %s", filename);
        return nullptr;
    }
#endif

    if (!epoxy_has_egl_extension(nullptr, "EGL_MESA_platform_surfaceless")) {
        g_set_error_literal(error, WPE_EGL_ERROR, WPE_EGL_ERROR_NOT_AVAILABLE, "Can't get EGL display: Surfaceless platform not supported");
        return nullptr;
    }

    EGLDisplay eglDisplay = EGL_NO_DISPLAY;
    if (epoxy_has_egl_extension(nullptr, "EGL_EXT_platform_base"))
        eglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    else if (epoxy_has_egl_extension(nullptr, "EGL_KHR_platform_base"))
        eglDisplay = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    if (eglDisplay != EGL_NO_DISPLAY)
        return eglDisplay;

    g_set_error_literal(error, WPE_EGL_ERROR, WPE_EGL_ERROR_NOT_AVAILABLE, "Can't get EGL display: failed to create surfaceless EGL display");
    return nullptr;
}

static WPEDRMDevice* wpeDisplayHeadlessGetDRMDevice(WPEDisplay* display)
{
    auto* displayHeadless = WPE_DISPLAY_HEADLESS(display);
    auto* priv = displayHeadless->priv;
    if (!priv->drmDevice.has_value())
        priv->drmDevice = wpeDRMDeviceCreateForDevice(nullptr);
    return priv->drmDevice->get();
}

static void wpe_display_headless_class_init(WPEDisplayHeadlessClass* displayHeadlessClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(displayHeadlessClass);
    objectClass->dispose = wpeDisplayHeadlessDispose;

    WPEDisplayClass* displayClass = WPE_DISPLAY_CLASS(displayHeadlessClass);
    displayClass->connect = wpeDisplayHeadlessConnect;
    displayClass->create_view = wpeDisplayHeadlessCreateView;
    displayClass->get_egl_display = wpeDisplayHeadlessGetEGLDisplay;
    displayClass->get_drm_device = wpeDisplayHeadlessGetDRMDevice;
}

/**
 * wpe_display_headless_new:
 *
 * Create a new #WPEDisplayHeadless
 *
 * Returns: (transfer full): a #WPEDisplay
 */
WPEDisplay* wpe_display_headless_new(void)
{
    return WPE_DISPLAY(g_object_new(WPE_TYPE_DISPLAY_HEADLESS, nullptr));
}

/**
 * wpe_display_headless_new_for_device: (skip)
 * @name: the name of the DRM device
 * @error: return location for error or %NULL to ignore
 *
 * Create a new #WPEDisplayHeadless for the DRM device with @name.
 *
 * Returns: (nullable) (transfer full): a #WPEDisplay, or %NULL if an error occurred
 */
WPEDisplay* wpe_display_headless_new_for_device(const char* name, GError** error)
{
#if USE(LIBDRM)
    auto drmDevice = wpeDRMDeviceCreateForDevice(name);
    if (!drmDevice) {
        g_set_error(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_NOT_SUPPORTED, "DRM device \"%s\" not found", name);
        return nullptr;
    }

    auto* display = WPE_DISPLAY_HEADLESS(wpe_display_headless_new());
    auto* priv = display->priv;
    priv->drmDevice = WTFMove(drmDevice);
    return WPE_DISPLAY(display);
#else
    g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_NOT_SUPPORTED, "DRM device not supported");
    return nullptr;
#endif
}
