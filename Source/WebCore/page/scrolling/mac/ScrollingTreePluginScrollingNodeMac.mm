/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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
#import "ScrollingTreePluginScrollingNodeMac.h"

#if PLATFORM(MAC)

#import "Logging.h"
#import "ScrollingStatePluginScrollingNode.h"
#import "ScrollingTree.h"
#import "ScrollingTreeScrollingNodeDelegateMac.h"
#import "WebCoreCALayerExtras.h"
#import <wtf/BlockObjCExceptions.h>
#import <wtf/text/TextStream.h>

namespace WebCore {

Ref<ScrollingTreePluginScrollingNodeMac> ScrollingTreePluginScrollingNodeMac::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreePluginScrollingNodeMac(scrollingTree, nodeID));
}

ScrollingTreePluginScrollingNodeMac::ScrollingTreePluginScrollingNodeMac(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreePluginScrollingNode(scrollingTree, nodeID)
{
    m_delegate = makeUnique<ScrollingTreeScrollingNodeDelegateMac>(*this);
}

ScrollingTreePluginScrollingNodeMac::~ScrollingTreePluginScrollingNodeMac() = default;

ScrollingTreeScrollingNodeDelegateMac& ScrollingTreePluginScrollingNodeMac::delegate() const
{
    return *static_cast<ScrollingTreeScrollingNodeDelegateMac*>(m_delegate.get());
}

void ScrollingTreePluginScrollingNodeMac::willBeDestroyed()
{
    delegate().nodeWillBeDestroyed();
}

bool ScrollingTreePluginScrollingNodeMac::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    if (!ScrollingTreePluginScrollingNode::commitStateBeforeChildren(stateNode))
        return false;

    auto* state = dynamicDowncast<ScrollingStatePluginScrollingNode>(stateNode);
    if (!state)
        return false;

    m_delegate->updateFromStateNode(*state);
    return true;
}

WheelEventHandlingResult ScrollingTreePluginScrollingNodeMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent, EventTargeting eventTargeting)
{
    if (hasNonRepaintSynchronousScrollingReasons() && eventTargeting != EventTargeting::NodeOnly)
        return { { WheelEventProcessingSteps::SynchronousScrolling, WheelEventProcessingSteps::NonBlockingDOMEventDispatch }, false };

    if (!canHandleWheelEvent(wheelEvent, eventTargeting))
        return WheelEventHandlingResult::unhandled();

    return WheelEventHandlingResult::result(delegate().handleWheelEvent(wheelEvent));
}

void ScrollingTreePluginScrollingNodeMac::willDoProgrammaticScroll(const FloatPoint& targetScrollPosition)
{
    delegate().willDoProgrammaticScroll(targetScrollPosition);
}

void ScrollingTreePluginScrollingNodeMac::currentScrollPositionChanged(ScrollType scrollType, ScrollingLayerPositionAction action)
{
    ScrollingTreePluginScrollingNode::currentScrollPositionChanged(scrollType, action);
    delegate().currentScrollPositionChanged();
}

void ScrollingTreePluginScrollingNodeMac::repositionScrollingLayers()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [static_cast<CALayer*>(scrollContainerLayer()) _web_setLayerBoundsOrigin:currentScrollOffset()];
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollingTreePluginScrollingNodeMac::repositionRelatedLayers()
{
    delegate().updateScrollbarPainters();
}

} // namespace WebCore

#endif // PLATFORM(MAC)
