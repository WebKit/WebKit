/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include "ThreadedScrollingCoordinator.h"

#if ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
#include "Logging.h"
#include "Page.h"
#include "ScrollingThread.h"
#include "ThreadedScrollingTree.h"
#include "WheelEventTestMonitor.h"

namespace WebCore {

ThreadedScrollingCoordinator::ThreadedScrollingCoordinator(Page* page)
    : AsyncScrollingCoordinator(page)
{
}

ThreadedScrollingCoordinator::~ThreadedScrollingCoordinator() = default;

void ThreadedScrollingCoordinator::pageDestroyed()
{
    AsyncScrollingCoordinator::pageDestroyed();

    // Invalidating the scrolling tree will break the reference cycle between the ScrollingCoordinator and ScrollingTree objects.
    RefPtr scrollingTree = static_pointer_cast<ThreadedScrollingTree>(releaseScrollingTree());
    ScrollingThread::dispatch([scrollingTree = WTFMove(scrollingTree)] {
        scrollingTree->invalidate();
    });
}

void ThreadedScrollingCoordinator::commitTreeStateIfNeeded()
{
    willCommitTree();

    LOG_WITH_STREAM(Scrolling, stream << "ThreadedScrollingCoordinator::commitTreeState, has changes " << scrollingStateTree()->hasChangedProperties());

    if (!scrollingStateTree()->hasChangedProperties())
        return;

    LOG_WITH_STREAM(ScrollingTree, stream << "ThreadedScrollingCoordinator::commitTreeState: state tree " << scrollingStateTreeAsText(debugScrollingStateTreeAsTextBehaviors));

    auto stateTree = scrollingStateTree()->commit(LayerRepresentation::PlatformLayerRepresentation);
    scrollingTree()->commitTreeState(WTFMove(stateTree));
}

WheelEventHandlingResult ThreadedScrollingCoordinator::handleWheelEventForScrolling(const PlatformWheelEvent& wheelEvent, ScrollingNodeID targetNodeID, std::optional<WheelScrollGestureState> gestureState)
{
    ASSERT(isMainThread());
    ASSERT(m_page);
    ASSERT(scrollingTree());

    if (scrollingTree()->willWheelEventStartSwipeGesture(wheelEvent))
        return WheelEventHandlingResult::unhandled();

    LOG_WITH_STREAM(Scrolling, stream << "ThreadedScrollingCoordinator::handleWheelEventForScrolling " << wheelEvent << " - sending event to scrolling thread, node " << targetNodeID << " gestureState " << gestureState);

    auto deferrer = WheelEventTestMonitorCompletionDeferrer { m_page->wheelEventTestMonitor().get(), reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(targetNodeID), WheelEventTestMonitor::DeferReason::PostMainThreadWheelEventHandling };

    RefPtr<ThreadedScrollingTree> threadedScrollingTree = downcast<ThreadedScrollingTree>(scrollingTree());
    ScrollingThread::dispatch([threadedScrollingTree, wheelEvent, targetNodeID, gestureState, deferrer = WTFMove(deferrer)] {
        threadedScrollingTree->handleWheelEventAfterMainThread(wheelEvent, targetNodeID, gestureState);
    });
    return WheelEventHandlingResult::handled();
}

void ThreadedScrollingCoordinator::wheelEventWasProcessedByMainThread(const PlatformWheelEvent& wheelEvent, std::optional<WheelScrollGestureState> gestureState)
{
    LOG_WITH_STREAM(Scrolling, stream << "ThreadedScrollingCoordinator::wheelEventWasProcessedByMainThread " << gestureState);

    RefPtr<ThreadedScrollingTree> threadedScrollingTree = downcast<ThreadedScrollingTree>(scrollingTree());
    threadedScrollingTree->wheelEventWasProcessedByMainThread(wheelEvent, gestureState);
}

void ThreadedScrollingCoordinator::scheduleTreeStateCommit()
{
    scheduleRenderingUpdate();
}

void ThreadedScrollingCoordinator::didScheduleRenderingUpdate()
{
    RefPtr<ThreadedScrollingTree> threadedScrollingTree = downcast<ThreadedScrollingTree>(scrollingTree());
    threadedScrollingTree->didScheduleRenderingUpdate();
}

void ThreadedScrollingCoordinator::willStartRenderingUpdate()
{
    ASSERT(isMainThread());
    if (m_page)
        m_page->layoutIfNeeded(LayoutOptions::UpdateCompositingLayers);
    RefPtr<ThreadedScrollingTree> threadedScrollingTree = downcast<ThreadedScrollingTree>(scrollingTree());
    threadedScrollingTree->willStartRenderingUpdate();
    commitTreeStateIfNeeded();
    synchronizeStateFromScrollingTree();
}

void ThreadedScrollingCoordinator::didCompleteRenderingUpdate()
{
    downcast<ThreadedScrollingTree>(scrollingTree())->didCompleteRenderingUpdate();

    // When scroll animations are running on the scrolling thread, we need something to continually tickle the
    // DisplayRefreshMonitor so that ThreadedScrollingTree::displayDidRefresh() will get called to service those animations.
    // We can achieve this by scheduling a rendering update; this won't cause extra work, since scrolling thread scrolls
    // will end up triggering these anyway.
    if (scrollingTree()->hasNodeWithActiveScrollAnimations())
        scheduleRenderingUpdate();
}

void ThreadedScrollingCoordinator::hasNodeWithAnimatedScrollChanged(bool hasAnimatingNode)
{
    // This is necessary to trigger a rendering update, after which the code in
    // AsyncScrollingCoordinator::didCompleteRenderingUpdate() triggers the rest.
    if (hasAnimatingNode)
        scheduleRenderingUpdate();
}

void ThreadedScrollingCoordinator::startMonitoringWheelEvents(bool clearLatchingState)
{
    // setIsMonitoringWheelEvents() is called on the root node via AsyncScrollingCoordinator::frameViewLayoutUpdated().

    if (clearLatchingState)
        scrollingTree()->clearLatchedNode();
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
