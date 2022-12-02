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
#include "ScrollingTreeFixedNodeNicosia.h"
#include "ScrollingTreeFrameHostingNode.h"
#include "ScrollingTreeFrameScrollingNodeNicosia.h"
#include "ScrollingTreeOverflowScrollProxyNodeNicosia.h"
#include "ScrollingTreeOverflowScrollingNodeNicosia.h"
#include "ScrollingTreePositionedNodeNicosia.h"
#include "ScrollingTreeStickyNodeNicosia.h"

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
        return ScrollingTreeOverflowScrollProxyNodeNicosia::create(*this, nodeID);
    case ScrollingNodeType::Fixed:
        return ScrollingTreeFixedNodeNicosia::create(*this, nodeID);
    case ScrollingNodeType::Sticky:
        return ScrollingTreeStickyNodeNicosia::create(*this, nodeID);
    case ScrollingNodeType::Positioned:
        return ScrollingTreePositionedNodeNicosia::create(*this, nodeID);
    }

    RELEASE_ASSERT_NOT_REACHED();
}

using Nicosia::CompositionLayer;

static bool collectDescendantLayersAtPoint(Vector<RefPtr<CompositionLayer>>& layersAtPoint, RefPtr<CompositionLayer> parent, const FloatPoint& point)
{
    bool existsOnLayer = false;
    bool existsOnDescendent = false;

    parent->accessCommitted([&](const CompositionLayer::LayerState& state) {
        existsOnLayer = !!state.scrollingNodeID && FloatRect({ }, state.size).contains(point) && state.eventRegion.contains(roundedIntPoint(point));

        for (auto child : state.children) {
            FloatPoint transformedPoint(point);
            child->accessCommitted([&](const CompositionLayer::LayerState& childState) {
                if (!childState.transform.isInvertible())
                    return;
                float originX = childState.anchorPoint.x() * childState.size.width();
                float originY = childState.anchorPoint.y() * childState.size.height();
                auto transform = *(TransformationMatrix()
                    .translate3d(originX + childState.position.x() - state.boundsOrigin.x(), originY + childState.position.y() - state.boundsOrigin.y(), childState.anchorPoint.z())
                    .multiply(childState.transform)
                    .translate3d(-originX, -originY, -childState.anchorPoint.z()).inverse());
                auto pointInChildSpace = transform.projectPoint(point);
                transformedPoint.set(pointInChildSpace.x(), pointInChildSpace.y());
            });
            existsOnDescendent |= collectDescendantLayersAtPoint(layersAtPoint, child, transformedPoint);
        }
    });

    if (existsOnLayer && !existsOnDescendent)
        layersAtPoint.append(parent);

    return existsOnLayer || existsOnDescendent;
}

RefPtr<ScrollingTreeNode> ScrollingTreeNicosia::scrollingNodeForPoint(FloatPoint point)
{
    auto* rootScrollingNode = rootNode();
    if (!rootScrollingNode)
        return nullptr;

    Locker layerLocker { m_layerHitTestMutex };

    auto rootContentsLayer = static_cast<ScrollingTreeFrameScrollingNodeNicosia*>(rootScrollingNode)->rootContentsLayer();
    Vector<RefPtr<CompositionLayer>> layersAtPoint;
    collectDescendantLayersAtPoint(layersAtPoint, rootContentsLayer, point);

    ScrollingTreeNode* returnNode = nullptr;
    for (auto layer : makeReversedRange(layersAtPoint)) {
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
