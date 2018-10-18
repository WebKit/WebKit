/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(IOS_FAMILY)

#import "ScrollingCoordinatorIOS.h"

#include "Frame.h"
#include "FrameView.h"
#include "Page.h"
#include "Region.h"
#include "ScrollingStateTree.h"
#include "ScrollingThread.h"
#include "ScrollingTreeFixedNode.h"
#include "ScrollingTreeFrameScrollingNodeIOS.h"
#include "ScrollingTreeStickyNode.h"
#include "ScrollingTreeIOS.h"
#include <wtf/MainThread.h>

namespace WebCore {

Ref<ScrollingCoordinator> ScrollingCoordinator::create(Page* page)
{
    return adoptRef(*new ScrollingCoordinatorIOS(page));
}

ScrollingCoordinatorIOS::ScrollingCoordinatorIOS(Page* page)
    : AsyncScrollingCoordinator(page)
    , m_scrollingStateTreeCommitterTimer(*this, &ScrollingCoordinatorIOS::commitTreeState)
{
    setScrollingTree(ScrollingTreeIOS::create(*this));
}

ScrollingCoordinatorIOS::~ScrollingCoordinatorIOS()
{
    ASSERT(!scrollingTree());
}

void ScrollingCoordinatorIOS::pageDestroyed()
{
    AsyncScrollingCoordinator::pageDestroyed();

    m_scrollingStateTreeCommitterTimer.stop();

    // Invalidating the scrolling tree will break the reference cycle between the ScrollingCoordinator and ScrollingTree objects.
    releaseScrollingTree();
}

void ScrollingCoordinatorIOS::commitTreeStateIfNeeded()
{
    if (!scrollingStateTree()->hasChangedProperties())
        return;

    commitTreeState();
    m_scrollingStateTreeCommitterTimer.stop();
}

void ScrollingCoordinatorIOS::scheduleTreeStateCommit()
{
    ASSERT(scrollingStateTree()->hasChangedProperties());

    if (m_scrollingStateTreeCommitterTimer.isActive())
        return;

    m_scrollingStateTreeCommitterTimer.startOneShot(0_s);
}

void ScrollingCoordinatorIOS::commitTreeState()
{
    ASSERT(scrollingStateTree()->hasChangedProperties());

    scrollingStateTree()->commit(LayerRepresentation::PlatformLayerRepresentation);
    // FIXME: figure out how to commit.
}


} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(IOS_FAMILY)
