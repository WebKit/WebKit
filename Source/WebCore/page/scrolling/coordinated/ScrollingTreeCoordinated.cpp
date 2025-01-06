/*
 * Copyright (C) 2018, 2024 Igalia S.L.
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
#include "ScrollingTreeCoordinated.h"

#if ENABLE(ASYNC_SCROLLING) && USE(COORDINATED_GRAPHICS)
#include "AsyncScrollingCoordinator.h"
#include "CoordinatedPlatformLayer.h"
#include "ScrollingThread.h"
#include "ScrollingTreeFixedNodeCoordinated.h"
#include "ScrollingTreeFrameHostingNode.h"
#include "ScrollingTreeFrameScrollingNodeCoordinated.h"
#include "ScrollingTreeOverflowScrollProxyNodeCoordinated.h"
#include "ScrollingTreeOverflowScrollingNodeCoordinated.h"
#include "ScrollingTreePositionedNodeCoordinated.h"
#include "ScrollingTreeStickyNodeCoordinated.h"

namespace WebCore {

Ref<ScrollingTreeCoordinated> ScrollingTreeCoordinated::create(AsyncScrollingCoordinator& scrollingCoordinator)
{
    return adoptRef(*new ScrollingTreeCoordinated(scrollingCoordinator));
}

ScrollingTreeCoordinated::ScrollingTreeCoordinated(AsyncScrollingCoordinator& scrollingCoordinator)
    : ThreadedScrollingTree(scrollingCoordinator)
{
}

Ref<ScrollingTreeNode> ScrollingTreeCoordinated::createScrollingTreeNode(ScrollingNodeType nodeType, ScrollingNodeID nodeID)
{
    switch (nodeType) {
    case ScrollingNodeType::MainFrame:
    case ScrollingNodeType::Subframe:
        return ScrollingTreeFrameScrollingNodeCoordinated::create(*this, nodeType, nodeID);
    case ScrollingNodeType::FrameHosting:
        return ScrollingTreeFrameHostingNode::create(*this, nodeID);
    case ScrollingNodeType::Overflow:
        return ScrollingTreeOverflowScrollingNodeCoordinated::create(*this, nodeID);
    case ScrollingNodeType::OverflowProxy:
        return ScrollingTreeOverflowScrollProxyNodeCoordinated::create(*this, nodeID);
    case ScrollingNodeType::Fixed:
        return ScrollingTreeFixedNodeCoordinated::create(*this, nodeID);
    case ScrollingNodeType::Sticky:
        return ScrollingTreeStickyNodeCoordinated::create(*this, nodeID);
    case ScrollingNodeType::Positioned:
        return ScrollingTreePositionedNodeCoordinated::create(*this, nodeID);
    case ScrollingNodeType::PluginScrolling:
    case ScrollingNodeType::PluginHosting:
        RELEASE_ASSERT_NOT_REACHED();
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void ScrollingTreeCoordinated::applyLayerPositionsInternal()
{
    auto* rootScrollingNode = rootNode();
    if (!rootScrollingNode)
        return;

    ThreadedScrollingTree::applyLayerPositionsInternal();

    if (ScrollingThread::isCurrentThread()) {
        auto rootContentsLayer = static_cast<ScrollingTreeFrameScrollingNodeCoordinated*>(rootScrollingNode)->rootContentsLayer();
        rootContentsLayer->requestComposition();
    }
}

void ScrollingTreeCoordinated::didCompleteRenderingUpdate()
{
    // If there's a composition requested or ongoing, wait for didCompletePlatformRenderingUpdate() that will be
    // called once the composiiton finishes.
    if (auto* rootScrollingNode = rootNode()) {
        auto rootContentsLayer = static_cast<ScrollingTreeFrameScrollingNodeCoordinated*>(rootScrollingNode)->rootContentsLayer();
        if (rootContentsLayer->isCompositionRequiredOrOngoing())
            return;
    }

    renderingUpdateComplete();
}

void ScrollingTreeCoordinated::didCompletePlatformRenderingUpdate()
{
    renderingUpdateComplete();
}

static bool collectDescendantLayersAtPoint(Vector<Ref<CoordinatedPlatformLayer>>& layersAtPoint, const Ref<CoordinatedPlatformLayer>& parent, const FloatPoint& point)
{
    bool existsOnDescendent = false;
    bool existsOnLayer = !!parent->scrollingNodeID() && parent->bounds().contains(point) && parent->eventRegion().contains(roundedIntPoint(point));
    for (auto& child : parent->children()) {
        Locker childLocker { child->lock() };
        FloatPoint transformedPoint(point);
        if (child->transform().isInvertible()) {
            float originX = child->anchorPoint().x() * child->size().width();
            float originY = child->anchorPoint().y() * child->size().height();
            auto transform = *(TransformationMatrix()
                .translate3d(originX + child->position().x() - parent->boundsOrigin().x(), originY + child->position().y() - parent->boundsOrigin().y(), child->anchorPoint().z())
                .multiply(child->transform())
                .translate3d(-originX, -originY, -child->anchorPoint().z()).inverse());
            auto pointInChildSpace = transform.projectPoint(point);
            transformedPoint.set(pointInChildSpace.x(), pointInChildSpace.y());
        }
        existsOnDescendent |= collectDescendantLayersAtPoint(layersAtPoint, child, transformedPoint);
    }

    if (existsOnLayer && !existsOnDescendent)
        layersAtPoint.append(parent);

    return existsOnLayer || existsOnDescendent;
}

RefPtr<ScrollingTreeNode> ScrollingTreeCoordinated::scrollingNodeForPoint(FloatPoint point)
{
    auto* rootScrollingNode = rootNode();
    if (!rootScrollingNode)
        return nullptr;

    Locker layerLocker { m_layerHitTestMutex };

    auto rootContentsLayer = static_cast<ScrollingTreeFrameScrollingNodeCoordinated*>(rootScrollingNode)->rootContentsLayer();
    Vector<Ref<CoordinatedPlatformLayer>> layersAtPoint;
    {
        Locker rootContentsLayerLocker { rootContentsLayer->lock() };
        collectDescendantLayersAtPoint(layersAtPoint, Ref { *rootContentsLayer }, point);
    }

    for (auto& layer : makeReversedRange(layersAtPoint)) {
        Locker locker { layer->lock() };
        auto* scrollingNode = nodeForID(layer->scrollingNodeID());
        if (is<ScrollingTreeScrollingNode>(scrollingNode))
            return scrollingNode;
    }

    return rootScrollingNode;
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && USE(COORDINATED_GRAPHICS)
