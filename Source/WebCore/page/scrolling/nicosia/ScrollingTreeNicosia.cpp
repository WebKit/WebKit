/*
 * Copyright (C) 2018 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollingTreeNicosia.h"

#if ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)

#include "AsyncScrollingCoordinator.h"
#include "NicosiaPlatformLayer.h"
#include "ScrollingTreeFixedNode.h"
#include "ScrollingTreeFrameHostingNode.h"
#include "ScrollingTreeFrameScrollingNodeNicosia.h"
#include "ScrollingTreeOverflowScrollProxyNode.h"
#include "ScrollingTreeOverflowScrollingNodeNicosia.h"
#include "ScrollingTreePositionedNode.h"
#include "ScrollingTreeStickyNode.h"

namespace WebCore {

Ref<ScrollingTreeNicosia> ScrollingTreeNicosia::create(AsyncScrollingCoordinator& scrollingCoordinator)
{
    return adoptRef(*new ScrollingTreeNicosia(scrollingCoordinator));
}

ScrollingTreeNicosia::ScrollingTreeNicosia(AsyncScrollingCoordinator& scrollingCoordinator)
    : ThreadedScrollingTree(scrollingCoordinator)
{
}

Ref<ScrollingTreeNode> ScrollingTreeNicosia::createScrollingTreeNode(ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    switch (nodeType) {
    case ScrollingNodeType::MainFrame:
    case ScrollingNodeType::Subframe:
        return ScrollingTreeFrameScrollingNodeNicosia::create(*this, nodeType, nodeID);
    case ScrollingNodeType::FrameHosting:
        return ScrollingTreeFrameHostingNode::create(*this, nodeID);
    case ScrollingNodeType::Overflow:
        return ScrollingTreeOverflowScrollingNodeNicosia::create(*this, nodeID);
    case ScrollingNodeType::OverflowProxy:
        return ScrollingTreeOverflowScrollProxyNode::create(*this, nodeID);
    case ScrollingNodeType::Fixed:
        return ScrollingTreeFixedNode::create(*this, nodeID);
    case ScrollingNodeType::Sticky:
        return ScrollingTreeStickyNode::create(*this, nodeID);
    case ScrollingNodeType::Positioned:
        return ScrollingTreePositionedNode::create(*this, nodeID);
    }

    RELEASE_ASSERT_NOT_REACHED();
}

using Nicosia::CompositionLayer;

static void collectDescendantLayersAtPoint(Vector<RefPtr<CompositionLayer>>& layersAtPoint, RefPtr<CompositionLayer> parent, const FloatPoint& point)
{
    bool childExistsAtPoint = false;

    parent->accessPending([&](const CompositionLayer::LayerState& state) {
        for (auto child : state.children) {
            bool containsPoint = false;
            FloatPoint transformedPoint;
            child->accessPending([&](const CompositionLayer::LayerState& childState) {
                if (!childState.transform.isInvertible())
                    return;
                float originX = childState.anchorPoint.x() * childState.size.width();
                float originY = childState.anchorPoint.y() * childState.size.height();
                auto transform = *(TransformationMatrix()
                    .translate3d(originX + childState.position.x(), originY + childState.position.y(), childState.anchorPoint.z())
                    .multiply(childState.transform)
                    .translate3d(-originX, -originY, -childState.anchorPoint.z()).inverse());
                auto childPoint = transform.projectPoint(point);
                if (FloatRect(FloatPoint(), childState.size).contains(childPoint)) {
                    containsPoint = true;
                    transformedPoint.set(childPoint.x(), childPoint.y());
                }
            });
            if (containsPoint) {
                childExistsAtPoint = true;
                collectDescendantLayersAtPoint(layersAtPoint, child, transformedPoint);
            }
        }
    });

    if (!childExistsAtPoint)
        layersAtPoint.append(parent);
}

RefPtr<ScrollingTreeNode> ScrollingTreeNicosia::scrollingNodeForPoint(FloatPoint point)
{
    auto* rootScrollingNode = rootNode();
    if (!rootScrollingNode)
        return nullptr;

    LayerTreeHitTestLocker layerLocker(m_scrollingCoordinator.get());

    auto rootContentsLayer = static_cast<ScrollingTreeFrameScrollingNodeNicosia*>(rootScrollingNode)->rootContentsLayer();
    Vector<RefPtr<CompositionLayer>> layersAtPoint;
    collectDescendantLayersAtPoint(layersAtPoint, rootContentsLayer, point);

    ScrollingTreeNode* returnNode = nullptr;
    for (auto layer : WTF::makeReversedRange(layersAtPoint)) {
        layer->accessCommitted([&](const CompositionLayer::LayerState& state) {
            auto* scrollingNode = nodeForID(state.scrollingNodeID);
            if (is<ScrollingTreeScrollingNode>(scrollingNode))
                returnNode = scrollingNode;
        });
        if (returnNode)
            break;
    }

    return returnNode ? returnNode : rootScrollingNode;
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)
