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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformWebViewClientWPE.h"

#if ENABLE(WPE_PLATFORM)

#include <cairo.h>
#include <wpe/headless/wpe-headless.h>
#include <wtf/glib/GUniquePtr.h>

namespace WTR {

PlatformWebViewClientWPE::PlatformWebViewClientWPE(WKPageConfigurationRef configuration)
    : m_display(adoptGRef(wpe_display_headless_new()))
{
    m_view = WKViewCreate(m_display.get(), configuration);
    auto* wpeView = WKViewGetView(m_view);
    wpe_view_resize(wpeView, 800, 600);
    g_signal_connect(wpeView, "buffer-rendered", G_CALLBACK(+[](WPEView*, WPEBuffer* buffer, gpointer userData) {
        auto& view = *static_cast<PlatformWebViewClientWPE*>(userData);
        view.m_buffer = buffer;
    }), this);
}

PlatformWebViewClientWPE::~PlatformWebViewClientWPE()
{
    g_signal_handlers_disconnect_by_data(WKViewGetView(m_view), this);
}

void PlatformWebViewClientWPE::addToWindow()
{
    // FIXME: implement.
}

void PlatformWebViewClientWPE::removeFromWindow()
{
    // FIXME: implement.
}

cairo_surface_t* PlatformWebViewClientWPE::snapshot()
{
    while (g_main_context_pending(nullptr))
        g_main_context_iteration(nullptr, TRUE);

    GUniqueOutPtr<GError> error;
    GBytes* bytes = wpe_buffer_import_to_pixels(m_buffer.get(), &error.outPtr());
    if (!bytes)
        g_error("Failed to import buffer to pixels: %s\n", error->message);

    auto width = wpe_buffer_get_width(m_buffer.get());
    auto height = wpe_buffer_get_height(m_buffer.get());
    auto stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    auto* data = static_cast<unsigned char*>(const_cast<void*>(g_bytes_get_data(bytes, nullptr)));
    cairo_surface_t* surface = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, width, height, stride);
    static cairo_user_data_key_t s_surfaceDataKey;
    cairo_surface_set_user_data(surface, &s_surfaceDataKey, g_object_ref(m_buffer.get()), [](void* data) {
        g_object_unref(WPE_BUFFER(data));
    });
    cairo_surface_mark_dirty(surface);

    return surface;
}

} // namespace WTR

#endif // ENABLE(WPE_PLATFORM)
