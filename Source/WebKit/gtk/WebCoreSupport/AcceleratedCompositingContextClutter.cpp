/*
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2012 Igalia, S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "AcceleratedCompositingContext.h"

#if USE(ACCELERATED_COMPOSITING) && USE(CLUTTER)

#include "Frame.h"
#include "FrameView.h"
#include "GraphicsLayer.h"
#include "NotImplemented.h"
#include "webkitwebviewprivate.h"
#include <clutter-gtk/clutter-gtk.h>
#include <clutter/clutter.h>

using namespace WebCore;

namespace WebKit {

AcceleratedCompositingContext::AcceleratedCompositingContext(WebKitWebView* webView)
    : m_webView(webView)
    , m_layerFlushTimerCallbackId(0)
    , m_rootGraphicsLayer(0)
    , m_rootLayerEmbedder(0)
{
}

AcceleratedCompositingContext::~AcceleratedCompositingContext()
{
    if (m_layerFlushTimerCallbackId)
        g_source_remove(m_layerFlushTimerCallbackId);
}

bool AcceleratedCompositingContext::enabled()
{
    return m_rootGraphicsLayer;
}

bool AcceleratedCompositingContext::renderLayersToWindow(cairo_t*, const IntRect& clipRect)
{
    notImplemented();
    return true;
}

void AcceleratedCompositingContext::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    if (!graphicsLayer) {
        gtk_container_remove(GTK_CONTAINER(m_webView), m_rootLayerEmbedder);
        m_rootLayerEmbedder = 0;
        m_rootGraphicsLayer = 0;
        return;
    }

    // Create an instance of GtkClutterEmbed to embed actors as GraphicsLayers.
    if (!m_rootLayerEmbedder) {
        m_rootLayerEmbedder = gtk_clutter_embed_new();
        gtk_container_add(GTK_CONTAINER(m_webView), m_rootLayerEmbedder);

        GtkAllocation allocation;
        gtk_widget_get_allocation(GTK_WIDGET(m_webView), &allocation);
        gtk_widget_size_allocate(GTK_WIDGET(m_rootLayerEmbedder), &allocation);
        gtk_widget_show(m_rootLayerEmbedder);
    }

    // Add a root GraphicsLayer to the stage.
    if (graphicsLayer) {
        m_rootGraphicsLayer = graphicsLayer;
        ClutterColor stageColor = { 0xFF, 0xFF, 0xFF, 0xFF };
        ClutterActor* stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(m_rootLayerEmbedder));
        clutter_stage_set_color(CLUTTER_STAGE(stage), &stageColor);
        clutter_container_add_actor(CLUTTER_CONTAINER(stage), m_rootGraphicsLayer->platformLayer());
        clutter_actor_show_all(stage);
    }

    scheduleLayerFlush();
}

void AcceleratedCompositingContext::setNonCompositedContentsNeedDisplay(const IntRect& rect)
{
    if (!m_rootGraphicsLayer)
        return;

    if (rect.isEmpty()) {
        m_rootGraphicsLayer->setNeedsDisplay();
        return;
    }

    m_rootGraphicsLayer->setNeedsDisplayInRect(rect);
}

void AcceleratedCompositingContext::resizeRootLayer(const IntSize& size)
{
    if (!m_rootLayerEmbedder)
        return;

    GtkAllocation allocation;
    allocation.x = 0;
    allocation.y = 0;
    allocation.width = size.width();
    allocation.height = size.height();
    gtk_widget_size_allocate(GTK_WIDGET(m_rootLayerEmbedder), &allocation);

    scheduleLayerFlush();
}

void AcceleratedCompositingContext::scrollNonCompositedContents(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    notImplemented();
}

gboolean AcceleratedCompositingContext::layerFlushTimerFiredCallback(AcceleratedCompositingContext* context)
{
    context->flushAndRenderLayers();
    return FALSE;
}

void AcceleratedCompositingContext::scheduleLayerFlush()
{
    if (m_layerFlushTimerCallbackId)
        return;

    // We use a GLib timer because otherwise GTK+ event handling during
    // dragging can starve WebCore timers, which have a lower priority.
    m_layerFlushTimerCallbackId = g_timeout_add_full(GDK_PRIORITY_EVENTS, 0, reinterpret_cast<GSourceFunc>(layerFlushTimerFiredCallback), this, 0);
}

bool AcceleratedCompositingContext::flushPendingLayerChanges()
{
    if (m_rootGraphicsLayer)
        m_rootGraphicsLayer->flushCompositingStateForThisLayerOnly();

    return core(m_webView)->mainFrame()->view()->flushCompositingStateIncludingSubframes();
}

void AcceleratedCompositingContext::flushAndRenderLayers()
{
    m_layerFlushTimerCallbackId = 0;
    flushPendingLayerChanges();
    if (!m_rootGraphicsLayer)
        return;

    renderLayersToWindow(0, IntRect());
}

void AcceleratedCompositingContext::notifyAnimationStarted(const WebCore::GraphicsLayer*, double time)
{
    ASSERT_NOT_REACHED();
}
void AcceleratedCompositingContext::notifyFlushRequired(const WebCore::GraphicsLayer*)
{
    ASSERT_NOT_REACHED();
}

void AcceleratedCompositingContext::paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect&)
{
    ASSERT_NOT_REACHED();
}

} // namespace WebKit

#endif // USE(ACCELERATED_COMPOSITING) && USE(CLUTTER)
