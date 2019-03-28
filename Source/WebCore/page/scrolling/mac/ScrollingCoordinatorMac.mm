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

#include "config.h"

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)

#import "ScrollingCoordinatorMac.h"

#include "Frame.h"
#include "FrameView.h"
#include "Logging.h"
#include "Page.h"
#include "PlatformWheelEvent.h"
#include "Region.h"
#include "ScrollingStateTree.h"
#include "ScrollingThread.h"
#include "ScrollingTreeFixedNode.h"
#include "ScrollingTreeFrameScrollingNodeMac.h"
#include "ScrollingTreeMac.h"
#include "ScrollingTreeStickyNode.h"
#include "TiledBacking.h"
#include <wtf/MainThread.h>

namespace WebCore {

Ref<ScrollingCoordinator> ScrollingCoordinator::create(Page* page)
{
    return adoptRef(*new ScrollingCoordinatorMac(page));
}

ScrollingCoordinatorMac::ScrollingCoordinatorMac(Page* page)
    : AsyncScrollingCoordinator(page)
    , m_scrollingStateTreeCommitterTimer(*this, &ScrollingCoordinatorMac::commitTreeState)
{
    setScrollingTree(ScrollingTreeMac::create(*this));
}

ScrollingCoordinatorMac::~ScrollingCoordinatorMac()
{
    ASSERT(!scrollingTree());
}

void ScrollingCoordinatorMac::pageDestroyed()
{
    AsyncScrollingCoordinator::pageDestroyed();

    m_scrollingStateTreeCommitterTimer.stop();

    // Invalidating the scrolling tree will break the reference cycle between the ScrollingCoordinator and ScrollingTree objects.
    RefPtr<ThreadedScrollingTree> scrollingTree = static_pointer_cast<ThreadedScrollingTree>(releaseScrollingTree());
    ScrollingThread::dispatch([scrollingTree] {
        scrollingTree->invalidate();
    });
}

void ScrollingCoordinatorMac::commitTreeStateIfNeeded()
{
    commitTreeState();
    m_scrollingStateTreeCommitterTimer.stop();
}

ScrollingEventResult ScrollingCoordinatorMac::handleWheelEvent(FrameView&, const PlatformWheelEvent& wheelEvent)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (scrollingTree()->willWheelEventStartSwipeGesture(wheelEvent))
        return ScrollingEventResult::DidNotHandleEvent;

    RefPtr<ThreadedScrollingTree> threadedScrollingTree = downcast<ThreadedScrollingTree>(scrollingTree());
    ScrollingThread::dispatch([threadedScrollingTree, wheelEvent] {
        threadedScrollingTree->handleWheelEvent(wheelEvent);
    });
    return ScrollingEventResult::DidHandleEvent;
}

void ScrollingCoordinatorMac::scheduleTreeStateCommit()
{
    ASSERT(scrollingStateTree()->hasChangedProperties() || eventTrackingRegionsDirty());

    if (m_scrollingStateTreeCommitterTimer.isActive())
        return;

    LOG(Scrolling, "ScrollingCoordinatorMac::scheduleTreeStateCommit");
    m_scrollingStateTreeCommitterTimer.startOneShot(0_s);
}

void ScrollingCoordinatorMac::commitTreeState()
{
    willCommitTree();

    LOG(Scrolling, "ScrollingCoordinatorMac::commitTreeState, has changes %d", scrollingStateTree()->hasChangedProperties());

    if (!scrollingStateTree()->hasChangedProperties())
        return;

    LOG(Scrolling, "%s", scrollingStateTreeAsText(ScrollingStateTreeAsTextBehaviorDebug).utf8().data());

    RefPtr<ThreadedScrollingTree> threadedScrollingTree = downcast<ThreadedScrollingTree>(scrollingTree());
    ScrollingStateTree* unprotectedTreeState = scrollingStateTree()->commit(LayerRepresentation::PlatformLayerRepresentation).release();

    threadedScrollingTree->incrementPendingCommitCount();

    ScrollingThread::dispatch([threadedScrollingTree, unprotectedTreeState] {
        std::unique_ptr<ScrollingStateTree> treeState(unprotectedTreeState);
        threadedScrollingTree->commitTreeState(WTFMove(treeState));
    });

    updateTiledScrollingIndicator();
}

void ScrollingCoordinatorMac::updateTiledScrollingIndicator()
{
    FrameView* frameView = m_page->mainFrame().view();
    if (!frameView)
        return;
    
    TiledBacking* tiledBacking = frameView->tiledBacking();
    if (!tiledBacking)
        return;

    ScrollingModeIndication indicatorMode;
    if (shouldUpdateScrollLayerPositionSynchronously(*frameView))
        indicatorMode = SynchronousScrollingBecauseOfStyleIndication;
    else
        indicatorMode = AsyncScrollingIndication;
    
    tiledBacking->setScrollingModeIndication(indicatorMode);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)
