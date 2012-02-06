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

#if ENABLE(THREADED_SCROLLING)

#include "GraphicsLayer.h"
#include "IntRect.h"
#include "Timer.h"
#include <wtf/Forward.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#endif

namespace WebCore {

class FrameView;
class GraphicsLayer;
class Page;
class PlatformWheelEvent;
class ScrollingTree;
class ScrollingTreeState;

#if ENABLE(GESTURE_EVENTS)
class PlatformGestureEvent;
#endif

class ScrollingCoordinator : public ThreadSafeRefCounted<ScrollingCoordinator> {
public:
    static PassRefPtr<ScrollingCoordinator> create(Page*);
    ~ScrollingCoordinator();

    void pageDestroyed();

    ScrollingTree* scrollingTree() const;

    // Return whether this scrolling coordinator handles scrolling for the given frame view.
    bool coordinatesScrollingForFrameView(FrameView*) const;

    // Should be called whenever the given frame view has been laid out.
    void frameViewLayoutUpdated(FrameView*);

    // Should be called whenever a wheel event handler is added or removed in the 
    // frame view's underlying document.
    void frameViewWheelEventHandlerCountChanged(FrameView*);

    // Should be called whenever the scroll layer for the given frame view changes.
    void frameViewScrollLayerDidChange(FrameView*, const GraphicsLayer*);

    // Should be called whenever the horizontal scrollbar layer for the given frame view changes.
    void frameViewHorizontalScrollbarLayerDidChange(FrameView*, GraphicsLayer* horizontalScrollbarLayer);

    // Should be called whenever the horizontal scrollbar layer for the given frame view changes.
    void frameViewVerticalScrollbarLayerDidChange(FrameView*, GraphicsLayer* verticalScrollbarLayer);

    // Dispatched by the scrolling tree whenever the main frame scroll position changes.
    void updateMainFrameScrollPosition(const IntPoint&);

private:
    explicit ScrollingCoordinator(Page*);

    void recomputeWheelEventHandlerCount();

    void scheduleTreeStateCommit();
    void scrollingTreeStateCommitterTimerFired(Timer<ScrollingCoordinator>*);
    void commitTreeStateIfNeeded();
    void commitTreeState();

    Page* m_page;
    RefPtr<ScrollingTree> m_scrollingTree;

    OwnPtr<ScrollingTreeState> m_scrollingTreeState;
    Timer<ScrollingCoordinator> m_scrollingTreeStateCommitterTimer;
};

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)

#endif // ScrollingCoordinator_h
