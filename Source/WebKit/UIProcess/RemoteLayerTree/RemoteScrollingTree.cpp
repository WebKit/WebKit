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
#include "RemoteScrollingTree.h"
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(UI_SIDE_COMPOSITING)

#include "RemoteLayerTreeHost.h"
#include "RemoteScrollingCoordinatorProxy.h"
#include <WebCore/ScrollingTreeFixedNodeCocoa.h>
#include <WebCore/ScrollingTreeFrameHostingNode.h>
#include <WebCore/ScrollingTreeFrameScrollingNode.h>
#include <WebCore/ScrollingTreeOverflowScrollProxyNodeCocoa.h>
#include <WebCore/ScrollingTreePluginHostingNode.h>
#include <WebCore/ScrollingTreePluginScrollingNode.h>
#include <WebCore/ScrollingTreePositionedNodeCocoa.h>
#include <WebCore/ScrollingTreeStickyNodeCocoa.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteScrollingTree);

using namespace WebCore;

RemoteScrollingTree::RemoteScrollingTree(RemoteScrollingCoordinatorProxy& scrollingCoordinator)
    : m_scrollingCoordinatorProxy(WeakPtr { scrollingCoordinator })
{
}

RemoteScrollingTree::~RemoteScrollingTree() = default;

void RemoteScrollingTree::invalidate()
{
    ASSERT(isMainRunLoop());
    Locker locker { m_treeLock };
    removeAllNodes();
    m_scrollingCoordinatorProxy = nullptr;
}

RemoteScrollingCoordinatorProxy* RemoteScrollingTree::scrollingCoordinatorProxy() const
{
    ASSERT(isMainRunLoop());
    return m_scrollingCoordinatorProxy.get();
}

void RemoteScrollingTree::scrollingTreeNodeDidScroll(ScrollingTreeScrollingNode& node, ScrollingLayerPositionAction scrollingLayerPositionAction)
{
    ASSERT(isMainRunLoop());

    ScrollingTree::scrollingTreeNodeDidScroll(node, scrollingLayerPositionAction);

    CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get();
    if (!scrollingCoordinatorProxy)
        return;

    std::optional<FloatPoint> layoutViewportOrigin;
    if (auto* scrollingNode = dynamicDowncast<ScrollingTreeFrameScrollingNode>(node))
        layoutViewportOrigin = scrollingNode->layoutViewport().location();

    scrollingCoordinatorProxy->scrollingTreeNodeDidScroll(node.scrollingNodeID(), node.currentScrollPosition(), layoutViewportOrigin, scrollingLayerPositionAction);
}

void RemoteScrollingTree::scrollingTreeNodeDidStopAnimatedScroll(ScrollingTreeScrollingNode& node)
{
    ASSERT(isMainRunLoop());

    CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get();
    if (!scrollingCoordinatorProxy)
        return;

    scrollingCoordinatorProxy->scrollingTreeNodeDidStopAnimatedScroll(node.scrollingNodeID());
}

bool RemoteScrollingTree::scrollingTreeNodeRequestsScroll(ScrollingNodeID nodeID, const RequestedScrollData& request)
{
    ASSERT(isMainRunLoop());

    CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get();
    if (!scrollingCoordinatorProxy)
        return false;

    return scrollingCoordinatorProxy->scrollingTreeNodeRequestsScroll(nodeID, request);
}

bool RemoteScrollingTree::scrollingTreeNodeRequestsKeyboardScroll(ScrollingNodeID nodeID, const RequestedKeyboardScrollData& request)
{
    ASSERT(isMainRunLoop());

    CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get();
    if (!scrollingCoordinatorProxy)
        return false;

    return scrollingCoordinatorProxy->scrollingTreeNodeRequestsKeyboardScroll(nodeID, request);
}

void RemoteScrollingTree::scrollingTreeNodeWillStartScroll(ScrollingNodeID nodeID)
{
    if (CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get())
        scrollingCoordinatorProxy->scrollingTreeNodeWillStartScroll(nodeID);
}

void RemoteScrollingTree::scrollingTreeNodeDidEndScroll(ScrollingNodeID nodeID)
{
    if (CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get())
        scrollingCoordinatorProxy->scrollingTreeNodeDidEndScroll(nodeID);
}

void RemoteScrollingTree::clearNodesWithUserScrollInProgress()
{
    ScrollingTree::clearNodesWithUserScrollInProgress();

    if (CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get())
        scrollingCoordinatorProxy->clearNodesWithUserScrollInProgress();
}

void RemoteScrollingTree::scrollingTreeNodeDidBeginScrollSnapping(ScrollingNodeID nodeID)
{
    if (CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get())
        scrollingCoordinatorProxy->scrollingTreeNodeDidBeginScrollSnapping(nodeID);
}

