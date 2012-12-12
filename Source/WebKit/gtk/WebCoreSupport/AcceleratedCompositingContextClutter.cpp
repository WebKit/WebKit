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
#include "GraphicsLayerActor.h"
#include "NotImplemented.h"
#include "Settings.h"
#include "webkitwebviewprivate.h"
#include <clutter-gtk/clutter-gtk.h>
#include <clutter/clutter.h>

using namespace WebCore;

namespace WebKit {

AcceleratedCompositingContext::AcceleratedCompositingContext(WebKitWebView* webView)
    : m_webView(webView)
    , m_layerFlushTimerCallbackId(0)
    , m_rootLayerEmbedder(0)
{
}

void AcceleratedCompositingContext::initialize()
{
    if (m_rootLayer)
        return;

    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(m_webView), &allocation);
    IntSize pageSize(allocation.width, allocation.height);

    m_rootLayer = GraphicsLayer::create(0, this);
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setSize(pageSize);

    // The non-composited contents are a child of the root layer.
    m_nonCompositedContentLayer = GraphicsLayer::create(0, this);
    m_nonCompositedContentLayer->setDrawsContent(true);
    m_nonCompositedContentLayer->setContentsOpaque(!m_webView->priv->transparent);
    m_nonCompositedContentLayer->setSize(pageSize);
    if (core(m_webView)->settings()->acceleratedDrawingEnabled())
        m_nonCompositedContentLayer->setAcceleratesDrawing(true);

#ifndef NDEBUG
    m_rootLayer->setName("Root layer");
    m_nonCompositedContentLayer->setName("Non-composited content");
#endif

    m_rootLayer->addChild(m_nonCompositedContentLayer.get());
    m_nonCompositedContentLayer->setNeedsDisplay();

    scheduleLayerFlush();
}

AcceleratedCompositingContext::~AcceleratedCompositingContext()
{
    if (m_layerFlushTimerCallbackId)
        g_source_remove(m_layerFlushTimerCallbackId);
}

bool AcceleratedCompositingContext::enabled()
{
    return m_rootLayer;
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
        m_rootLayer = nullptr;
        m_nonCompositedContentLayer = nullptr;
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

    // Add the accelerated layer tree hierarchy.
    initialize();

    m_nonCompositedContentLayer->removeAllChildren();
    m_nonCompositedContentLayer->addChild(graphicsLayer);

    // Add a root GraphicsLayer to the stage.
    if (graphicsLayer) {
        ClutterColor stageColor = { 0xFF, 0xFF, 0xFF, 0xFF };
        ClutterActor* stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(m_rootLayerEmbedder));
        clutter_stage_set_color(CLUTTER_STAGE(stage), &stageColor);
        clutter_container_add_actor(CLUTTER_CONTAINER(stage), m_rootLayer->platformLayer());
        clutter_actor_show_all(stage);
    }

    scheduleLayerFlush();
}

void AcceleratedCompositingContext::setNonCompositedContentsNeedDisplay(const IntRect& rect)
{
    if (!m_rootLayer)
        return;

    if (rect.isEmpty()) {
        m_rootLayer->setNeedsDisplay();
        return;
    }

    m_nonCompositedContentLayer->setNeedsDisplayInRect(rect);
    scheduleLayerFlush();
}

void AcceleratedCompositingContext::resizeRootLayer(const IntSize& newSize)
{
    if (!m_rootLayerEmbedder)
        return;

    if (m_rootLayer->size() == newSize)
        return;

    GtkAllocation allocation;
    allocation.x = 0;
    allocation.y = 0;
    allocation.width = newSize.width();
    allocation.height = newSize.height();
    gtk_widget_size_allocate(GTK_WIDGET(m_rootLayerEmbedder), &allocation);

    m_rootLayer->setSize(newSize);

    // If the newSize exposes new areas of the non-composited content a setNeedsDisplay is needed
    // for those newly exposed areas.
    FloatSize oldSize = m_nonCompositedContentLayer->size();
    m_nonCompositedContentLayer->setSize(newSize);

    if (newSize.width() > oldSize.width()) {
        float height = std::min(static_cast<float>(newSize.height()), oldSize.height());
        m_nonCompositedContentLayer->setNeedsDisplayInRect(FloatRect(oldSize.width(), 0, newSize.width() - oldSize.width(), height));
    }

    if (newSize.height() > oldSize.height())
        m_nonCompositedContentLayer->setNeedsDisplayInRect(FloatRect(0, oldSize.height(), newSize.width(), newSize.height() - oldSize.height()));

    m_nonCompositedContentLayer->setNeedsDisplayInRect(IntRect(IntPoint(), newSize));
    scheduleLayerFlush();
}

void AcceleratedCompositingContext::scrollNonCompositedContents(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    m_nonCompositedContentLayer->setNeedsDisplayInRect(scrollRect);
    scheduleLayerFlush();
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
    if (m_rootLayer) {
        m_rootLayer->flushCompositingStateForThisLayerOnly();
        m_nonCompositedContentLayer->flushCompositingStateForThisLayerOnly();
    }

    return core(m_webView)->mainFrame()->view()->flushCompositingStateIncludingSubframes();
}

void AcceleratedCompositingContext::flushAndRenderLayers()
{
    m_layerFlushTimerCallbackId = 0;

    if (!enabled())
        return;

    Frame* frame = core(m_webView)->mainFrame();
    if (!frame || !frame->contentRenderer() || !frame->view())
        return;

    if (!flushPendingLayerChanges())
        return;
}

void AcceleratedCompositingContext::notifyAnimationStarted(const WebCore::GraphicsLayer*, double time)
{
    ASSERT_NOT_REACHED();
}
void AcceleratedCompositingContext::notifyFlushRequired(const WebCore::GraphicsLayer*)
{
    ASSERT_NOT_REACHED();
}

void AcceleratedCompositingContext::paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext& context, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& rectToPaint)
{
    context.save();
    context.clip(rectToPaint);
    core(m_webView)->mainFrame()->view()->paint(&context, rectToPaint);
    context.restore();
}

} // namespace WebKit

#endif // USE(ACCELERATED_COMPOSITING) && USE(CLUTTER)
