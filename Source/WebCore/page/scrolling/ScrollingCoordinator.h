/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef ScrollingCoordinator_h
#define ScrollingCoordinator_h

#include "GraphicsLayer.h"
#include "IntRect.h"
#include "PlatformWheelEvent.h"
#include "ScrollTypes.h"
#include "Timer.h"
#include <wtf/Forward.h>

#if ENABLE(THREADED_SCROLLING)
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>
#endif

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#endif

namespace WebCore {

class FrameView;
class GraphicsLayer;
class Page;
class Region;
class ScrollingCoordinatorPrivate;
class ScrollingTreeState;

#if ENABLE(THREADED_SCROLLING)
class ScrollingTree;
#endif

class ScrollingCoordinator : public ThreadSafeRefCounted<ScrollingCoordinator> {
public:
    static PassRefPtr<ScrollingCoordinator> create(Page*);
    ~ScrollingCoordinator();

    void pageDestroyed();

#if ENABLE(THREADED_SCROLLING)
    ScrollingTree* scrollingTree() const;
    void commitTreeStateIfNeeded();
#endif

    // Return whether this scrolling coordinator handles scrolling for the given frame view.
    bool coordinatesScrollingForFrameView(FrameView*) const;

    // Should be called whenever the given frame view has been laid out.
    void frameViewLayoutUpdated(FrameView*);

    // Should be called whenever a wheel event handler is added or removed in the 
    // frame view's underlying document.
    void frameViewWheelEventHandlerCountChanged(FrameView*);

    // Should be called whenever the slow repaint objects counter changes between zero and one.
    void frameViewHasSlowRepaintObjectsDidChange(FrameView*);

    // Should be called whenever the fixed objects counter changes between zero and one.
    // FIXME: This is a temporary workaround so that we'll fall into main thread scrolling mode
    // if a page has fixed elements. The scrolling tree should know about the fixed elements
    // so it can make sure they stay fixed even when we scroll on the scrolling thread.
    void frameViewHasFixedObjectsDidChange(FrameView*);

    // Should be called whenever the root layer for the given frame view changes.
    void frameViewRootLayerDidChange(FrameView*);

    // Should be called whenever the horizontal scrollbar layer for the given frame view changes.
    void frameViewHorizontalScrollbarLayerDidChange(FrameView*, GraphicsLayer* horizontalScrollbarLayer);

    // Should be called whenever the vertical scrollbar layer for the given frame view changes.
    void frameViewVerticalScrollbarLayerDidChange(FrameView*, GraphicsLayer* verticalScrollbarLayer);

    // Requests that the scrolling coordinator updates the scroll position of the given frame view. If this function returns true, it means that the
    // position will be updated asynchronously. If it returns false, the caller should update the scrolling position itself.
    bool requestScrollPositionUpdate(FrameView*, const IntPoint&);

    // Handle the wheel event on the scrolling thread. Returns whether the event was handled or not.
    bool handleWheelEvent(FrameView*, const PlatformWheelEvent&);

    // Dispatched by the scrolling tree whenever the main frame scroll position changes.
    void updateMainFrameScrollPosition(const IntPoint&);

    // Dispatched by the scrolling tree whenever the main frame scroll position changes and the scroll layer position needs to be updated as well.
    void updateMainFrameScrollPositionAndScrollLayerPosition();

#if PLATFORM(MAC) || (PLATFORM(CHROMIUM) && OS(DARWIN))
    // Dispatched by the scrolling tree during handleWheelEvent. This is required as long as scrollbars are painted on the main thread.
    void handleWheelEventPhase(PlatformWheelEventPhase);
#endif

    // Force all scroll layer position updates to happen on the main thread.
    void setForceMainThreadScrollLayerPositionUpdates(bool);

private:
    explicit ScrollingCoordinator(Page*);

    void recomputeWheelEventHandlerCount();
    void updateShouldUpdateScrollLayerPositionOnMainThread();

    void setScrollLayer(GraphicsLayer*);
    void setNonFastScrollableRegion(const Region&);

    struct ScrollParameters {
        ScrollElasticity horizontalScrollElasticity;
        ScrollElasticity verticalScrollElasticity;

        bool hasEnabledHorizontalScrollbar;
        bool hasEnabledVerticalScrollbar;

        ScrollbarMode horizontalScrollbarMode;
        ScrollbarMode verticalScrollbarMode;

        IntPoint scrollOrigin;

        IntRect viewportRect;
        IntSize contentsSize;
    };

    void setScrollParameters(const ScrollParameters&);
    void setWheelEventHandlerCount(unsigned);
    void setShouldUpdateScrollLayerPositionOnMainThread(bool);

    Page* m_page;

    bool m_forceMainThreadScrollLayerPositionUpdates;

#if ENABLE(THREADED_SCROLLING)
    void scheduleTreeStateCommit();

    void scrollingTreeStateCommitterTimerFired(Timer<ScrollingCoordinator>*);
    void commitTreeState();

    OwnPtr<ScrollingTreeState> m_scrollingTreeState;
    RefPtr<ScrollingTree> m_scrollingTree;
    Timer<ScrollingCoordinator> m_scrollingTreeStateCommitterTimer;
#endif

    ScrollingCoordinatorPrivate* m_private;
};

} // namespace WebCore

#endif // ScrollingCoordinator_h
