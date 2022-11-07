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

#import "config.h"
#import "RemoteScrollingTreeMac.h"

#if PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING)

#import "RemoteScrollingCoordinatorProxy.h"
#import "ScrollingTreeFrameScrollingNodeRemoteMac.h"
#import "ScrollingTreeOverflowScrollingNodeRemoteMac.h"
#import <WebCore/ScrollingTreeFixedNodeCocoa.h>

namespace WebKit {
using namespace WebCore;

Ref<RemoteScrollingTree> RemoteScrollingTree::create(RemoteScrollingCoordinatorProxy& scrollingCoordinator)
{
    return adoptRef(*new RemoteScrollingTreeMac(scrollingCoordinator));
}

RemoteScrollingTreeMac::RemoteScrollingTreeMac(RemoteScrollingCoordinatorProxy& scrollingCoordinator)
    : RemoteScrollingTree(scrollingCoordinator)
{
}

RemoteScrollingTreeMac::~RemoteScrollingTreeMac() = default;

void RemoteScrollingTreeMac::handleWheelEventPhase(ScrollingNodeID, PlatformWheelEventPhase)
{
    // FIXME: Is this needed?
}

void RemoteScrollingTreeMac::hasNodeWithAnimatedScrollChanged(bool hasNodeWithAnimatedScroll)
{
    m_scrollingCoordinatorProxy.hasNodeWithAnimatedScrollChanged(hasNodeWithAnimatedScroll);
}

void RemoteScrollingTreeMac::displayDidRefresh(PlatformDisplayID)
{
    Locker locker { m_treeLock };
    serviceScrollAnimations(MonotonicTime::now()); // FIXME: Share timestamp with the rest of the update.
}

Ref<ScrollingTreeNode> RemoteScrollingTreeMac::createScrollingTreeNode(ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    switch (nodeType) {
    case ScrollingNodeType::MainFrame:
    case ScrollingNodeType::Subframe:
        return ScrollingTreeFrameScrollingNodeRemoteMac::create(*this, nodeType, nodeID);

    case ScrollingNodeType::Overflow:
        return ScrollingTreeOverflowScrollingNodeRemoteMac::create(*this, nodeID);

    case ScrollingNodeType::FrameHosting:
    case ScrollingNodeType::OverflowProxy:
    case ScrollingNodeType::Fixed:
    case ScrollingNodeType::Sticky:
    case ScrollingNodeType::Positioned:
        return RemoteScrollingTree::createScrollingTreeNode(nodeType, nodeID);
    }
    ASSERT_NOT_REACHED();
    return ScrollingTreeFixedNodeCocoa::create(*this, nodeID);
}

void RemoteScrollingTreeMac::handleMouseEvent(const WebCore::PlatformMouseEvent& event)
{
    if (!rootNode())
        return;
    static_cast<ScrollingTreeFrameScrollingNodeRemoteMac&>(*rootNode()).handleMouseEvent(event);
}

} // namespace WebKit

#endif // PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING)
