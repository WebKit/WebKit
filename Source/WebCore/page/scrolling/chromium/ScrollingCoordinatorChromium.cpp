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

#include "ScrollingCoordinatorChromium.h"

#include "Frame.h"
#include "FrameView.h"
#include "GraphicsLayerChromium.h"
#include "Page.h"
#include "Region.h"
#include "RenderLayerCompositor.h"
#include "RenderView.h"
#include "ScrollbarThemeComposite.h"
#include "WebScrollbarImpl.h"
#include "WebScrollbarThemeGeometryNative.h"
#include <public/Platform.h>
#include <public/WebCompositorSupport.h>
#include <public/WebScrollbar.h>
#include <public/WebScrollbarLayer.h>
#include <public/WebScrollbarThemeGeometry.h>
#include <public/WebScrollbarThemePainter.h>

using WebKit::WebLayer;
using WebKit::WebRect;
using WebKit::WebScrollbarLayer;
using WebKit::WebVector;

namespace WebCore {

static WebLayer* scrollingWebLayerForGraphicsLayer(GraphicsLayer* layer)
{
    return layer->platformLayer();
}

WebLayer* ScrollingCoordinatorChromium::scrollingWebLayerForScrollableArea(ScrollableArea* scrollableArea)
{
    GraphicsLayer* graphicsLayer = scrollLayerForScrollableArea(scrollableArea);
    return graphicsLayer ? scrollingWebLayerForGraphicsLayer(graphicsLayer) : 0;
}

ScrollingCoordinatorChromium::ScrollingCoordinatorChromium(Page* page)
    : ScrollingCoordinator(page)
{
}

ScrollingCoordinatorChromium::~ScrollingCoordinatorChromium()
{
    for (ScrollbarMap::iterator it = m_horizontalScrollbars.begin(); it != m_horizontalScrollbars.end(); ++it)
        GraphicsLayerChromium::unregisterContentsLayer(it->value->layer());
    for (ScrollbarMap::iterator it = m_verticalScrollbars.begin(); it != m_verticalScrollbars.end(); ++it)
        GraphicsLayerChromium::unregisterContentsLayer(it->value->layer());
}

void ScrollingCoordinatorChromium::frameViewLayoutUpdated(FrameView* frameView)
{
    ASSERT(m_page);

    // Compute the region of the page that we can't do fast scrolling for. This currently includes
    // all scrollable areas, such as subframes, overflow divs and list boxes, whose composited
    // scrolling are not enabled. We need to do this even if the frame view whose layout was updated
    // is not the main frame.
    Region nonFastScrollableRegion = computeNonFastScrollableRegion(m_page->mainFrame(), IntPoint());
    setNonFastScrollableRegion(nonFastScrollableRegion);
#if ENABLE(TOUCH_EVENT_TRACKING)
    Vector<IntRect> touchEventTargetRects;
    computeAbsoluteTouchEventTargetRects(m_page->mainFrame()->document(), touchEventTargetRects);
    setTouchEventTargetRects(touchEventTargetRects);
#endif
    if (WebLayer* scrollLayer = scrollingWebLayerForScrollableArea(frameView))
        scrollLayer->setBounds(frameView->contentsSize());
}

void ScrollingCoordinatorChromium::touchEventTargetRectsDidChange(const Document*)
{
#if ENABLE(TOUCH_EVENT_TRACKING)
    // The rects are always evaluated and used in the main frame coordinates.
    FrameView* frameView = m_page->mainFrame()->view();
    Document* document = m_page->mainFrame()->document();

    // Wait until after layout to update.
    if (frameView->needsLayout() || !document)
        return;

    Vector<IntRect> touchEventTargetRects;
    computeAbsoluteTouchEventTargetRects(document, touchEventTargetRects);
    setTouchEventTargetRects(touchEventTargetRects);
#endif
}

static PassOwnPtr<WebScrollbarLayer> createScrollbarLayer(Scrollbar* scrollbar)
{
    // All Chromium scrollbar themes derive from ScrollbarThemeComposite.
    ScrollbarThemeComposite* themeComposite = static_cast<ScrollbarThemeComposite*>(scrollbar->theme());
    WebKit::WebScrollbarThemePainter painter(themeComposite, scrollbar);
    OwnPtr<WebKit::WebScrollbarThemeGeometry> geometry(WebKit::WebScrollbarThemeGeometryNative::create(themeComposite));

    OwnPtr<WebScrollbarLayer> scrollbarLayer = adoptPtr(WebKit::Platform::current()->compositorSupport()->createScrollbarLayer(new WebKit::WebScrollbarImpl(scrollbar), painter, geometry.leakPtr()));
    GraphicsLayerChromium::registerContentsLayer(scrollbarLayer->layer());
    return scrollbarLayer.release();
}

static void detachScrollbarLayer(GraphicsLayer* scrollbarGraphicsLayer)
{
    ASSERT(scrollbarGraphicsLayer);

    scrollbarGraphicsLayer->setContentsToPlatformLayer(0);
    scrollbarGraphicsLayer->setDrawsContent(true);
}

static void setupScrollbarLayer(GraphicsLayer* scrollbarGraphicsLayer, WebScrollbarLayer* scrollbarLayer, WebLayer* scrollLayer)
{
    ASSERT(scrollbarGraphicsLayer);
    ASSERT(scrollbarLayer);

    if (!scrollLayer) {
        detachScrollbarLayer(scrollbarGraphicsLayer);
        return;
    }
    scrollbarLayer->setScrollLayer(scrollLayer);
    scrollbarGraphicsLayer->setContentsToPlatformLayer(scrollbarLayer->layer());
    scrollbarGraphicsLayer->setDrawsContent(false);
}

void ScrollingCoordinatorChromium::willDestroyScrollableArea(ScrollableArea* scrollableArea)
{
    removeWebScrollbarLayer(scrollableArea, HorizontalScrollbar);
    removeWebScrollbarLayer(scrollableArea, VerticalScrollbar);
}

void ScrollingCoordinatorChromium::scrollableAreaScrollbarLayerDidChange(ScrollableArea* scrollableArea, ScrollbarOrientation orientation)
{
#if OS(DARWIN)
    static const bool platformSupportsCoordinatedScrollbar = false;
    static const bool platformSupportsMainFrameOnly = false; // Don't care.
#elif OS(WINDOWS)
    static const bool platformSupportsCoordinatedScrollbar = true;
    static const bool platformSupportsMainFrameOnly = true;
#else
    static const bool platformSupportsCoordinatedScrollbar = true;
    static const bool platformSupportsMainFrameOnly = false;
#endif
    if (!platformSupportsCoordinatedScrollbar)
        return;

    bool isMainFrame = (scrollableArea == static_cast<ScrollableArea*>(m_page->mainFrame()->view()));
    if (!isMainFrame && platformSupportsMainFrameOnly)
        return;

    GraphicsLayer* scrollbarGraphicsLayer = orientation == HorizontalScrollbar ? horizontalScrollbarLayerForScrollableArea(scrollableArea) : verticalScrollbarLayerForScrollableArea(scrollableArea);
    if (scrollbarGraphicsLayer) {
        Scrollbar* scrollbar = orientation == HorizontalScrollbar ? scrollableArea->horizontalScrollbar() : scrollableArea->verticalScrollbar();
        if (scrollbar->isCustomScrollbar()) {
            detachScrollbarLayer(scrollbarGraphicsLayer);
            return;
        }

        WebScrollbarLayer* scrollbarLayer = getWebScrollbarLayer(scrollableArea, orientation);
        if (!scrollbarLayer)
            scrollbarLayer = addWebScrollbarLayer(scrollableArea, orientation, createScrollbarLayer(scrollbar));

        // Root layer non-overlay scrollbars should be marked opaque to disable
        // blending.
        bool isOpaqueScrollbar = !scrollbar->isOverlayScrollbar();
        if (!scrollbarGraphicsLayer->contentsOpaque())
            scrollbarGraphicsLayer->setContentsOpaque(isMainFrame && isOpaqueScrollbar);
        scrollbarLayer->layer()->setOpaque(scrollbarGraphicsLayer->contentsOpaque());

        setupScrollbarLayer(scrollbarGraphicsLayer, scrollbarLayer, scrollingWebLayerForScrollableArea(scrollableArea));
    } else
        removeWebScrollbarLayer(scrollableArea, orientation);
}

void ScrollingCoordinatorChromium::setNonFastScrollableRegion(const Region& region)
{
    if (WebLayer* scrollLayer = scrollingWebLayerForScrollableArea(m_page->mainFrame()->view())) {
        Vector<IntRect> rects = region.rects();
        WebVector<WebRect> webRects(rects.size());
        for (size_t i = 0; i < rects.size(); ++i)
            webRects[i] = rects[i];
        scrollLayer->setNonFastScrollableRegion(webRects);
    }
}

void ScrollingCoordinatorChromium::setTouchEventTargetRects(const Vector<IntRect>& absoluteHitTestRects)
{
    if (WebLayer* scrollLayer = scrollingWebLayerForScrollableArea(m_page->mainFrame()->view())) {
        WebVector<WebRect> webRects(absoluteHitTestRects.size());
        for (size_t i = 0; i < absoluteHitTestRects.size(); ++i)
            webRects[i] = absoluteHitTestRects[i];
        scrollLayer->setTouchEventHandlerRegion(webRects);
    }
}

void ScrollingCoordinatorChromium::setWheelEventHandlerCount(unsigned wheelEventHandlerCount)
{
    if (WebLayer* scrollLayer = scrollingWebLayerForScrollableArea(m_page->mainFrame()->view()))
        scrollLayer->setHaveWheelEventHandlers(wheelEventHandlerCount > 0);
}

void ScrollingCoordinatorChromium::setShouldUpdateScrollLayerPositionOnMainThread(MainThreadScrollingReasons reasons)
{
    if (WebLayer* scrollLayer = scrollingWebLayerForScrollableArea(m_page->mainFrame()->view()))
        scrollLayer->setShouldScrollOnMainThread(reasons);
}

void ScrollingCoordinatorChromium::setLayerIsContainerForFixedPositionLayers(GraphicsLayer* layer, bool enable)
{
    if (WebLayer* scrollableLayer = scrollingWebLayerForGraphicsLayer(layer))
        scrollableLayer->setIsContainerForFixedPositionLayers(enable);
}

void ScrollingCoordinatorChromium::setLayerIsFixedToContainerLayer(GraphicsLayer* layer, bool enable)
{
    if (WebLayer* scrollableLayer = scrollingWebLayerForGraphicsLayer(layer))
        scrollableLayer->setFixedToContainerLayer(enable);
}

void ScrollingCoordinatorChromium::scrollableAreaScrollLayerDidChange(ScrollableArea* scrollableArea)
{
    GraphicsLayerChromium* scrollLayer = static_cast<GraphicsLayerChromium*>(scrollLayerForScrollableArea(scrollableArea));
    if (scrollLayer)
        scrollLayer->setScrollableArea(scrollableArea);

    WebLayer* webLayer = scrollingWebLayerForScrollableArea(scrollableArea);
    if (webLayer) {
        webLayer->setScrollable(true);
        webLayer->setScrollPosition(scrollableArea->scrollPosition());
        webLayer->setMaxScrollPosition(IntSize(scrollableArea->scrollSize(HorizontalScrollbar), scrollableArea->scrollSize(VerticalScrollbar)));
    }
    if (WebScrollbarLayer* scrollbarLayer = getWebScrollbarLayer(scrollableArea, HorizontalScrollbar))
        setupScrollbarLayer(horizontalScrollbarLayerForScrollableArea(scrollableArea), scrollbarLayer, webLayer);
    if (WebScrollbarLayer* scrollbarLayer = getWebScrollbarLayer(scrollableArea, VerticalScrollbar))
        setupScrollbarLayer(verticalScrollbarLayerForScrollableArea(scrollableArea), scrollbarLayer, webLayer);
}

void ScrollingCoordinatorChromium::recomputeWheelEventHandlerCountForFrameView(FrameView* frameView)
{
    UNUSED_PARAM(frameView);
    setWheelEventHandlerCount(computeCurrentWheelEventHandlerCount());
}

WebScrollbarLayer* ScrollingCoordinatorChromium::addWebScrollbarLayer(ScrollableArea* scrollableArea, ScrollbarOrientation orientation, PassOwnPtr<WebKit::WebScrollbarLayer> scrollbarLayer)
{
    ScrollbarMap& scrollbars = orientation == HorizontalScrollbar ? m_horizontalScrollbars : m_verticalScrollbars;
    return scrollbars.add(scrollableArea, scrollbarLayer).iterator->value.get();
}

WebScrollbarLayer* ScrollingCoordinatorChromium::getWebScrollbarLayer(ScrollableArea* scrollableArea, ScrollbarOrientation orientation)
{
    ScrollbarMap& scrollbars = orientation == HorizontalScrollbar ? m_horizontalScrollbars : m_verticalScrollbars;
    return scrollbars.get(scrollableArea);
}

void ScrollingCoordinatorChromium::removeWebScrollbarLayer(ScrollableArea* scrollableArea, ScrollbarOrientation orientation)
{
    ScrollbarMap& scrollbars = orientation == HorizontalScrollbar ? m_horizontalScrollbars : m_verticalScrollbars;
    if (OwnPtr<WebScrollbarLayer> scrollbarLayer = scrollbars.take(scrollableArea))
        GraphicsLayerChromium::unregisterContentsLayer(scrollbarLayer->layer());
}

}
