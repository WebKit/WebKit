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

#import "config.h"

#if ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)

#import "ScrollingCoordinatorMac.h"

#import "Frame.h"
#import "FrameView.h"
#import "Logging.h"
#import "Page.h"
#import "PlatformCALayerContentsDelayedReleaser.h"
#import "PlatformWheelEvent.h"
#import "Region.h"
#import "ScrollingStateTree.h"
#import "ScrollingThread.h"
#import "ScrollingTreeFixedNode.h"
#import "ScrollingTreeFrameScrollingNodeMac.h"
#import "ScrollingTreeMac.h"
#import "ScrollingTreeStickyNodeCocoa.h"
#import "TiledBacking.h"
#import <wtf/MainThread.h>

namespace WebCore {

Ref<ScrollingCoordinator> ScrollingCoordinator::create(Page* page)
{
    return adoptRef(*new ScrollingCoordinatorMac(page));
}

ScrollingCoordinatorMac::ScrollingCoordinatorMac(Page* page)
    : AsyncScrollingCoordinator(page)
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

    // Invalidating the scrolling tree will break the reference cycle between the ScrollingCoordinator and ScrollingTree objects.
    RefPtr scrollingTree = static_pointer_cast<ThreadedScrollingTree>(releaseScrollingTree());
    ScrollingThread::dispatch([scrollingTree = WTFMove(scrollingTree)] {
        scrollingTree->invalidate();
    });
}

bool ScrollingCoordinatorMac::handleWheelEventForScrolling(const PlatformWheelEvent& wheelEvent, ScrollingNodeID targetNodeID, std::optional<WheelScrollGestureState> gestureState)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (scrollingTree()->willWheelEventStartSwipeGesture(wheelEvent))
        return false;

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingCoordinatorMac::handleWheelEventForScrolling " << wheelEvent << " - sending event to scrolling thread, node " << targetNodeID << " gestureState " << gestureState);

    auto deferrer = WheelEventTestMonitorCompletionDeferrer { m_page->wheelEventTestMonitor().get(), reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(targetNodeID), WheelEventTestMonitor::PostMainThreadWheelEventHandling };

    RefPtr<ThreadedScrollingTree> threadedScrollingTree = downcast<ThreadedScrollingTree>(scrollingTree());
    ScrollingThread::dispatch([threadedScrollingTree, wheelEvent, targetNodeID, gestureState, deferrer = WTFMove(deferrer)] {
        threadedScrollingTree->handleWheelEventAfterMainThread(wheelEvent, targetNodeID, gestureState);
    });
    return true;
}

void ScrollingCoordinatorMac::wheelEventWasProcessedByMainThread(const PlatformWheelEvent& wheelEvent, std::optional<WheelScrollGestureState> gestureState)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingCoordinatorMac::wheelEventWasProcessedByMainThread " << gestureState);

    RefPtr<ThreadedScrollingTree> threadedScrollingTree = downcast<ThreadedScrollingTree>(scrollingTree());
    threadedScrollingTree->wheelEventWasProcessedByMainThread(wheelEvent, gestureState);
}

void ScrollingCoordinatorMac::scheduleTreeStateCommit()
{
    scheduleRenderingUpdate();
}

void ScrollingCoordinatorMac::commitTreeStateIfNeeded()
{
    willCommitTree();

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingCoordinatorMac::commitTreeState, has changes " << scrollingStateTree()->hasChangedProperties());

    if (!scrollingStateTree()->hasChangedProperties())
        return;

    LOG_WITH_STREAM(ScrollingTree, stream << "ScrollingCoordinatorMac::commitTreeState: state tree " << scrollingStateTreeAsText(debugScrollingStateTreeAsTextBehaviors));

    auto stateTree = scrollingStateTree()->commit(LayerRepresentation::PlatformLayerRepresentation);
    scrollingTree()->commitTreeState(WTFMove(stateTree));

    updateTiledScrollingIndicator();
}

void ScrollingCoordinatorMac::didScheduleRenderingUpdate()
{
    RefPtr<ThreadedScrollingTree> threadedScrollingTree = downcast<ThreadedScrollingTree>(scrollingTree());
    threadedScrollingTree->didScheduleRenderingUpdate();
}

void ScrollingCoordinatorMac::willStartRenderingUpdate()
{
    RefPtr<ThreadedScrollingTree> threadedScrollingTree = downcast<ThreadedScrollingTree>(scrollingTree());
    threadedScrollingTree->willStartRenderingUpdate();
    commitTreeStateIfNeeded();
    synchronizeStateFromScrollingTree();
}

void ScrollingCoordinatorMac::didCompleteRenderingUpdate()
{
    downcast<ThreadedScrollingTree>(scrollingTree())->didCompleteRenderingUpdate();

    // When scroll animations are running on the scrolling thread, we need something to continually tickle the
    // DisplayRefreshMonitor so that ThreadedScrollingTree::displayDidRefresh() will get called to service those animations.
    // We can achieve this by scheduling a rendering update; this won't cause extra work, since scrolling thread scrolls
    // will end up triggering these anyway.
    if (scrollingTree()->hasNodeWithActiveScrollAnimations())
        scheduleRenderingUpdate();
}

void ScrollingCoordinatorMac::willStartPlatformRenderingUpdate()
{
    PlatformCALayerContentsDelayedReleaser::singleton().mainThreadCommitWillStart();
}

void ScrollingCoordinatorMac::didCompletePlatformRenderingUpdate()
{
    downcast<ScrollingTreeMac>(scrollingTree())->didCompletePlatformRenderingUpdate();
    PlatformCALayerContentsDelayedReleaser::singleton().mainThreadCommitDidEnd();
}

void ScrollingCoordinatorMac::hasNodeWithAnimatedScrollChanged(bool hasAnimatingNode)
{
    // This is necessary to trigger a rendering update, after which the code in
    // ScrollingCoordinatorMac::didCompleteRenderingUpdate() triggers the rest.
    if (hasAnimatingNode)
        scheduleRenderingUpdate();
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

void ScrollingCoordinatorMac::startMonitoringWheelEvents(bool clearLatchingState)
{
    if (clearLatchingState)
        scrollingTree()->clearLatchedNode();
    auto monitor = m_page->wheelEventTestMonitor();
    scrollingTree()->setWheelEventTestMonitor(WTFMove(monitor));
}

void ScrollingCoordinatorMac::stopMonitoringWheelEvents()
{
    scrollingTree()->setWheelEventTestMonitor(nullptr);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
