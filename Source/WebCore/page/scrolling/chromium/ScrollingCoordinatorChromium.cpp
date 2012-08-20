/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "ScrollingCoordinator.h"

#include "Frame.h"
#include "FrameView.h"
#include "Page.h"
#include "Region.h"
#include "RenderLayerCompositor.h"
#include "RenderView.h"
#include "ScrollbarThemeComposite.h"
#include "WebScrollbarThemeGeometryNative.h"
#include <public/WebScrollbar.h>
#include <public/WebScrollbarLayer.h>
#include <public/WebScrollbarThemeGeometry.h>
#include <public/WebScrollbarThemePainter.h>

using WebKit::WebLayer;
using WebKit::WebRect;
using WebKit::WebScrollbarLayer;
using WebKit::WebVector;

namespace WebCore {

class ScrollingCoordinatorPrivate {
WTF_MAKE_NONCOPYABLE(ScrollingCoordinatorPrivate);
public:
    ScrollingCoordinatorPrivate()
        : m_scrollLayer(0)
    {
    }

    ~ScrollingCoordinatorPrivate() { }

    void setScrollLayer(WebLayer* layer)
    {
        m_scrollLayer = layer;

        if (m_horizontalScrollbarLayer)
            m_horizontalScrollbarLayer->setScrollLayer(layer);
        if (m_verticalScrollbarLayer)
            m_verticalScrollbarLayer->setScrollLayer(layer);
    }

    void setHorizontalScrollbarLayer(PassOwnPtr<WebScrollbarLayer> layer)
    {
        m_horizontalScrollbarLayer = layer;
    }

    void setVerticalScrollbarLayer(PassOwnPtr<WebScrollbarLayer> layer)
    {
        m_verticalScrollbarLayer = layer;
    }

