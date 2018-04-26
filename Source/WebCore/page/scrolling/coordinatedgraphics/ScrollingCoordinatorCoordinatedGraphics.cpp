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
#include "ScrollingCoordinatorCoordinatedGraphics.h"

#if USE(COORDINATED_GRAPHICS)

#include "ScrollingThread.h"
#include "ScrollingTreeCoordinatedGraphics.h"

namespace WebCore {

Ref<ScrollingCoordinator> ScrollingCoordinator::create(Page* page)
{
    return adoptRef(*new ScrollingCoordinatorCoordinatedGraphics(page));
}

ScrollingCoordinatorCoordinatedGraphics::ScrollingCoordinatorCoordinatedGraphics(Page* page)
    : AsyncScrollingCoordinator(page)
    , m_scrollingStateTreeCommitterTimer(RunLoop::main(), this, &ScrollingCoordinatorCoordinatedGraphics::commitTreeState)
{
    setScrollingTree(ScrollingTreeCoordinatedGraphics::create(*this));
}

ScrollingCoordinatorCoordinatedGraphics::~ScrollingCoordinatorCoordinatedGraphics()
{
    ASSERT(!scrollingTree());
}

void ScrollingCoordinatorCoordinatedGraphics::pageDestroyed()
{
    AsyncScrollingCoordinator::pageDestroyed();

    m_scrollingStateTreeCommitterTimer.stop();

    releaseScrollingTree();
}

void ScrollingCoordinatorCoordinatedGraphics::commitTreeStateIfNeeded()
{
    commitTreeState();
    m_scrollingStateTreeCommitterTimer.stop();
}

bool ScrollingCoordinatorCoordinatedGraphics::handleWheelEvent(FrameView&, const PlatformWheelEvent&)
{
    return false;
}

void ScrollingCoordinatorCoordinatedGraphics::scheduleTreeStateCommit()
{
    if (!m_scrollingStateTreeCommitterTimer.isActive())
        m_scrollingStateTreeCommitterTimer.startOneShot(0_s);
}

void ScrollingCoordinatorCoordinatedGraphics::commitTreeState()
{
    if (!scrollingStateTree()->hasChangedProperties())
        return;

    RefPtr<ThreadedScrollingTree> threadedScrollingTree = downcast<ThreadedScrollingTree>(scrollingTree());

    auto treeState = scrollingStateTree()->commit(LayerRepresentation::PlatformLayerRepresentation);
    ScrollingThread::dispatch([threadedScrollingTree, treeState = WTFMove(treeState)]() mutable {
        threadedScrollingTree->commitTreeState(WTFMove(treeState));
    });
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
