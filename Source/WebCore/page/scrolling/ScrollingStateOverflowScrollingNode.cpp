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
#include "ScrollingStateOverflowScrollingNode.h"

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingStateTree.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

ScrollingStateOverflowScrollingNode::ScrollingStateOverflowScrollingNode(
    ScrollingNodeID scrollingNodeID,
    Vector<Ref<WebCore::ScrollingStateNode>>&& children,
    OptionSet<ScrollingStateNodeProperty> changedProperties,
    std::optional<WebCore::PlatformLayerIdentifier> layerID,
    FloatSize scrollableAreaSize,
    FloatSize totalContentsSize,
    FloatSize reachableContentsSize,
    FloatPoint scrollPosition,
    IntPoint scrollOrigin,
    ScrollableAreaParameters&& scrollableAreaParameters,
#if ENABLE(SCROLLING_THREAD)
    OptionSet<SynchronousScrollingReason> synchronousScrollingReasons,
#endif
    RequestedScrollData&& requestedScrollData,
    FloatScrollSnapOffsetsInfo&& snapOffsetsInfo,
    std::optional<unsigned> currentHorizontalSnapPointIndex,
    std::optional<unsigned> currentVerticalSnapPointIndex,
    bool isMonitoringWheelEvents,
    std::optional<PlatformLayerIdentifier> scrollContainerLayer,
    std::optional<PlatformLayerIdentifier> scrolledContentsLayer,
    std::optional<PlatformLayerIdentifier> horizontalScrollbarLayer,
    std::optional<PlatformLayerIdentifier> verticalScrollbarLayer,
    bool mouseIsOverContentArea,
    MouseLocationState&& mouseLocationState,
    ScrollbarHoverState&& scrollbarHoverState,
    ScrollbarEnabledState&& scrollbarEnabledState,
    RequestedKeyboardScrollData&& scrollData
) : ScrollingStateScrollingNode(
    ScrollingNodeType::Overflow,
    scrollingNodeID,
    WTFMove(children),
    changedProperties,
    layerID,
    scrollableAreaSize,
    totalContentsSize,
    reachableContentsSize,
    scrollPosition,
    scrollOrigin,
    WTFMove(scrollableAreaParameters),
#if ENABLE(SCROLLING_THREAD)
    synchronousScrollingReasons,
#endif
    WTFMove(requestedScrollData),
    WTFMove(snapOffsetsInfo),
    currentHorizontalSnapPointIndex,
    currentVerticalSnapPointIndex,
    isMonitoringWheelEvents,
    scrollContainerLayer,
    scrolledContentsLayer,
    horizontalScrollbarLayer,
    verticalScrollbarLayer,
    mouseIsOverContentArea,
    WTFMove(mouseLocationState),
    WTFMove(scrollbarHoverState),
    WTFMove(scrollbarEnabledState),
    WTFMove(scrollData)
) { }

ScrollingStateOverflowScrollingNode::ScrollingStateOverflowScrollingNode(ScrollingStateTree& stateTree, ScrollingNodeID nodeID)
    : ScrollingStateScrollingNode(stateTree, ScrollingNodeType::Overflow, nodeID)
{
}

ScrollingStateOverflowScrollingNode::ScrollingStateOverflowScrollingNode(const ScrollingStateOverflowScrollingNode& stateNode, ScrollingStateTree& adoptiveTree)
    : ScrollingStateScrollingNode(stateNode, adoptiveTree)
{
}

ScrollingStateOverflowScrollingNode::~ScrollingStateOverflowScrollingNode() = default;

Ref<ScrollingStateNode> ScrollingStateOverflowScrollingNode::clone(ScrollingStateTree& adoptiveTree)
{
    return adoptRef(*new ScrollingStateOverflowScrollingNode(*this, adoptiveTree));
}

void ScrollingStateOverflowScrollingNode::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ts << "Overflow scrolling node";
    
    ScrollingStateScrollingNode::dumpProperties(ts, behavior);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
