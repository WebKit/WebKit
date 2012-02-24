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

#include "LayerChromium.h"
#include "Region.h"

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

void ScrollingCoordinator::frameViewHorizontalScrollbarLayerDidChange(FrameView*, GraphicsLayer* horizontalScrollbarLayer)
{
    // FIXME: Implement!
}

void ScrollingCoordinator::frameViewVerticalScrollbarLayerDidChange(FrameView*, GraphicsLayer* verticalScrollbarLayer)
{
    // FIXME: Implement!
}

void ScrollingCoordinator::setScrollLayer(GraphicsLayer* scrollLayer)
{
    m_private->setScrollLayer(scrollLayer ? scrollLayer->platformLayer() : 0);
}

void ScrollingCoordinator::setNonFastScrollableRegion(const Region&)
{
    // FIXME: Implement!
}

void ScrollingCoordinator::setScrollParameters(ScrollElasticity horizontalScrollElasticity, ScrollElasticity verticalScrollElasticity,
                                               bool hasEnabledHorizontalScrollbar, bool hasEnabledVerticalScrollbar,
                                               const IntRect& viewportRect, const IntSize& contentsSize)
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
    // FIXME: Implement!
}

}
