/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GraphicsLayerWC.h"

#include "WCPlatformLayerGCGL.h"

namespace WebKit {
using namespace WebCore;

GraphicsLayerWC::GraphicsLayerWC(Type layerType, GraphicsLayerClient& client, Observer& observer)
    : GraphicsLayer(layerType, client)
    , m_observer(&observer)
{
    m_observer->graphicsLayerAdded(*this);
}

GraphicsLayerWC::~GraphicsLayerWC()
{
    willBeDestroyed();
    if (m_observer)
        m_observer->graphicsLayerRemoved(*this);
}

GraphicsLayer::PlatformLayerID GraphicsLayerWC::generateLayerID()
{
    // 0 and max can't be used for hash keys
    static GraphicsLayer::PlatformLayerID id = 1;
    return id++;
}

GraphicsLayer::PlatformLayerID GraphicsLayerWC::primaryLayerID() const
{
    return m_layerID;
}

void GraphicsLayerWC::setNeedsDisplay()
{
    if (!drawsContent())
        return;
    noteLayerPropertyChanged(WCLayerChange::BackingStore);
    addRepaintRect({ { }, m_size });
}

void GraphicsLayerWC::setNeedsDisplayInRect(const WebCore::FloatRect& rect, ShouldClipToLayer)
{
    if (!drawsContent())
        return;
    noteLayerPropertyChanged(WCLayerChange::BackingStore);
    addRepaintRect(rect);
}

void GraphicsLayerWC::setContentsNeedsDisplay()
{
    // For example, if WebGL canvas changed, it needs flush to display.
    noteLayerPropertyChanged({ });
}

bool GraphicsLayerWC::setChildren(Vector<Ref<GraphicsLayer>>&& children)
{
    bool childrenChanged = GraphicsLayer::setChildren(WTFMove(children));
    if (childrenChanged)
        noteLayerPropertyChanged(WCLayerChange::Children);
    return childrenChanged;
}

void GraphicsLayerWC::addChild(Ref<GraphicsLayer>&& childLayer)
{
    GraphicsLayer::addChild(WTFMove(childLayer));
    noteLayerPropertyChanged(WCLayerChange::Children);
}

void GraphicsLayerWC::addChildAtIndex(Ref<GraphicsLayer>&& childLayer, int index)
{
    GraphicsLayer::addChildAtIndex(WTFMove(childLayer), index);
    noteLayerPropertyChanged(WCLayerChange::Children);
}

void GraphicsLayerWC::addChildBelow(Ref<GraphicsLayer>&& childLayer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(WTFMove(childLayer), sibling);
    noteLayerPropertyChanged(WCLayerChange::Children);
}

void GraphicsLayerWC::addChildAbove(Ref<GraphicsLayer>&& childLayer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildAbove(WTFMove(childLayer), sibling);
    noteLayerPropertyChanged(WCLayerChange::Children);
}

bool GraphicsLayerWC::replaceChild(GraphicsLayer* oldChild, Ref<GraphicsLayer>&& newChild)
{
    if (GraphicsLayer::replaceChild(oldChild, WTFMove(newChild))) {
        noteLayerPropertyChanged(WCLayerChange::Children);
        return true;
    }
    return false;
}

void GraphicsLayerWC::removeFromParent()
{
    if (m_parent)
        static_cast<GraphicsLayerWC*>(m_parent)->noteLayerPropertyChanged(WCLayerChange::Children);
    GraphicsLayer::removeFromParent();
}

void GraphicsLayerWC::setMaskLayer(RefPtr<GraphicsLayer>&& layer)
{
    if (layer == m_maskLayer)
        return;
    GraphicsLayer::setMaskLayer(WTFMove(layer));
    noteLayerPropertyChanged(WCLayerChange::MaskLayer);
}

void GraphicsLayerWC::setReplicatedLayer(GraphicsLayer* layer)
{
    if (layer == m_replicatedLayer)
        return;
    GraphicsLayer::setReplicatedLayer(layer);
    noteLayerPropertyChanged(WCLayerChange::ReplicaLayer);
}

void GraphicsLayerWC::setReplicatedByLayer(RefPtr<GraphicsLayer>&& layer)
{
    if (layer == m_replicaLayer)
        return;
    GraphicsLayer::setReplicatedByLayer(WTFMove(layer));
    noteLayerPropertyChanged(WCLayerChange::ReplicaLayer);
}

void GraphicsLayerWC::setPosition(const FloatPoint& point)
{
    if (point == m_position)
        return;
    GraphicsLayer::setPosition(point);
    noteLayerPropertyChanged(WCLayerChange::Geometry);
}

void GraphicsLayerWC::setAnchorPoint(const FloatPoint3D& point)
{
    if (point == m_anchorPoint)
        return;
    GraphicsLayer::setAnchorPoint(point);
    noteLayerPropertyChanged(WCLayerChange::Geometry);
}

void GraphicsLayerWC::setSize(const FloatSize& size)
{
    if (size == m_size)
        return;
    GraphicsLayer::setSize(size);
    noteLayerPropertyChanged(WCLayerChange::Geometry);
}

void GraphicsLayerWC::setBoundsOrigin(const FloatPoint& origin)
{
    if (origin == m_boundsOrigin)
        return;
    GraphicsLayer::setBoundsOrigin(origin);
    noteLayerPropertyChanged(WCLayerChange::Geometry);
}

void GraphicsLayerWC::setTransform(const TransformationMatrix& t)
{
    if (t == transform())
        return;
    GraphicsLayer::setTransform(t);
    noteLayerPropertyChanged(WCLayerChange::Transform);
}

void GraphicsLayerWC::setChildrenTransform(const TransformationMatrix& t)
{
    if (t == childrenTransform())
        return;
    GraphicsLayer::setChildrenTransform(t);
    noteLayerPropertyChanged(WCLayerChange::ChildrenTransform);
}

void GraphicsLayerWC::setPreserves3D(bool value)
{
    if (value == preserves3D())
        return;
    GraphicsLayer::setPreserves3D(value);
    noteLayerPropertyChanged(WCLayerChange::Preserves3D);
}

void GraphicsLayerWC::setMasksToBounds(bool value)
{
    if (value == masksToBounds())
        return;
    GraphicsLayer::setMasksToBounds(value);
    noteLayerPropertyChanged(WCLayerChange::MasksToBounds);
}

void GraphicsLayerWC::setOpacity(float value)
{
    if (value == opacity())
        return;
    GraphicsLayer::setOpacity(value);
    noteLayerPropertyChanged(WCLayerChange::Opacity);
}

void GraphicsLayerWC::setContentsRect(const FloatRect& value)
{
    if (value == contentsRect())
        return;
    GraphicsLayer::setContentsRect(value);
    noteLayerPropertyChanged(WCLayerChange::ContentsRect);
}

void GraphicsLayerWC::setContentsClippingRect(const FloatRoundedRect& value)
{
    if (value == contentsClippingRect())
        return;
    GraphicsLayer::setContentsClippingRect(value);
    noteLayerPropertyChanged(WCLayerChange::ContentsClippingRect);
}

void GraphicsLayerWC::setDrawsContent(bool value)
{
    if (value == drawsContent())
        return;
    GraphicsLayer::setDrawsContent(value);
    noteLayerPropertyChanged(WCLayerChange::BackingStore);
}

void GraphicsLayerWC::setContentsVisible(bool value)
{
    if (value == contentsAreVisible())
        return;
    GraphicsLayer::setContentsVisible(value);
    noteLayerPropertyChanged(WCLayerChange::ContentsVisible);
}

void GraphicsLayerWC::setBackfaceVisibility(bool value)
{
    if (value == backfaceVisibility())
        return;
    GraphicsLayer::setBackfaceVisibility(value);
    noteLayerPropertyChanged(WCLayerChange::BackfaceVisibility);
}

void GraphicsLayerWC::setContentsToSolidColor(const Color& color)
{
    if (color == m_solidColor)
        return;
    m_solidColor = color;
    noteLayerPropertyChanged(WCLayerChange::SolidColor);
}

void GraphicsLayerWC::setContentsToPlatformLayer(PlatformLayer* platformLayer, ContentsLayerPurpose)
{
    if (m_platformLayer == platformLayer)
        return;
    m_platformLayer = platformLayer;
    noteLayerPropertyChanged(WCLayerChange::PlatformLayer);
}

bool GraphicsLayerWC::usesContentsLayer() const
{
    return m_platformLayer;
}

void GraphicsLayerWC::setShowDebugBorder(bool show)
{
    if (isShowingDebugBorder() == show)
        return;
    GraphicsLayer::setShowDebugBorder(show);
    updateDebugIndicators();
    noteLayerPropertyChanged(WCLayerChange::DebugVisuals);
}

void GraphicsLayerWC::setDebugBorder(const Color& color, float width)
{
    m_debugBorderColor = color;
    m_debugBorderWidth = width;
    noteLayerPropertyChanged(WCLayerChange::DebugVisuals);
}

void GraphicsLayerWC::setShowRepaintCounter(bool show)
{
    if (isShowingRepaintCounter() == show)
        return;
    GraphicsLayer::setShowRepaintCounter(show);
    noteLayerPropertyChanged(WCLayerChange::RepaintCount);
}

static bool filtersCanBeComposited(const FilterOperations& filters)
{
    if (!filters.size())
        return false;
    for (const auto& filterOperation : filters.operations()) {
        if (filterOperation->type() == FilterOperation::REFERENCE)
            return false;
    }
    return true;
}

bool GraphicsLayerWC::setFilters(const FilterOperations& filters)
{
    bool canCompositeFilters = filtersCanBeComposited(filters);
    if (GraphicsLayer::filters() == filters)
        return canCompositeFilters;

    if (canCompositeFilters) {
        if (!GraphicsLayer::setFilters(filters))
            return false;
    } else if (GraphicsLayer::filters().size())
        clearFilters();

    noteLayerPropertyChanged(WCLayerChange::Filters);
    return canCompositeFilters;
}

bool GraphicsLayerWC::setBackdropFilters(const FilterOperations& filters)
{
    bool canCompositeFilters = filtersCanBeComposited(filters);
    if (m_backdropFilters == filters)
        return canCompositeFilters;
    if (canCompositeFilters)
        GraphicsLayer::setBackdropFilters(filters);
    else
        clearBackdropFilters();
    noteLayerPropertyChanged(WCLayerChange::BackdropFilters);
    return canCompositeFilters;
}

void GraphicsLayerWC::setBackdropFiltersRect(const FloatRoundedRect& backdropFiltersRect)
{
    if (m_backdropFiltersRect == backdropFiltersRect)
        return;
    GraphicsLayer::setBackdropFiltersRect(backdropFiltersRect);
    noteLayerPropertyChanged(WCLayerChange::BackdropFilters);
}

void GraphicsLayerWC::noteLayerPropertyChanged(OptionSet<WCLayerChange> flags)
{
    if (beingDestroyed())
        return;
    bool needsFlush = !m_uncommittedChanges;
    m_uncommittedChanges.add(flags);
    if (needsFlush)
        client().notifyFlushRequired(this);
}

void GraphicsLayerWC::flushCompositingState(const FloatRect& rect)
{
    if (auto* mask = maskLayer())
        mask->flushCompositingStateForThisLayerOnly();
    if (auto* replica = replicaLayer())
        replica->flushCompositingStateForThisLayerOnly();
    flushCompositingStateForThisLayerOnly();
    for (auto& child : children())
        child->flushCompositingState(rect);
}

void GraphicsLayerWC::flushCompositingStateForThisLayerOnly()
{
    if (!m_uncommittedChanges)
        return;
    WCLayerUpateInfo update;
    update.id = primaryLayerID();
    update.changes = std::exchange(m_uncommittedChanges, { });
    if (update.changes & WCLayerChange::Children) {
        update.children = WTF::map(children(), [](auto& layer) {
            return layer->primaryLayerID();
        });
    }
    if (update.changes & WCLayerChange::MaskLayer) {
        if (maskLayer())
            update.maskLayer = maskLayer()->primaryLayerID();
        else
            update.maskLayer = std::nullopt;
    }
    if (update.changes & WCLayerChange::ReplicaLayer) {
        if (replicaLayer())
            update.replicaLayer = replicaLayer()->primaryLayerID();
        else
            update.replicaLayer = std::nullopt;
    }
    if (update.changes & WCLayerChange::Geometry) {
        update.position = position();
        update.anchorPoint = anchorPoint();
        update.size = size();
        update.boundsOrigin = boundsOrigin();
    }
    if (update.changes & WCLayerChange::Preserves3D)
        update.preserves3D = preserves3D();
    if (update.changes & WCLayerChange::ContentsRect)
        update.contentsRect = contentsRect();
    if (update.changes & WCLayerChange::ContentsClippingRect)
        update.contentsClippingRect = contentsClippingRect();
    if (update.changes & WCLayerChange::ContentsVisible)
        update.contentsVisible = contentsAreVisible();
    if (update.changes & WCLayerChange::BackfaceVisibility)
        update.backfaceVisibility = backfaceVisibility();
    if (update.changes & WCLayerChange::MasksToBounds)
        update.masksToBounds = masksToBounds();
    if (update.changes & WCLayerChange::BackingStore) {
        if (drawsContent() && contentsAreVisible() && !size().isEmpty()) {
            auto image = m_observer->createImageBuffer(size());
            FloatRect clipRect = { { }, size() };
            paintGraphicsLayerContents(image->context(), clipRect);
            image->flushDrawingContextAsync();
            update.backingStore.setImageBuffer(WTFMove(image));

            incrementRepaintCount();
            update.changes.add(WCLayerChange::RepaintCount);
        }
    }
    if (update.changes & WCLayerChange::SolidColor)
        update.solidColor = m_solidColor;
    if (update.changes & WCLayerChange::DebugVisuals) {
        update.showDebugBorder = isShowingDebugBorder();
        update.debugBorderColor = m_debugBorderColor;
        update.debugBorderWidth = m_debugBorderWidth;
    }
    if (update.changes & WCLayerChange::RepaintCount) {
        update.showRepaintCounter = isShowingRepaintCounter();
        update.repaintCount = repaintCount();
    }
    if (update.changes & WCLayerChange::Opacity)
        update.opacity = opacity();
    if (update.changes & WCLayerChange::Transform)
        update.transform = transform();
    if (update.changes & WCLayerChange::ChildrenTransform)
        update.childrenTransform = childrenTransform();
    if (update.changes & WCLayerChange::Filters)
        update.filters = filters();
    if (update.changes & WCLayerChange::BackdropFilters) {
        update.backdropFilters = backdropFilters();
        update.backdropFiltersRect = backdropFiltersRect();
    }
    if (update.changes & WCLayerChange::PlatformLayer) {
        if (m_platformLayer)
            update.graphicsContextGLIdentifier = static_cast<WCPlatformLayerGCGL*>(m_platformLayer)->graphicsContextGLIdentifier().toUInt64();
        else
            update.graphicsContextGLIdentifier = 0;
    }
    m_observer->commitLayerUpateInfo(WTFMove(update));
}

} // namespace WebKit
