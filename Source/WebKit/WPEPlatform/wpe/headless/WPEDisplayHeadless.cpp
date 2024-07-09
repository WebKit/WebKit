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
#include <gio/gio.h>
#include <optional>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

#if USE(LIBDRM)
#include <drm_fourcc.h>
#include <xf86drm.h>
#endif

/**
 * WPEDisplayHeadless:
 *
 */
struct _WPEDisplayHeadlessPrivate {
#if USE(LIBDRM)
    std::optional<CString> drmDevice;
    std::optional<CString> drmRenderNode;
#endif
};
WEBKIT_DEFINE_FINAL_TYPE_WITH_CODE(WPEDisplayHeadless, wpe_display_headless, WPE_TYPE_DISPLAY, WPEDisplay,
    wpeEnsureExtensionPointsRegistered();
    g_io_extension_point_implement(WPE_DISPLAY_EXTENSION_POINT_NAME, g_define_type_id, "wpe-display-headless", -100))

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

#if USE(LIBDRM)
static WPEBufferDMABufFormats* wpeDisplayHeadlessGetPreferredDMABufFormats(WPEDisplay*)
{
    // FIXME: we should get a DRM device in headless mode too when not forcing swrast.
    auto* builder = wpe_buffer_dma_buf_formats_builder_new(nullptr);
    wpe_buffer_dma_buf_formats_builder_append_group(builder, nullptr, WPE_BUFFER_DMA_BUF_FORMAT_USAGE_MAPPING);
    wpe_buffer_dma_buf_formats_builder_append_format(builder, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR);
    return wpe_buffer_dma_buf_formats_builder_end(builder);
}

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
    WPEDisplayClass* displayClass = WPE_DISPLAY_CLASS(displayHeadlessClass);
    displayClass->connect = wpeDisplayHeadlessConnect;
    displayClass->create_view = wpeDisplayHeadlessCreateView;
#if USE(LIBDRM)
    displayClass->get_preferred_dma_buf_formats = wpeDisplayHeadlessGetPreferredDMABufFormats;
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