void RemoteScrollingTree::scrollingTreeNodeDidEndScrollSnapping(ScrollingNodeID nodeID)
{
    if (CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get())
        scrollingCoordinatorProxy->scrollingTreeNodeDidEndScrollSnapping(nodeID);
}

Ref<ScrollingTreeNode> RemoteScrollingTree::createScrollingTreeNode(ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    switch (nodeType) {
    case ScrollingNodeType::MainFrame:
    case ScrollingNodeType::Subframe:
    case ScrollingNodeType::Overflow:
    case ScrollingNodeType::PluginScrolling:
        ASSERT_NOT_REACHED(); // Subclass should have handled this.
        break;

    case ScrollingNodeType::FrameHosting:
        return ScrollingTreeFrameHostingNode::create(*this, nodeID);
    case ScrollingNodeType::PluginHosting:
        return ScrollingTreePluginHostingNode::create(*this, nodeID);
    case ScrollingNodeType::OverflowProxy:
        return ScrollingTreeOverflowScrollProxyNodeCocoa::create(*this, nodeID);
    case ScrollingNodeType::Fixed:
        return ScrollingTreeFixedNodeCocoa::create(*this, nodeID);
    case ScrollingNodeType::Sticky:
        return ScrollingTreeStickyNodeCocoa::create(*this, nodeID);
    case ScrollingNodeType::Positioned:
        return ScrollingTreePositionedNodeCocoa::create(*this, nodeID);
    }
    ASSERT_NOT_REACHED();
    return ScrollingTreeFixedNodeCocoa::create(*this, nodeID);
}

void RemoteScrollingTree::currentSnapPointIndicesDidChange(ScrollingNodeID nodeID, std::optional<unsigned> horizontal, std::optional<unsigned> vertical)
{
    ASSERT(isMainRunLoop());

    CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get();
    if (!scrollingCoordinatorProxy)
        return;

    scrollingCoordinatorProxy->currentSnapPointIndicesDidChange(nodeID, horizontal, vertical);
}

void RemoteScrollingTree::reportExposedUnfilledArea(MonotonicTime time, unsigned unfilledArea)
{
    ASSERT(isMainRunLoop());

    CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get();
    if (!scrollingCoordinatorProxy)
        return;

    scrollingCoordinatorProxy->reportExposedUnfilledArea(time, unfilledArea);
}

void RemoteScrollingTree::reportSynchronousScrollingReasonsChanged(MonotonicTime timestamp, OptionSet<SynchronousScrollingReason> reasons)
{
    ASSERT(isMainRunLoop());

    CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get();
    if (!scrollingCoordinatorProxy)
        return;

    scrollingCoordinatorProxy->reportSynchronousScrollingReasonsChanged(timestamp, reasons);
}

void RemoteScrollingTree::receivedWheelEventWithPhases(PlatformWheelEventPhase phase, PlatformWheelEventPhase momentumPhase)
{
    ASSERT(isMainRunLoop());

    CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get();
    if (!scrollingCoordinatorProxy)
        return;

    scrollingCoordinatorProxy->receivedWheelEventWithPhases(phase, momentumPhase);
}

void RemoteScrollingTree::deferWheelEventTestCompletionForReason(ScrollingNodeID nodeID, WheelEventTestMonitor::DeferReason reason)
{
    ASSERT(isMainRunLoop());

    CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get();
    if (!scrollingCoordinatorProxy || !isMonitoringWheelEvents())
        return;

    scrollingCoordinatorProxy->deferWheelEventTestCompletionForReason(nodeID, reason);
}

void RemoteScrollingTree::removeWheelEventTestCompletionDeferralForReason(ScrollingNodeID nodeID, WheelEventTestMonitor::DeferReason reason)
{
    ASSERT(isMainRunLoop());

    CheckedPtr scrollingCoordinatorProxy = m_scrollingCoordinatorProxy.get();
    if (!scrollingCoordinatorProxy || !isMonitoringWheelEvents())
        return;

    scrollingCoordinatorProxy->removeWheelEventTestCompletionDeferralForReason(nodeID, reason);
}

void RemoteScrollingTree::propagateSynchronousScrollingReasons(const HashSet<ScrollingNodeID>& synchronousScrollingNodes)
{
    m_hasNodesWithSynchronousScrollingReasons = !synchronousScrollingNodes.isEmpty();
}

void RemoteScrollingTree::tryToApplyLayerPositions()
{
    ASSERT(!isMainRunLoop());
    Locker locker { m_treeLock };
    if (m_hasNodesWithSynchronousScrollingReasons)
        return;

    applyLayerPositionsInternal();
}


} // namespace WebKit

#endif // ENABLE(UI_SIDE_COMPOSITING)
