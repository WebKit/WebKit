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

#ifndef ScrollingStateScrollingNode_h
#define ScrollingStateScrollingNode_h

#if ENABLE(THREADED_SCROLLING)

#include "IntRect.h"
#include "Region.h"
#include "ScrollTypes.h"
#include "ScrollingCoordinator.h"
#include "ScrollingStateNode.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class ScrollingStateScrollingNode : public ScrollingStateNode {
public:
    static PassOwnPtr<ScrollingStateScrollingNode> create(ScrollingStateTree*, ScrollingNodeID);

    virtual PassOwnPtr<ScrollingStateNode> clone();

    virtual ~ScrollingStateScrollingNode();

    enum ChangedProperty {
        ViewportRect = 1 << 0,
        ContentsSize = 1 << 1,
        FrameScaleFactor = 1 << 2,
        NonFastScrollableRegion = 1 << 3,
        WheelEventHandlerCount = 1 << 4,
        ShouldUpdateScrollLayerPositionOnMainThread = 1 << 5,
        HorizontalScrollElasticity = 1 << 6,
        VerticalScrollElasticity = 1 << 7,
        HasEnabledHorizontalScrollbar = 1 << 8,
        HasEnabledVerticalScrollbar = 1 << 9,
        HorizontalScrollbarMode = 1 << 10,
        VerticalScrollbarMode = 1 << 11,
        ScrollOrigin = 1 << 12,
        RequestedScrollPosition = 1 << 13,
    };

    virtual bool isScrollingNode() OVERRIDE { return true; }

    virtual bool hasChangedProperties() const OVERRIDE { return m_changedProperties; }
    virtual unsigned changedProperties() const OVERRIDE { return m_changedProperties; }
    virtual void resetChangedProperties() OVERRIDE { m_changedProperties = 0; }

    const IntRect& viewportRect() const { return m_viewportRect; }
    void setViewportRect(const IntRect&);

    const IntSize& contentsSize() const { return m_contentsSize; }
    void setContentsSize(const IntSize&);

    float frameScaleFactor() const { return m_frameScaleFactor; }
    void setFrameScaleFactor(float);

    const Region& nonFastScrollableRegion() const { return m_nonFastScrollableRegion; }
    void setNonFastScrollableRegion(const Region&);

    unsigned wheelEventHandlerCount() const { return m_wheelEventHandlerCount; }
    void setWheelEventHandlerCount(unsigned);

    MainThreadScrollingReasons shouldUpdateScrollLayerPositionOnMainThread() const { return m_shouldUpdateScrollLayerPositionOnMainThread; }
    void setShouldUpdateScrollLayerPositionOnMainThread(MainThreadScrollingReasons);

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

    const IntPoint& requestedScrollPosition() const { return m_requestedScrollPosition; }
    void setRequestedScrollPosition(const IntPoint&, bool representsProgrammaticScroll);

    const IntPoint& scrollOrigin() const { return m_scrollOrigin; }
    void setScrollOrigin(const IntPoint&);

    bool requestedScrollPositionRepresentsProgrammaticScroll() const { return m_requestedScrollPositionRepresentsProgrammaticScroll; }

    virtual void dumpProperties(TextStream&, int indent) const OVERRIDE;

private:
    ScrollingStateScrollingNode(ScrollingStateTree*, ScrollingNodeID);
    ScrollingStateScrollingNode(const ScrollingStateScrollingNode&);

    unsigned m_changedProperties;

    IntRect m_viewportRect;
    IntSize m_contentsSize;
    
    float m_frameScaleFactor;

    Region m_nonFastScrollableRegion;

    unsigned m_wheelEventHandlerCount;

    MainThreadScrollingReasons m_shouldUpdateScrollLayerPositionOnMainThread;

    ScrollElasticity m_horizontalScrollElasticity;
    ScrollElasticity m_verticalScrollElasticity;

    bool m_hasEnabledHorizontalScrollbar;
    bool m_hasEnabledVerticalScrollbar;
    bool m_requestedScrollPositionRepresentsProgrammaticScroll;

    ScrollbarMode m_horizontalScrollbarMode;
    ScrollbarMode m_verticalScrollbarMode;

    IntPoint m_requestedScrollPosition;
    IntPoint m_scrollOrigin;
};

inline ScrollingStateScrollingNode* toScrollingStateScrollingNode(ScrollingStateNode* node)
{
    ASSERT(!node || node->isScrollingNode());
    return static_cast<ScrollingStateScrollingNode*>(node);
}
    
// This will catch anyone doing an unnecessary cast.
void toScrollingStateScrollingNode(const ScrollingStateScrollingNode*);

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)

#endif // ScrollingStateScrollingNode_h
