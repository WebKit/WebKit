/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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
#import "ScrollingTreePluginScrollingNodeIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "ScrollingTreeScrollingNodeDelegateIOS.h"

#import <WebCore/ScrollingStatePluginScrollingNode.h>

namespace WebKit {
using namespace WebCore;

Ref<ScrollingTreePluginScrollingNodeIOS> ScrollingTreePluginScrollingNodeIOS::create(WebCore::ScrollingTree& scrollingTree, WebCore::ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreePluginScrollingNodeIOS(scrollingTree, nodeID));
}

ScrollingTreePluginScrollingNodeIOS::ScrollingTreePluginScrollingNodeIOS(WebCore::ScrollingTree& scrollingTree, WebCore::ScrollingNodeID nodeID)
    : ScrollingTreePluginScrollingNode(scrollingTree, nodeID)
{
    m_delegate = makeUnique<ScrollingTreeScrollingNodeDelegateIOS>(*this);
}

ScrollingTreePluginScrollingNodeIOS::~ScrollingTreePluginScrollingNodeIOS()
{
}

ScrollingTreeScrollingNodeDelegateIOS& ScrollingTreePluginScrollingNodeIOS::delegate() const
{
    return *static_cast<ScrollingTreeScrollingNodeDelegateIOS*>(m_delegate.get());
}

WKBaseScrollView *ScrollingTreePluginScrollingNodeIOS::scrollView() const
{
    return delegate().scrollView();
}

bool ScrollingTreePluginScrollingNodeIOS::commitStateBeforeChildren(const WebCore::ScrollingStateNode& stateNode)
{
    if (!is<ScrollingStateScrollingNode>(stateNode))
        return false;

    if (stateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer))
        delegate().resetScrollViewDelegate();

    if (!ScrollingTreePluginScrollingNode::commitStateBeforeChildren(stateNode))
        return false;

    delegate().commitStateBeforeChildren(downcast<ScrollingStateScrollingNode>(stateNode));
    return true;
}

bool ScrollingTreePluginScrollingNodeIOS::commitStateAfterChildren(const ScrollingStateNode& stateNode)
{
    auto* scrollingStateNode = dynamicDowncast<ScrollingStateScrollingNode>(stateNode);
    if (!scrollingStateNode)
        return false;

    delegate().commitStateAfterChildren(*scrollingStateNode);

    return ScrollingTreePluginScrollingNode::commitStateAfterChildren(*scrollingStateNode);
}

void ScrollingTreePluginScrollingNodeIOS::repositionScrollingLayers()
{
    delegate().repositionScrollingLayers();
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
