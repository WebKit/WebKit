/*
 * Copyright (C) 2018 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollingCoordinatorNicosia.h"

#if ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)

#include "ScrollingThread.h"
#include "ScrollingTreeNicosia.h"

namespace WebCore {

Ref<ScrollingCoordinator> ScrollingCoordinator::create(Page* page)
{
    return adoptRef(*new ScrollingCoordinatorNicosia(page));
}

ScrollingCoordinatorNicosia::ScrollingCoordinatorNicosia(Page* page)
    : AsyncScrollingCoordinator(page)
{
    setScrollingTree(ScrollingTreeNicosia::create(*this));
}

ScrollingCoordinatorNicosia::~ScrollingCoordinatorNicosia()
{
    ASSERT(!scrollingTree());
}

void ScrollingCoordinatorNicosia::pageDestroyed()
{
    AsyncScrollingCoordinator::pageDestroyed();

    // Invalidating the scrolling tree will break the reference cycle between the ScrollingCoordinator and ScrollingTree objects.
    RefPtr scrollingTree = static_pointer_cast<ThreadedScrollingTree>(releaseScrollingTree());
    ScrollingThread::dispatch([scrollingTree = WTFMove(scrollingTree)] {
        scrollingTree->invalidate();
    });
}

void ScrollingCoordinatorNicosia::commitTreeStateIfNeeded()
{
    willCommitTree();

    if (!scrollingStateTree()->hasChangedProperties())
        return;

    auto stateTree = scrollingStateTree()->commit(LayerRepresentation::PlatformLayerRepresentation);
    scrollingTree()->commitTreeState(WTFMove(stateTree));
}

bool ScrollingCoordinatorNicosia::handleWheelEventForScrolling(const PlatformWheelEvent& wheelEvent, ScrollingNodeID targetNode, std::optional<WheelScrollGestureState> gestureState)
{
    ASSERT(isMainThread());
    ASSERT(m_page);
    ASSERT(scrollingTree());

    ScrollingThread::dispatch([threadedScrollingTree = Ref { downcast<ThreadedScrollingTree>(*scrollingTree()) }, wheelEvent, targetNode, gestureState] {
        threadedScrollingTree->handleWheelEventAfterMainThread(wheelEvent, targetNode, gestureState);
    });
    return true;
}

void ScrollingCoordinatorNicosia::wheelEventWasProcessedByMainThread(const PlatformWheelEvent& wheelEvent, std::optional<WheelScrollGestureState> gestureState)
{
    RefPtr<ThreadedScrollingTree> threadedScrollingTree = downcast<ThreadedScrollingTree>(scrollingTree());
    threadedScrollingTree->wheelEventWasProcessedByMainThread(wheelEvent, gestureState);
}

void ScrollingCoordinatorNicosia::scheduleTreeStateCommit()
{
    scheduleRenderingUpdate();
}

void ScrollingCoordinatorNicosia::willStartRenderingUpdate()
{
    RefPtr<ThreadedScrollingTree> threadedScrollingTree = downcast<ThreadedScrollingTree>(scrollingTree());
    threadedScrollingTree->willStartRenderingUpdate();
    commitTreeStateIfNeeded();
    synchronizeStateFromScrollingTree();
}

void ScrollingCoordinatorNicosia::didCompleteRenderingUpdate()
{
    downcast<ThreadedScrollingTree>(scrollingTree())->didCompleteRenderingUpdate();

    // Scroll animations are driven by the display refresh, so make sure that we
    // keep firing until they're complete.
    if (scrollingTree()->hasNodeWithActiveScrollAnimations())
        scheduleRenderingUpdate();
}

void ScrollingCoordinatorNicosia::hasNodeWithAnimatedScrollChanged(bool hasAnimatingNode)
{
    if (hasAnimatingNode)
        scheduleRenderingUpdate();
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)
