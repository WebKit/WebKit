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
#include <ranges>

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
    assertIsHeld(m_treeLock);
    auto* rootScrollingNode = rootNode();
    if (!rootScrollingNode)
        return;

    ThreadedScrollingTree::applyLayerPositionsInternal();

    if (ScrollingThread::isCurrentThread()) {
        auto rootContentsLayer = static_cast<ScrollingTreeFrameScrollingNodeCoordinated*>(rootScrollingNode)->rootContentsLayer();
        rootContentsLayer->requestComposition(CompositionReason::AsyncScrolling);
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

using LayerAndPoint = std::pair<Ref<CoordinatedPlatformLayer>, FloatPoint>;

static void collectDescendantLayersAtPoint(Vector<LayerAndPoint, 16>& layersAtPoint, const Ref<CoordinatedPlatformLayer>& parent, const FloatPoint& point, const std::function<bool(const Ref<CoordinatedPlatformLayer>&, const FloatPoint&)>& transformedPointFunction)
{
    Vector<Ref<CoordinatedPlatformLayer>> children;
    FloatPoint parentBoundsOrigin;
    {
        Locker parentLocker { parent->lock() };

        if (parent->masksToBounds() && !parent->bounds().contains(point))
            return;

        if (RefPtr mask = parent->mask()) {
            Locker maskLocker { mask->lock() };
            if (!mask->bounds().contains(point))
                return;
        }

        children = parent->children();
        parentBoundsOrigin = parent->boundsOrigin();
    }

    for (auto& layer : children) {
        FloatPoint transformedPoint;
        bool handlesEvent;
        bool hasChildren;
        {
            Locker layerLocker { layer->lock() };

            if (!layer->transform().isInvertible())
                continue;

            float originX = layer->anchorPoint().x() * layer->size().width();
            float originY = layer->anchorPoint().y() * layer->size().height();

            auto transform = TransformationMatrix()
                .translate3d(originX + layer->position().x() - parentBoundsOrigin.x(), originY + layer->position().y() - parentBoundsOrigin.y(), layer->anchorPoint().z())
                .multiply(layer->transform())
                .translate3d(-originX, -originY, -layer->anchorPoint().z())
                .inverse()
                .value();

            transformedPoint = transform.projectPoint(point);

            handlesEvent = [&] {
                assertIsHeld(layer->lock());
                if (layer->bounds().isEmpty())
                    return false;
                if (!layer->bounds().contains(transformedPoint))
                    return false;
                if (transformedPointFunction)
                    return transformedPointFunction(layer, transformedPoint);
                return true;
            }();

            hasChildren = !layer->children().isEmpty();
        }

        if (handlesEvent)
            layersAtPoint.append({ layer, transformedPoint });

        if (hasChildren)
            collectDescendantLayersAtPoint(layersAtPoint, layer, transformedPoint, transformedPointFunction);
    }
}

static bool layerEventRegionContainsPoint(const Ref<CoordinatedPlatformLayer>& layer, const FloatPoint& localPoint)
{
    assertIsHeld(layer->lock());
    // Scrolling changes boundsOrigin on the scroll container layer, but we computed its event region ignoring scroll position, so factor out bounds origin.
    auto originRelativePoint = localPoint - toFloatSize(layer->boundsOrigin());
    return layer->eventRegion().contains(roundedIntPoint(originRelativePoint));
}

static Vector<LayerAndPoint, 16> layersAtPointToCheckForScrolling(const Ref<CoordinatedPlatformLayer>& parent, const FloatPoint& point, bool& hasAnyNonInteractiveScrollingLayers)
{
    Vector<LayerAndPoint, 16> layersAtPoint;
    collectDescendantLayersAtPoint(layersAtPoint, parent, point, [&] (auto layer, auto transformedPoint) {
        assertIsHeld(layer->lock());
        if (layerEventRegionContainsPoint(layer, transformedPoint))
            return true;
        if (layer->scrollingNodeID()) {
            hasAnyNonInteractiveScrollingLayers = true;
            return true;
        }
        return false;
    });
    // Hit-test front to back.
    layersAtPoint.reverse();
    return layersAtPoint;
}

static bool isScrolledBy(const ScrollingTree& tree, ScrollingNodeID scrollingNodeID, const RefPtr<CoordinatedPlatformLayer>& hitLayer)
{
    for (auto layer = hitLayer; layer;) {
        Locker layerLocker { layer->lock() };

        auto nodeID = layer->scrollingNodeID();
        if (nodeID == scrollingNodeID)
            return true;

        RefPtr scrollingNode = tree.nodeForID(nodeID);
        if (RefPtr proxyNode = dynamicDowncast<ScrollingTreeOverflowScrollProxyNode>(scrollingNode)) {
            auto actingOverflowScrollingNodeID = proxyNode->overflowScrollingNodeID();
            if (actingOverflowScrollingNodeID == scrollingNodeID)
                return true;
        }

        if (RefPtr positionedNode = dynamicDowncast<ScrollingTreePositionedNode>(scrollingNode)) {
            if (positionedNode->relatedOverflowScrollingNodes().contains(scrollingNodeID))
                return false;
        }

        layer = layer->parent();
    }

    return false;
}

RefPtr<ScrollingTreeNode> ScrollingTreeCoordinated::scrollingNodeForPoint(FloatPoint point)
{
    RefPtr rootScrollingNode = rootNode();
    if (!rootScrollingNode)
        return nullptr;

    Locker locker { m_layerHitTestMutex };

    auto rootContentsLayer = static_cast<ScrollingTreeFrameScrollingNodeCoordinated*>(rootScrollingNode.get())->rootContentsLayer();
    FloatPoint scrollOrigin = rootScrollingNode->scrollOrigin();
    auto pointInContentsLayer = point;
    pointInContentsLayer.moveBy(scrollOrigin);

    bool hasAnyNonInteractiveScrollingLayers = false;
    auto layersAtPoint = layersAtPointToCheckForScrolling(*rootContentsLayer, pointInContentsLayer, hasAnyNonInteractiveScrollingLayers);

    RefPtr<CoordinatedPlatformLayer> frontmostInteractiveLayer;
    for (size_t i = 0; i < layersAtPoint.size(); ++i) {
        auto [layer, transformedPoint] = layersAtPoint[i];

        {
            Locker layerLocker { layer->lock() };
            if (!layerEventRegionContainsPoint(layer, transformedPoint))
                continue;
        }

        if (!frontmostInteractiveLayer)
            frontmostInteractiveLayer = layer.get();

        auto scrollingNodeForLayer = [&] (auto layer, auto point) -> RefPtr<ScrollingTreeNode> {
            UNUSED_PARAM(point);
            std::optional<ScrollingNodeID> nodeID;
            {
                Locker layerLocker { layer->lock() };
                nodeID = layer->scrollingNodeID();
            }
            RefPtr scrollingNode = nodeForID(nodeID);
            if (!is<ScrollingTreeScrollingNode>(scrollingNode))
                return nullptr;
            ASSERT(frontmostInteractiveLayer);
            if (isScrolledBy(*this, *nodeID, frontmostInteractiveLayer.get()))
                return scrollingNode;
            return nullptr;
        };

        if (RefPtr scrollingNode = scrollingNodeForLayer(layer, transformedPoint))
            return scrollingNode;

        // This layer may be scrolled by some other layer further back which may itself be non-interactive.
        if (hasAnyNonInteractiveScrollingLayers) {
            for (size_t j = i + 1; j < layersAtPoint.size(); ++j) {
                auto [behindLayer, behindPoint] = layersAtPoint[j];
                if (RefPtr scrollingNode = scrollingNodeForLayer(behindLayer, behindPoint))
                    return scrollingNode;
            }
        }
        // FIXME: Hit-test scroll indicator layers.
    }

    return rootScrollingNode;
}

#if HAVE(DISPLAY_LINK)
void ScrollingTreeCoordinated::hasNodeWithAnimatedScrollChanged(bool hasNodeWithAnimatedScroll)
{
    ASSERT(ScrollingThread::isCurrentThread());

    if (hasNodeWithAnimatedScroll)
        didScheduleRenderingUpdate();

    RefPtr scrollingCoordinator = m_scrollingCoordinator;
    if (!scrollingCoordinator)
        return;
    scrollingCoordinator->hasNodeWithAnimatedScrollChanged(hasNodeWithAnimatedScroll);
}
#endif

#if ENABLE(WHEEL_EVENT_REGIONS)
OptionSet<EventListenerRegionType> ScrollingTreeCoordinated::eventListenerRegionTypesForPoint(FloatPoint point) const
{
    RefPtr rootScrollingNode = rootNode();
    if (!rootScrollingNode)
        return { };

    Locker locker { m_layerHitTestMutex };

    auto rootContentsLayer = static_cast<ScrollingTreeFrameScrollingNodeCoordinated*>(rootScrollingNode.get())->rootContentsLayer();

    Vector<LayerAndPoint, 16> layersAtPoint;
    collectDescendantLayersAtPoint(layersAtPoint, *rootContentsLayer, point, [&] (auto layer, auto transformedPoint) {
        assertIsHeld(layer->lock());
        return layerEventRegionContainsPoint(layer, transformedPoint);
    });

    if (layersAtPoint.isEmpty())
        return { };

    auto [hitLayer, transformedPoint] = layersAtPoint.last();

    Locker hitLayerLocker { hitLayer->lock() };
    return hitLayer->eventRegion().eventListenerRegionTypesForPoint(roundedIntPoint(transformedPoint));
}
#endif

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && USE(COORDINATED_GRAPHICS)
