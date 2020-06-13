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
#import "PlatformWheelEvent.h"
#import "Region.h"
#import "ScrollingStateTree.h"
#import "ScrollingThread.h"
#import "ScrollingTreeFixedNode.h"
#import "ScrollingTreeFrameScrollingNodeMac.h"
#import "ScrollingTreeMac.h"
#import "ScrollingTreeStickyNode.h"
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
    RefPtr<ThreadedScrollingTree> scrollingTree = static_pointer_cast<ThreadedScrollingTree>(releaseScrollingTree());
    ScrollingThread::dispatch([scrollingTree] {
        scrollingTree->invalidate();
    });
}

bool ScrollingCoordinatorMac::handleWheelEvent(FrameView&, const PlatformWheelEvent& wheelEvent)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (scrollingTree()->willWheelEventStartSwipeGesture(wheelEvent))
        return false;

    RefPtr<ThreadedScrollingTree> threadedScrollingTree = downcast<ThreadedScrollingTree>(scrollingTree());
    ScrollingThread::dispatch([threadedScrollingTree, wheelEvent] {
        threadedScrollingTree->handleWheelEventAfterMainThread(wheelEvent);
    });
    return true;
}

void ScrollingCoordinatorMac::scheduleTreeStateCommit()
{
    m_page->scheduleRenderingUpdate();
}

void ScrollingCoordinatorMac::commitTreeStateIfNeeded()
{
    willCommitTree();

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingCoordinatorMac::commitTreeState, has changes " << scrollingStateTree()->hasChangedProperties());

    if (!scrollingStateTree()->hasChangedProperties())
        return;

    LOG_WITH_STREAM(ScrollingTree, stream << scrollingStateTreeAsText(ScrollingStateTreeAsTextBehaviorDebug));

    auto stateTree = scrollingStateTree()->commit(LayerRepresentation::PlatformLayerRepresentation);
    scrollingTree()->commitTreeState(WTFMove(stateTree));

    updateTiledScrollingIndicator();
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
