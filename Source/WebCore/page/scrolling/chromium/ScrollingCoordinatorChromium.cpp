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
#include "LayerChromium.h"
#include "Region.h"
#include "RenderLayerCompositor.h"
#include "RenderView.h"
#include "ScrollbarLayerChromium.h"
#include "ScrollbarTheme.h"
#include "cc/CCProxy.h"

namespace WebCore {

class ScrollingCoordinatorPrivate {
WTF_MAKE_NONCOPYABLE(ScrollingCoordinatorPrivate);
public:
    ScrollingCoordinatorPrivate() { }
    ~ScrollingCoordinatorPrivate() { }

    void setScrollLayer(LayerChromium* layer) { m_scrollLayer = layer; }
    LayerChromium* scrollLayer() const { return m_scrollLayer.get(); }

private:
    RefPtr<LayerChromium> m_scrollLayer;
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

static void scrollbarLayerDidChange(Scrollbar* scrollbar, LayerChromium* scrollLayer, GraphicsLayer* scrollbarGraphicsLayer, FrameView* frameView)
{
    ASSERT(scrollbar);
    ASSERT(scrollbarGraphicsLayer);

    if (!scrollLayer) {
        // FIXME: sometimes we get called before setScrollLayer, workaround by finding the scroll layout ourselves.
        scrollLayer = scrollLayerForFrameView(frameView)->platformLayer();
        ASSERT(scrollLayer);
    }

    // Root layer non-overlay scrollbars should be marked opaque to disable
    // blending.
    bool isOpaqueRootScrollbar = !frameView->parent() && !scrollbar->isOverlayScrollbar();
    if (!scrollbarGraphicsLayer->contentsOpaque())
        scrollbarGraphicsLayer->setContentsOpaque(isOpaqueRootScrollbar);

    // Only certain platforms support the way that scrollbars are currently
    // being painted on the impl thread. For example, Cocoa is not threadsafe.
    bool platformSupported = false;
#if OS(LINUX)
    platformSupported = true;
#endif

    if (scrollbar->isCustomScrollbar() || !CCProxy::hasImplThread() || !platformSupported) {
        scrollbarGraphicsLayer->setContentsToMedia(0);
        scrollbarGraphicsLayer->setDrawsContent(true);
        return;
    }

    RefPtr<ScrollbarLayerChromium> scrollbarLayer = ScrollbarLayerChromium::create(scrollbar, scrollLayer->id());
    scrollbarGraphicsLayer->setContentsToMedia(scrollbarLayer.get());
    scrollbarGraphicsLayer->setDrawsContent(false);
    scrollbarLayer->setOpaque(scrollbarGraphicsLayer->contentsOpaque());
}

void ScrollingCoordinator::frameViewHorizontalScrollbarLayerDidChange(FrameView* frameView, GraphicsLayer* horizontalScrollbarLayer)
{
    if (!horizontalScrollbarLayer || !coordinatesScrollingForFrameView(frameView))
        return;

    scrollbarLayerDidChange(frameView->horizontalScrollbar(), m_private->scrollLayer(), horizontalScrollbarLayer, frameView);
}

void ScrollingCoordinator::frameViewVerticalScrollbarLayerDidChange(FrameView* frameView, GraphicsLayer* verticalScrollbarLayer)
{
    if (!verticalScrollbarLayer || !coordinatesScrollingForFrameView(frameView))
        return;

    scrollbarLayerDidChange(frameView->verticalScrollbar(), m_private->scrollLayer(), verticalScrollbarLayer, frameView);
}

void ScrollingCoordinator::setScrollLayer(GraphicsLayer* scrollLayer)
{
    m_private->setScrollLayer(scrollLayer ? scrollLayer->platformLayer() : 0);
}

void ScrollingCoordinator::setNonFastScrollableRegion(const Region& region)
{
    if (LayerChromium* layer = m_private->scrollLayer())
        layer->setNonFastScrollableRegion(region);
}

void ScrollingCoordinator::setScrollParameters(const ScrollParameters&)
{
    // FIXME: Implement!
}

void ScrollingCoordinator::setWheelEventHandlerCount(unsigned wheelEventHandlerCount)
{
    if (LayerChromium* layer = m_private->scrollLayer())
        layer->setHaveWheelEventHandlers(wheelEventHandlerCount > 0);
}

void ScrollingCoordinator::setShouldUpdateScrollLayerPositionOnMainThread(bool should)
{
    if (LayerChromium* layer = m_private->scrollLayer())
        layer->setShouldScrollOnMainThread(should);
}

}
