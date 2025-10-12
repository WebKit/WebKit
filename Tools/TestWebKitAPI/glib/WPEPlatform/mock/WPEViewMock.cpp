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
#include "WPEViewMock.h"

#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

struct _WPEViewMockPrivate {
    GRefPtr<WPEBuffer> pendingBuffer;
    GRefPtr<WPEBuffer> committedBuffer;
    guint frameTimerID;
};
WEBKIT_DEFINE_FINAL_TYPE(WPEViewMock, wpe_view_mock, WPE_TYPE_VIEW, WPEView)

static void wpeViewMockConstructed(GObject* object)
{
    G_OBJECT_CLASS(wpe_view_mock_parent_class)->constructed(object);

    auto* view = WPE_VIEW(object);
    g_signal_connect(view, "notify::toplevel", G_CALLBACK(+[](WPEView* view, GParamSpec*, gpointer) {
        auto* toplevel = wpe_view_get_toplevel(view);
        if (!toplevel) {
            wpe_view_unmap(view);
            return;
        }

        int width;
        int height;
        wpe_toplevel_get_size(toplevel, &width, &height);
        if (width && height)
            wpe_view_resized(view, width, height);

        wpe_view_map(view);
    }), nullptr);
}

static void wpeViewMockDispose(GObject* object)
{
    auto* priv = WPE_VIEW_MOCK(object)->priv;
    g_clear_handle_id(&priv->frameTimerID, g_source_remove);

    G_OBJECT_CLASS(wpe_view_mock_parent_class)->dispose(object);
}

static gboolean wpeViewMockRenderBuffer(WPEView* view, WPEBuffer* buffer, const WPERectangle*, guint, GError**)
{
    auto* priv = WPE_VIEW_MOCK(view)->priv;
    priv->pendingBuffer = buffer;
    priv->frameTimerID = g_timeout_add(1000 / 60, +[](gpointer userData) -> gboolean {
        auto* priv = WPE_VIEW_MOCK(userData)->priv;
        priv->frameTimerID = 0;

        auto* view = WPE_VIEW(userData);
        if (priv->committedBuffer)
            wpe_view_buffer_released(view, priv->committedBuffer.get());
        priv->committedBuffer = WTFMove(priv->pendingBuffer);
        wpe_view_buffer_rendered(view, priv->committedBuffer.get());

        return G_SOURCE_REMOVE;
    }, view);

    return TRUE;
}

static void wpe_view_mock_class_init(WPEViewMockClass* viewMockClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(viewMockClass);
    objectClass->constructed = wpeViewMockConstructed;
    objectClass->dispose = wpeViewMockDispose;

    WPEViewClass* viewClass = WPE_VIEW_CLASS(viewMockClass);
    viewClass->render_buffer = wpeViewMockRenderBuffer;
}
