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
#include "ScrollingTreeMac.h"

#include "ScrollingTreeFixedNode.h"
#include "ScrollingTreeFrameScrollingNodeMac.h"
#include "ScrollingTreeStickyNode.h"

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)

using namespace WebCore;

Ref<ScrollingTreeMac> ScrollingTreeMac::create(AsyncScrollingCoordinator& scrollingCoordinator)
{
    return adoptRef(*new ScrollingTreeMac(scrollingCoordinator));
}

ScrollingTreeMac::ScrollingTreeMac(AsyncScrollingCoordinator& scrollingCoordinator)
    : ThreadedScrollingTree(scrollingCoordinator)
{
}

Ref<ScrollingTreeNode> ScrollingTreeMac::createScrollingTreeNode(ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    switch (nodeType) {
    case ScrollingNodeType::MainFrame:
    case ScrollingNodeType::Subframe:
        return ScrollingTreeFrameScrollingNodeMac::create(*this, nodeType, nodeID);
    case ScrollingNodeType::Overflow:
        ASSERT_NOT_REACHED();
        break;
    case ScrollingNodeType::Fixed:
        return ScrollingTreeFixedNode::create(*this, nodeID);
    case ScrollingNodeType::Sticky:
        return ScrollingTreeStickyNode::create(*this, nodeID);
    }
    ASSERT_NOT_REACHED();
    return ScrollingTreeFixedNode::create(*this, nodeID);
}

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)