    WebLayer* scrollLayer() const { return m_scrollLayer; }

private:
    WebLayer* m_scrollLayer;
    OwnPtr<WebScrollbarLayer> m_horizontalScrollbarLayer;
    OwnPtr<WebScrollbarLayer> m_verticalScrollbarLayer;
};

PassRefPtr<ScrollingCoordinator> ScrollingCoordinator::create(Page* page)
{
    RefPtr<ScrollingCoordinator> coordinator(adoptRef(new ScrollingCoordinator(page)));
    coordinator->m_private = new ScrollingCoordinatorPrivate;
    return coordinator.release();
}

ScrollingCoordinator::~ScrollingCoordinator()
{
    ASSERT(!m_page);
    delete m_private;
}

static GraphicsLayer* scrollLayerForFrameView(FrameView* frameView)
{
#if USE(ACCELERATED_COMPOSITING)
    Frame* frame = frameView->frame();
    if (!frame)
        return 0;

    RenderView* renderView = frame->contentRenderer();
    if (!renderView)
        return 0;
    return renderView->compositor()->scrollLayer();
#else
    return 0;
#endif
}

static WebLayer* scrollableLayerForGraphicsLayer(GraphicsLayer* layer)
{
    return layer->platformLayer();
}

static PassOwnPtr<WebScrollbarLayer> createScrollbarLayer(Scrollbar* scrollbar, WebLayer* scrollLayer, GraphicsLayer* scrollbarGraphicsLayer, FrameView* frameView)
{
    ASSERT(scrollbar);
    ASSERT(scrollbarGraphicsLayer);

    if (!scrollLayer) {
        // FIXME: sometimes we get called before setScrollLayer, workaround by finding the scroll layout ourselves.
        scrollLayer = scrollableLayerForGraphicsLayer(scrollLayerForFrameView(frameView));
        ASSERT(scrollLayer);
    }

    // Root layer non-overlay scrollbars should be marked opaque to disable
    // blending.
    bool isOpaqueRootScrollbar = !frameView->parent() && !scrollbar->isOverlayScrollbar();
    if (!scrollbarGraphicsLayer->contentsOpaque())
        scrollbarGraphicsLayer->setContentsOpaque(isOpaqueRootScrollbar);

    // FIXME: Mac scrollbar themes are not thread-safe to paint.
    bool platformSupported = true;
#if OS(DARWIN)
    platformSupported = false;
#endif

    if (!platformSupported || scrollbar->isCustomScrollbar()) {
        scrollbarGraphicsLayer->setContentsToMedia(0);
        scrollbarGraphicsLayer->setDrawsContent(true);
        return nullptr;
    }

    // All Chromium scrollbar themes derive from ScrollbarThemeComposite.
    ScrollbarThemeComposite* themeComposite = static_cast<ScrollbarThemeComposite*>(scrollbar->theme());
    WebKit::WebScrollbarThemePainter painter(themeComposite, scrollbar);
    OwnPtr<WebKit::WebScrollbarThemeGeometry> geometry(WebKit::WebScrollbarThemeGeometryNative::create(themeComposite));

    OwnPtr<WebScrollbarLayer> scrollbarLayer = adoptPtr(WebScrollbarLayer::create(scrollbar, painter, geometry.release()));
    scrollbarLayer->setScrollLayer(scrollLayer);

    scrollbarGraphicsLayer->setContentsToMedia(scrollbarLayer->layer());
    scrollbarGraphicsLayer->setDrawsContent(false);
    scrollbarLayer->layer()->setOpaque(scrollbarGraphicsLayer->contentsOpaque());

    return scrollbarLayer.release();
}

void ScrollingCoordinator::frameViewHorizontalScrollbarLayerDidChange(FrameView* frameView, GraphicsLayer* horizontalScrollbarLayer)
{
    if (!horizontalScrollbarLayer || !coordinatesScrollingForFrameView(frameView))
        return;

    setScrollLayer(scrollLayerForFrameView(m_page->mainFrame()->view()));
    m_private->setHorizontalScrollbarLayer(createScrollbarLayer(frameView->horizontalScrollbar(), m_private->scrollLayer(), horizontalScrollbarLayer, frameView));
}

void ScrollingCoordinator::frameViewVerticalScrollbarLayerDidChange(FrameView* frameView, GraphicsLayer* verticalScrollbarLayer)
{
    if (!verticalScrollbarLayer || !coordinatesScrollingForFrameView(frameView))
        return;

    setScrollLayer(scrollLayerForFrameView(m_page->mainFrame()->view()));
    m_private->setVerticalScrollbarLayer(createScrollbarLayer(frameView->verticalScrollbar(), m_private->scrollLayer(), verticalScrollbarLayer, frameView));
}

void ScrollingCoordinator::setScrollLayer(GraphicsLayer* scrollLayer)
{
    m_private->setScrollLayer(scrollLayer ? scrollableLayerForGraphicsLayer(scrollLayer) : 0);
}

void ScrollingCoordinator::setNonFastScrollableRegion(const Region& region)
{
    // We won't necessarily get a setScrollLayer() call before this one, so grab the root ourselves.
    setScrollLayer(scrollLayerForFrameView(m_page->mainFrame()->view()));
    if (m_private->scrollLayer()) {
        Vector<IntRect> rects = region.rects();
        WebVector<WebRect> webRects(rects.size());
        for (size_t i = 0; i < rects.size(); ++i)
            webRects[i] = rects[i];
        m_private->scrollLayer()->setNonFastScrollableRegion(webRects);
    }
}

void ScrollingCoordinator::setScrollParameters(const ScrollParameters&)
{
    // FIXME: Implement!
}

void ScrollingCoordinator::setWheelEventHandlerCount(unsigned wheelEventHandlerCount)
{
    // We won't necessarily get a setScrollLayer() call before this one, so grab the root ourselves.
    setScrollLayer(scrollLayerForFrameView(m_page->mainFrame()->view()));
    if (m_private->scrollLayer())
        m_private->scrollLayer()->setHaveWheelEventHandlers(wheelEventHandlerCount > 0);
}

void ScrollingCoordinator::setShouldUpdateScrollLayerPositionOnMainThread(bool should)
{
    // We won't necessarily get a setScrollLayer() call before this one, so grab the root ourselves.
    setScrollLayer(scrollLayerForFrameView(m_page->mainFrame()->view()));
    if (m_private->scrollLayer())
        m_private->scrollLayer()->setShouldScrollOnMainThread(should);
}

bool ScrollingCoordinator::supportsFixedPositionLayers() const
{
    return true;
}

void ScrollingCoordinator::setLayerIsContainerForFixedPositionLayers(GraphicsLayer* layer, bool enable)
{
    if (WebLayer* scrollableLayer = scrollableLayerForGraphicsLayer(layer))
        scrollableLayer->setIsContainerForFixedPositionLayers(enable);
}

void ScrollingCoordinator::setLayerIsFixedToContainerLayer(GraphicsLayer* layer, bool enable)
{
    if (WebLayer* scrollableLayer = scrollableLayerForGraphicsLayer(layer))
        scrollableLayer->setFixedToContainerLayer(enable);
}

}
