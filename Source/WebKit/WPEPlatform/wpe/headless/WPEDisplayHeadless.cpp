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
#if USE(GBM)
    UnixFileDescriptor gbmDeviceFD;
    struct gbm_device* gbmDevice;
#endif
#if USE(LIBDRM)
    std::optional<CString> drmDevice;
    std::optional<CString> drmRenderNode;
#endif
};
WEBKIT_DEFINE_FINAL_TYPE_WITH_CODE(WPEDisplayHeadless, wpe_display_headless, WPE_TYPE_DISPLAY, WPEDisplay,
    wpeEnsureExtensionPointsRegistered();
    g_io_extension_point_implement(WPE_DISPLAY_EXTENSION_POINT_NAME, g_define_type_id, "wpe-display-headless", -100))

static void wpeDisplayHeadlessDispose(GObject* object)
{
#if USE(GBM)
    auto* priv = WPE_DISPLAY_HEADLESS(object)->priv;
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
    auto* view = wpe_view_headless_new(WPE_DISPLAY_HEADLESS(display));
    GRefPtr<WPEToplevel> toplevel = adoptGRef(wpe_toplevel_headless_new(WPE_DISPLAY_HEADLESS(display)));
    wpe_view_set_toplevel(view, toplevel.get());
    return view;
}

static gpointer wpeDisplayHeadlessGetEGLDisplay(WPEDisplay* display, GError** error)
{
#if USE(GBM)
    if (const char* filename = wpe_display_get_drm_render_node(display)) {
        if (!epoxy_has_egl_extension(nullptr, "EGL_KHR_platform_gbm")) {
            g_set_error_literal(error, WPE_EGL_ERROR, WPE_EGL_ERROR_NOT_AVAILABLE, "Can't get EGL display: GBM platform not supported");
            return nullptr;
        }

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

#if USE(LIBDRM)
static void wpeDisplayHeadlessInitializeDRMDevices(WPEDisplayHeadless* display)
{
    auto* priv = display->priv;
    priv->drmDevice = CString();
    priv->drmRenderNode = CString();

    drmDevicePtr devices[64];
    memset(devices, 0, sizeof(devices));

    int numDevices = drmGetDevices2(0, devices, std::size(devices));
    if (numDevices <= 0)
        return;

    for (int i = 0; i < numDevices && priv->drmDevice->isNull(); ++i) {
        drmDevice* device = devices[i];
        if (!(device->available_nodes & (1 << DRM_NODE_PRIMARY | 1 << DRM_NODE_RENDER)))
            continue;

        if (device->available_nodes & (1 << DRM_NODE_RENDER))
            priv->drmRenderNode = CString(device->nodes[DRM_NODE_RENDER]);
        if (device->available_nodes & (1 << DRM_NODE_PRIMARY))
            priv->drmDevice = CString(device->nodes[DRM_NODE_PRIMARY]);
    }
    drmFreeDevices(devices, numDevices);
}

static const char* wpeDisplayHeadlessGetDRMDevice(WPEDisplay* display)
{
    auto* displayHeadless = WPE_DISPLAY_HEADLESS(display);
    auto* priv = displayHeadless->priv;
    if (!priv->drmDevice.has_value())
        wpeDisplayHeadlessInitializeDRMDevices(displayHeadless);
    return priv->drmDevice->data();
}

static const char* wpeDisplayHeadlessGetDRMRenderNode(WPEDisplay* display)
{
    auto* displayHeadless = WPE_DISPLAY_HEADLESS(display);
    auto* priv = displayHeadless->priv;
    if (!priv->drmRenderNode.has_value())
        wpeDisplayHeadlessInitializeDRMDevices(displayHeadless);
    return !priv->drmRenderNode->isNull() ? priv->drmRenderNode->data() : priv->drmDevice->data();
}
#endif

static void wpe_display_headless_class_init(WPEDisplayHeadlessClass* displayHeadlessClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(displayHeadlessClass);
    objectClass->dispose = wpeDisplayHeadlessDispose;

    WPEDisplayClass* displayClass = WPE_DISPLAY_CLASS(displayHeadlessClass);
    displayClass->connect = wpeDisplayHeadlessConnect;
    displayClass->create_view = wpeDisplayHeadlessCreateView;
    displayClass->get_egl_display = wpeDisplayHeadlessGetEGLDisplay;
#if USE(LIBDRM)
    displayClass->get_drm_device = wpeDisplayHeadlessGetDRMDevice;
    displayClass->get_drm_render_node = wpeDisplayHeadlessGetDRMRenderNode;
#endif
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
    auto* display = WPE_DISPLAY_HEADLESS(wpe_display_headless_new());
    auto* priv = display->priv;
    drmDevicePtr devices[64];
    memset(devices, 0, sizeof(devices));

    int numDevices = drmGetDevices2(0, devices, std::size(devices));
    if (numDevices <= 0) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_NOT_SUPPORTED, "No DRM device found");
        return nullptr;
    }

    for (int i = 0; i < numDevices; ++i) {
        drmDevice* device = devices[i];
        for (int j = 0; j < DRM_NODE_MAX; ++j) {
            if (device->available_nodes & (1 << j) && !strcmp(device->nodes[j], name)) {
                if (device->available_nodes & (1 << DRM_NODE_RENDER))
                    priv->drmRenderNode = CString(device->nodes[DRM_NODE_RENDER]);
                else
                    priv->drmRenderNode = CString();
                if (device->available_nodes & (1 << DRM_NODE_PRIMARY))
                    priv->drmDevice = CString(device->nodes[DRM_NODE_PRIMARY]);
                else
                    priv->drmDevice = CString();
                drmFreeDevices(devices, numDevices);
                return WPE_DISPLAY(display);
            }
        }
    }

    drmFreeDevices(devices, numDevices);
    g_set_error(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_NOT_SUPPORTED, "DRM device \"%s\" not found", name);
    return nullptr;
#else
    g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_NOT_SUPPORTED, "DRM device not supported");
    return nullptr;
#endif
}
