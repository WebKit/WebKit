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

#include "config.h"
#include "ScrollingTree.h"

#if ENABLE(THREADED_SCROLLING)

#include "PlatformWheelEvent.h"
#include "ScrollingCoordinator.h"
#include "ScrollingThread.h"
#include "ScrollingTreeNode.h"
#include "ScrollingTreeState.h"
#include <wtf/MainThread.h>

namespace WebCore {

PassRefPtr<ScrollingTree> ScrollingTree::create(ScrollingCoordinator* scrollingCoordinator)
{
    return adoptRef(new ScrollingTree(scrollingCoordinator));
}

ScrollingTree::ScrollingTree(ScrollingCoordinator* scrollingCoordinator)
    : m_scrollingCoordinator(scrollingCoordinator)
    , m_rootNode(ScrollingTreeNode::create(this))
    , m_hasWheelEventHandlers(false)
{
}

ScrollingTree::~ScrollingTree()
{
    ASSERT(!m_scrollingCoordinator);
}

bool ScrollingTree::tryToHandleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    {
        MutexLocker lock(m_mutex);

        if (m_hasWheelEventHandlers)
            return false;

        if (!m_nonFastScrollableRegion.isEmpty()) {
            // FIXME: This is not correct for non-default scroll origins.
            IntPoint position = wheelEvent.position();
            position.moveBy(m_mainFrameScrollPosition);
            if (m_nonFastScrollableRegion.contains(position))
                return false;
        }
    }

    ScrollingThread::dispatch(bind(&ScrollingTree::handleWheelEvent, this, wheelEvent));
    return true;
}

void ScrollingTree::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    ASSERT(ScrollingThread::isCurrentThread());

    m_rootNode->handleWheelEvent(wheelEvent);
}

void ScrollingTree::invalidate()
{
    // Invalidate is dispatched by the ScrollingCoordinator class on the ScrollingThread
    // to break the reference cycle between ScrollingTree and ScrollingCoordinator when the
    // ScrollingCoordinator's page is destroyed.
    ASSERT(ScrollingThread::isCurrentThread());
    m_scrollingCoordinator = nullptr;
}

void ScrollingTree::commitNewTreeState(PassOwnPtr<ScrollingTreeState> scrollingTreeState)
{
    ASSERT(ScrollingThread::isCurrentThread());

    if (scrollingTreeState->changedProperties() & (ScrollingTreeState::WheelEventHandlerCount | ScrollingTreeState::NonFastScrollableRegion)) {
        MutexLocker lock(m_mutex);

        if (scrollingTreeState->changedProperties() & ScrollingTreeState::WheelEventHandlerCount)
            m_hasWheelEventHandlers = scrollingTreeState->wheelEventHandlerCount();
        if (scrollingTreeState->changedProperties() & ScrollingTreeState::NonFastScrollableRegion)
            m_nonFastScrollableRegion = scrollingTreeState->nonFastScrollableRegion();
    }

    m_rootNode->update(scrollingTreeState.get());
}

void ScrollingTree::updateMainFrameScrollPosition(const IntPoint& scrollPosition)
{
    if (!m_scrollingCoordinator)
        return;

    {
        MutexLocker lock(m_mutex);
        m_mainFrameScrollPosition = scrollPosition;
    }

    callOnMainThread(bind(&ScrollingCoordinator::updateMainFrameScrollPosition, m_scrollingCoordinator.get(), scrollPosition));
}

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)
