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
#include "WPEViewHeadless.h"

#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/glib/WTFGType.h>

/**
 * WPEViewHeadless:
 *
 */
struct _WPEViewHeadlessPrivate {
    GRefPtr<WPEBuffer> buffer;
    GRefPtr<GSource> frameSource;
    gint64 lastFrameTime;
};
WEBKIT_DEFINE_FINAL_TYPE(WPEViewHeadless, wpe_view_headless, WPE_TYPE_VIEW, WPEView)

static GSourceFuncs frameSourceFuncs = {
    nullptr, // prepare
    nullptr, // check
    // dispatch
    [](GSource* source, GSourceFunc callback, gpointer userData) -> gboolean
    {
        if (g_source_get_ready_time(source) == -1)
            return G_SOURCE_CONTINUE;
        g_source_set_ready_time(source, -1);
        return callback(userData);
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

static void wpeViewHeadlessConstructed(GObject* object)
{
    G_OBJECT_CLASS(wpe_view_headless_parent_class)->constructed(object);

    auto* priv = WPE_VIEW_HEADLESS(object)->priv;
    priv->frameSource = adoptGRef(g_source_new(&frameSourceFuncs, sizeof(GSource)));
    g_source_set_priority(priv->frameSource.get(), RunLoopSourcePriority::RunLoopTimer);
    g_source_set_name(priv->frameSource.get(), "WPE headless frame timer");
    g_source_set_callback(priv->frameSource.get(), [](gpointer userData) -> gboolean {
        auto* view = WPE_VIEW(userData);
        auto* priv = WPE_VIEW_HEADLESS(view)->priv;
        wpe_view_buffer_rendered(view, priv->buffer.get());
        priv->buffer = nullptr;
        if (g_source_is_destroyed(priv->frameSource.get()))
            return G_SOURCE_REMOVE;
        return G_SOURCE_CONTINUE;
    }, object, nullptr);
    g_source_attach(priv->frameSource.get(), g_main_context_get_thread_default());
    g_source_set_ready_time(priv->frameSource.get(), -1);
}

static void wpeViewHeadlessDispose(GObject* object)
{
    auto* priv = WPE_VIEW_HEADLESS(object)->priv;

    if (priv->frameSource) {
        g_source_destroy(priv->frameSource.get());
        priv->frameSource = nullptr;
    }

    G_OBJECT_CLASS(wpe_view_headless_parent_class)->dispose(object);
}

static gboolean wpeViewHeadlessRenderBuffer(WPEView* view, WPEBuffer* buffer, GError**)
{
    auto* priv = WPE_VIEW_HEADLESS(view)->priv;
    priv->buffer = buffer;
    auto now = g_get_monotonic_time();
    if (!priv->lastFrameTime)
        priv->lastFrameTime = now;
    auto next = priv->lastFrameTime + (G_USEC_PER_SEC / 60);
    priv->lastFrameTime = now;
    if (next <= now)
        g_source_set_ready_time(priv->frameSource.get(), 0);
    else
        g_source_set_ready_time(priv->frameSource.get(), next);

    return TRUE;
}

static gboolean wpeViewHeadlessSetFullscreen(WPEView* view, gboolean fullscreen)
{
    auto state = wpe_view_get_state(view);
    if (fullscreen)
        state = static_cast<WPEViewState>(state | WPE_VIEW_STATE_FULLSCREEN);
    else
        state = static_cast<WPEViewState>(state & ~WPE_VIEW_STATE_FULLSCREEN);
    wpe_view_set_state(view, state);

    return TRUE;
}


static void wpe_view_headless_class_init(WPEViewHeadlessClass* viewHeadlessClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(viewHeadlessClass);
    objectClass->constructed = wpeViewHeadlessConstructed;
    objectClass->dispose = wpeViewHeadlessDispose;

    WPEViewClass* viewClass = WPE_VIEW_CLASS(viewHeadlessClass);
    viewClass->render_buffer = wpeViewHeadlessRenderBuffer;
    viewClass->set_fullscreen = wpeViewHeadlessSetFullscreen;
}

/**
 * wpe_view_headless_new:
 * @display: a #WPEDisplayHeadless
 *
 * Create a new #WPEViewHeadless
 *
 * Returns: (transfer full): a #WPEView
 */
WPEView* wpe_view_headless_new(WPEDisplayHeadless* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY_HEADLESS(display), nullptr);

    return WPE_VIEW(g_object_new(WPE_TYPE_VIEW_HEADLESS, "display", display, nullptr));
}
