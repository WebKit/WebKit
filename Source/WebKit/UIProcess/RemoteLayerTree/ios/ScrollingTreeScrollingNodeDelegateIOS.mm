/*
 * Copyright (C) 2017 Igalia S.L. All rights reserved.
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
#import "ScrollingTreeScrollingNodeDelegateIOS.h"

#if PLATFORM(IOS) && ENABLE(ASYNC_SCROLLING)

#import <WebCore/ScrollingTree.h>
#import <WebCore/ScrollingTreeFrameScrollingNode.h>
#import <WebCore/ScrollingTreeScrollingNode.h>

using namespace WebCore;

namespace WebKit {

ScrollingTreeScrollingNodeDelegateIOS::ScrollingTreeScrollingNodeDelegateIOS(ScrollingTreeScrollingNode& scrollingNode)
    : ScrollingTreeScrollingNodeDelegate(scrollingNode)
    , m_updatingFromStateNode(false)
{
}

ScrollingTreeScrollingNodeDelegateIOS::~ScrollingTreeScrollingNodeDelegateIOS()
{
}

void ScrollingTreeScrollingNodeDelegateIOS::updateChildNodesAfterScroll(const FloatPoint& scrollPosition)
{
    if (!scrollingNode().children())
        return;

    FloatRect fixedPositionRect;
    ScrollingTreeFrameScrollingNode* frameNode = scrollingNode().enclosingFrameNode();
    if (frameNode && frameNode->parent())
        fixedPositionRect = frameNode->fixedPositionRect();
    else
        fixedPositionRect = scrollingTree().fixedPositionRect();
    FloatSize scrollDelta = lastCommittedScrollPosition() - scrollPosition;

    for (auto& child : *scrollingNode().children())
        child->updateLayersAfterAncestorChange(scrollingNode(), fixedPositionRect, scrollDelta);
}

void ScrollingTreeScrollingNodeDelegateIOS::scrollWillStart() const
{
    scrollingTree().scrollingTreeNodeWillStartScroll();
}

void ScrollingTreeScrollingNodeDelegateIOS::scrollDidEnd() const
{
    scrollingTree().scrollingTreeNodeDidEndScroll();
}

void ScrollingTreeScrollingNodeDelegateIOS::scrollViewWillStartPanGesture() const
{
    scrollingTree().scrollingTreeNodeWillStartPanGesture();
}

void ScrollingTreeScrollingNodeDelegateIOS::scrollViewDidScroll(const FloatPoint& scrollPosition, bool inUserInteraction) const
{
    if (m_updatingFromStateNode)
        return;

    scrollingTree().scrollPositionChangedViaDelegatedScrolling(scrollingNode().scrollingNodeID(), scrollPosition, inUserInteraction);
}

void ScrollingTreeScrollingNodeDelegateIOS::currentSnapPointIndicesDidChange(unsigned horizontal, unsigned vertical) const
{
    if (m_updatingFromStateNode)
        return;

    scrollingTree().currentSnapPointIndicesDidChange(scrollingNode().scrollingNodeID(), horizontal, vertical);
}

} // namespace WebKit

#endif // PLATFORM(IOS) && ENABLE(ASYNC_SCROLLING)
