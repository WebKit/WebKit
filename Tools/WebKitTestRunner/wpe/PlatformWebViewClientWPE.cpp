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
#include <wpe/wpe-platform.h>
#include <wtf/glib/GUniquePtr.h>

IGNORE_CLANG_WARNINGS_BEGIN("cast-align")
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkPixmap.h>
IGNORE_CLANG_WARNINGS_END

namespace WTR {

PlatformWebViewClientWPE::PlatformWebViewClientWPE(WKPageConfigurationRef configuration)
{
    WPEDisplay* display = wpe_display_get_default();
    if (!display)
        g_error("Failed to get the default WPE display\n");
    m_view = WKViewCreate(display, configuration);
    auto* wpeView = WKViewGetView(m_view);
    m_toplevel = wpe_view_get_toplevel(wpeView);
    wpe_view_focus_in(wpeView);
    wpe_toplevel_resize(m_toplevel.get(), 800, 600);
    g_signal_connect(wpeView, "buffer-rendered", G_CALLBACK(+[](WPEView*, WPEBuffer* buffer, gpointer userData) {
        auto& view = *static_cast<PlatformWebViewClientWPE*>(userData);
        view.m_buffer = buffer;
    }), this);
}

PlatformWebViewClientWPE::~PlatformWebViewClientWPE()
{
    g_signal_handlers_disconnect_by_data(WKViewGetView(m_view), this);
}

static bool toplevelContainsView(WPEToplevel* toplevel, WPEView* view)
{
    struct Context {
        WPEView* view;
        bool found;
    } context = { view, false };

    wpe_toplevel_foreach_view(toplevel, +[](WPEToplevel* toplevel, WPEView* item, gpointer userData) -> gboolean {
        auto& context = *static_cast<Context*>(userData);
        if (context.view == item) {
            context.found = true;
            return TRUE;
        }
        return FALSE;
    }, &context);

    return context.found;
}

void PlatformWebViewClientWPE::addToWindow()
{
    auto* wpeView = WKViewGetView(m_view);
    if (toplevelContainsView(m_toplevel.get(), wpeView))
        return;

    wpe_view_set_toplevel(wpeView, m_toplevel.get());
}

void PlatformWebViewClientWPE::removeFromWindow()
{
    auto* wpeView = WKViewGetView(m_view);
    if (!toplevelContainsView(m_toplevel.get(), wpeView))
        return;

    wpe_view_set_toplevel(wpeView, nullptr);
}

WKSize PlatformWebViewClientWPE::size()
{
    int width, height;
    wpe_toplevel_get_size(m_toplevel.get(), &width, &height);
    return { static_cast<double>(width), static_cast<double>(height) };
}

void PlatformWebViewClientWPE::resize(WKSize size)
{
    wpe_toplevel_resize(m_toplevel.get(), size.width, size.height);
}

void PlatformWebViewClientWPE::focus()
{
    wpe_view_focus_in(WKViewGetView(m_view));
}

PlatformImage PlatformWebViewClientWPE::snapshot()
{
    while (g_main_context_pending(nullptr))
        g_main_context_iteration(nullptr, TRUE);

    GUniqueOutPtr<GError> error;
    GBytes* pixels = wpe_buffer_import_to_pixels(m_buffer.get(), &error.outPtr());
    if (!pixels)
        g_error("Failed to import buffer to pixels: %s\n", error->message);

    gsize pixelsDataSize;
    const auto* pixelsData = g_bytes_get_data(pixels, &pixelsDataSize);
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new(pixelsData, pixelsDataSize));

    auto width = wpe_buffer_get_width(m_buffer.get());
    auto height = wpe_buffer_get_height(m_buffer.get());
    auto info = SkImageInfo::MakeN32Premul(width, height, SkColorSpace::MakeSRGB());
    SkPixmap pixmap(info, g_bytes_get_data(bytes.get(), nullptr), info.minRowBytes());
    return SkImages::RasterFromPixmap(pixmap, [](const void*, void* context) {
        g_bytes_unref(static_cast<GBytes*>(context));
    }, bytes.leakRef()).release();
}

} // namespace WTR

#endif // ENABLE(WPE_PLATFORM)
