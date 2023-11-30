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

#include "WPEExtensions.h"
#include "WPEViewHeadless.h"
#include <gio/gio.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

#if USE(LIBDRM)
#include <drm_fourcc.h>
#endif

/**
 * WPEDisplayHeadless:
 *
 */
struct _WPEDisplayHeadlessPrivate {

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
    return wpe_view_headless_new(WPE_DISPLAY_HEADLESS(display));
}

#if USE(LIBDRM)
static GList* wpeDisplayHeadlessGetPreferredDMABufFormats(WPEDisplay*)
{
    GList* preferredFormats = nullptr;
    GRefPtr<GArray> dmabufModifiers = adoptGRef(g_array_sized_new(FALSE, TRUE, sizeof(guint64), 1));
    guint64 linearModifier = DRM_FORMAT_MOD_LINEAR;
    g_array_append_val(dmabufModifiers.get(), linearModifier);
    return g_list_prepend(preferredFormats, wpe_buffer_dma_buf_format_new(WPE_BUFFER_DMA_BUF_FORMAT_USAGE_MAPPING, DRM_FORMAT_ARGB8888, dmabufModifiers.get()));
}
#endif

static void wpe_display_headless_class_init(WPEDisplayHeadlessClass* displayHeadlessClass)
{
    WPEDisplayClass* displayClass = WPE_DISPLAY_CLASS(displayHeadlessClass);
    displayClass->connect = wpeDisplayHeadlessConnect;
    displayClass->create_view = wpeDisplayHeadlessCreateView;
#if USE(LIBDRM)
    displayClass->get_preferred_dma_buf_formats = wpeDisplayHeadlessGetPreferredDMABufFormats;
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
