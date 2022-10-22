/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "ScrollingTreeOverflowScrollProxyNodeCocoa.h"

#if ENABLE(ASYNC_SCROLLING)

#import "ScrollingStateOverflowScrollProxyNode.h"
#import "ScrollingStateTree.h"
#import "WebCoreCALayerExtras.h"

namespace WebCore {

Ref<ScrollingTreeOverflowScrollProxyNodeCocoa> ScrollingTreeOverflowScrollProxyNodeCocoa::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeOverflowScrollProxyNodeCocoa(scrollingTree, nodeID));
}

ScrollingTreeOverflowScrollProxyNodeCocoa::ScrollingTreeOverflowScrollProxyNodeCocoa(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeOverflowScrollProxyNode(scrollingTree, nodeID)
{
}

ScrollingTreeOverflowScrollProxyNodeCocoa::~ScrollingTreeOverflowScrollProxyNodeCocoa() = default;

void ScrollingTreeOverflowScrollProxyNodeCocoa::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    if (stateNode.hasChangedProperty(ScrollingStateNode::Property::Layer))
        m_layer = static_cast<CALayer*>(stateNode.layer());

    ScrollingTreeOverflowScrollProxyNode::commitStateBeforeChildren(stateNode);
}

void ScrollingTreeOverflowScrollProxyNodeCocoa::applyLayerPositions()
{
    FloatPoint scrollOffset = computeLayerPosition();

    LOG_WITH_STREAM(ScrollingTree, stream << "ScrollingTreeOverflowScrollProxyNodeCocoa " << scrollingNodeID() << " applyLayerPositions: setting bounds origin to " << scrollOffset);
    [m_layer _web_setLayerBoundsOrigin:scrollOffset];
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
