/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef ScrollingTreeState_h
#define ScrollingTreeState_h

#if ENABLE(THREADED_SCROLLING)

#include "GraphicsLayer.h"
#include "IntRect.h"
#include "Region.h"
#include "ScrollTypes.h"
#include <wtf/PassOwnPtr.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#endif

namespace WebCore {

// The ScrollingTreeState object keeps track of the current state of scrolling related properties.
// Whenever any properties change, the scrolling coordinator will be informed and will update the state
// and schedule a timer that will clone the new state and send it over to the scrolling thread, avoiding locking.
// FIXME: Once we support fast scrolling in subframes, this will have to become a tree-like structure.
class ScrollingTreeState {
public:
    static PassOwnPtr<ScrollingTreeState> create();
    ~ScrollingTreeState();

    enum ChangedProperty {
        ViewportRect = 1 << 0,
        ContentsSize = 1 << 1,
        NonFastScrollableRegion = 1 << 2,
        WheelEventHandlerCount = 1 << 3,
        ShouldUpdateScrollLayerPositionOnMainThread = 1 << 4,
        HorizontalScrollElasticity = 1 << 5,
        VerticalScrollElasticity = 1 << 6,
        HasEnabledHorizontalScrollbar = 1 << 7,
        HasEnabledVerticalScrollbar = 1 << 8,
        HorizontalScrollbarMode = 1 << 9,
        VerticalScrollbarMode = 1 << 10,
        ScrollOrigin = 1 << 11,
        ScrollLayer = 1 << 12,
        RequestedScrollPosition = 1 << 13,
    };

    bool hasChangedProperties() const { return m_changedProperties; }
    unsigned changedProperties() const { return m_changedProperties; }

    const IntRect& viewportRect() const { return m_viewportRect; }
    void setViewportRect(const IntRect&);

    const IntSize& contentsSize() const { return m_contentsSize; }
    void setContentsSize(const IntSize&);

    const Region& nonFastScrollableRegion() const { return m_nonFastScrollableRegion; }
    void setNonFastScrollableRegion(const Region&);

    unsigned wheelEventHandlerCount() const { return m_wheelEventHandlerCount; }
    void setWheelEventHandlerCount(unsigned);

    bool shouldUpdateScrollLayerPositionOnMainThread() const { return m_shouldUpdateScrollLayerPositionOnMainThread; }
    void setShouldUpdateScrollLayerPositionOnMainThread(bool);

    ScrollElasticity horizontalScrollElasticity() const { return m_horizontalScrollElasticity; }
    void setHorizontalScrollElasticity(ScrollElasticity);

    ScrollElasticity verticalScrollElasticity() const { return m_verticalScrollElasticity; }
    void setVerticalScrollElasticity(ScrollElasticity);

    bool hasEnabledHorizontalScrollbar() const { return m_hasEnabledHorizontalScrollbar; }
    void setHasEnabledHorizontalScrollbar(bool);

    bool hasEnabledVerticalScrollbar() const { return m_hasEnabledVerticalScrollbar; }
    void setHasEnabledVerticalScrollbar(bool);

    ScrollbarMode horizontalScrollbarMode() const { return m_horizontalScrollbarMode; }
    void setHorizontalScrollbarMode(ScrollbarMode);

    ScrollbarMode verticalScrollbarMode() const { return m_verticalScrollbarMode; }
    void setVerticalScrollbarMode(ScrollbarMode);

    PlatformLayer* platformScrollLayer() const;
    void setScrollLayer(const GraphicsLayer*);

    const IntPoint& requestedScrollPosition() const { return m_requestedScrollPosition; }
    void setRequestedScrollPosition(const IntPoint&);

    const IntPoint& scrollOrigin() const { return m_scrollOrigin; }
    void setScrollOrigin(const IntPoint&);

    // Copies the current tree state and clears the changed properties mask in the original.
    PassOwnPtr<ScrollingTreeState> commit();

private:
    ScrollingTreeState();

    unsigned m_changedProperties;

    IntRect m_viewportRect;
    IntSize m_contentsSize;

    Region m_nonFastScrollableRegion;

    unsigned m_wheelEventHandlerCount;

    bool m_shouldUpdateScrollLayerPositionOnMainThread;

    ScrollElasticity m_horizontalScrollElasticity;
    ScrollElasticity m_verticalScrollElasticity;

    bool m_hasEnabledHorizontalScrollbar;
    bool m_hasEnabledVerticalScrollbar;

    ScrollbarMode m_horizontalScrollbarMode;
    ScrollbarMode m_verticalScrollbarMode;

    IntPoint m_requestedScrollPosition;
    IntPoint m_scrollOrigin;

#if PLATFORM(MAC)
    RetainPtr<PlatformLayer> m_platformScrollLayer;
#endif

};

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)

#endif // ScrollingTreeState_h
