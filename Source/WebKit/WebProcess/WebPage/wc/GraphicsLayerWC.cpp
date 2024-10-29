/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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

#if USE(GRAPHICS_LAYER_WC)

#include "WCPlatformLayerGCGL.h"
#include "WCTileGrid.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/TransformState.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

class WCTiledBacking final : public TiledBacking {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WCTiledBacking);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WCTiledBacking);
public:
    WCTiledBacking(GraphicsLayerWC& owner)
        : m_owner(owner) { }

    bool paintAndFlush(WCLayerUpdateInfo& update)
    {
        bool repainted = false;
        for (auto& entry : m_tileGrid.tiles()) {
            auto& tile = *entry.value;
            WCTileUpdate tileUpdate;
            tileUpdate.index = entry.key;
            tileUpdate.willRemove = tile.willRemove();
            if (tileUpdate.willRemove) {
                update.background.tileUpdates.append(WTFMove(tileUpdate));
                continue;
            }
            if (!tile.hasDirtyRect())
                continue;
            repainted = true;
            float deviceScaleFactor = m_owner.deviceScaleFactor();
            auto& dirtyRect = tile.dirtyRect();
            IntRect scaledDirtyRect = dirtyRect;
            scaledDirtyRect.scale(deviceScaleFactor);
            tileUpdate.dirtyRect = scaledDirtyRect;
            auto image = m_owner.createImageBuffer(dirtyRect.size(), deviceScaleFactor);
            auto& context = image->context();
            context.translate(-dirtyRect.x(), -dirtyRect.y());
            m_owner.paintGraphicsLayerContents(context, dirtyRect);
            image->flushDrawingContextAsync();
            tileUpdate.backingStore.setImageBuffer(WTFMove(image));
            update.background.tileUpdates.append(WTFMove(tileUpdate));
        }
        m_tileGrid.clearDirtyRects();
        return repainted;
    }

    void setSize(const FloatSize& size)
    {
        m_tileGrid.setSize(expandedIntSize(size));
    }

    void setDirtyRect(const WebCore::FloatRect& rect)
    {
        m_tileGrid.addDirtyRect(enclosingIntRect(rect));
    }

    bool updateCoverageRect(const FloatRect& rect)
    {
        return m_tileGrid.setCoverageRect(enclosingIntRect(rect));
    }

    TiledBacking* tiledBacking() const { return const_cast<WCTiledBacking*>(this); };

    // TiledBacking override
    void setClient(TiledBackingClient*) final { }
    PlatformLayerIdentifier layerIdentifier() const final { return *m_owner.primaryLayerID(); }
    TileGridIdentifier primaryGridIdentifier() const final { return TileGridIdentifier { 0 }; }
    std::optional<TileGridIdentifier> secondaryGridIdentifier() const final { return { }; }
    void setVisibleRect(const FloatRect&) final { }
    FloatRect visibleRect() const final { return { }; };
    void setLayoutViewportRect(std::optional<FloatRect>) final { }
    void setCoverageRect(const FloatRect& rect) final { }
    FloatRect coverageRect() const final { return { }; };
    bool tilesWouldChangeForCoverageRect(const FloatRect&) const final { return false; }
    void setTiledScrollingIndicatorPosition(const FloatPoint&) final { }
    void setTopContentInset(float) final { }
    void setVelocity(const VelocityData&) final { }
    void setTileSizeUpdateDelayDisabledForTesting(bool) final { };
    void setScrollability(OptionSet<Scrollability>) final { }
    void prepopulateRect(const FloatRect&) final { }
    void setIsInWindow(bool) final { }
    bool isInWindow() const final { return m_isInWindow; }
    void setTileCoverage(TileCoverage) final { }
    TileCoverage tileCoverage() const final { return { }; };
    FloatRect adjustTileCoverageRect(const FloatRect& coverageRect, const FloatRect& previousVisibleRect, const FloatRect& currentVisibleRect, bool sizeChanged) final { return { }; };
    FloatRect adjustTileCoverageRectForScrolling(const FloatRect& coverageRect, const FloatSize& newSize, const FloatRect& previousVisibleRect, const FloatRect& currentVisibleRect, float contentsScale) final { return { }; };
    void willStartLiveResize() final { };
    void didEndLiveResize() final { };
    IntSize tileSize() const final { return m_tileGrid.tilePixelSize(); }
    FloatRect rectForTile(TileIndex) const { return { }; }
    void revalidateTiles() final { }
    IntRect tileGridExtent() const final { return { }; }
    void setScrollingPerformanceTestingEnabled(bool flag) final { }
    bool scrollingPerformanceTestingEnabled() const final { return false; };
    double retainedTileBackingStoreMemory() const final { return 0; }
    IntRect tileCoverageRect() const final { return { }; }
    void setScrollingModeIndication(ScrollingModeIndication) final { }
    void setHasMargins(bool marginTop, bool marginBottom, bool marginLeft, bool marginRight) final { }
    bool hasMargins() const final { return false; };
    bool hasHorizontalMargins() const final { return false; };
    bool hasVerticalMargins() const final { return false; };
    void setMarginSize(int) final { }
    int topMarginHeight() const final { return 0; };
    int bottomMarginHeight() const final { return 0; };
    int leftMarginWidth() const final { return 0; };
    int rightMarginWidth() const final { return 0; };
    void setZoomedOutContentsScale(float) final { }
    float zoomedOutContentsScale() const final { return 1; }
    float tilingScaleFactor() const final { return 1; }
    IntRect bounds() const final { return { { }, IntSize(m_owner.size()) }; };
    IntRect boundsWithoutMargin() const final { return bounds(); };

private:
    bool m_isInWindow { false };
    GraphicsLayerWC& m_owner;
    WCTileGrid m_tileGrid;
};


GraphicsLayerWC::GraphicsLayerWC(Type layerType, GraphicsLayerClient& client, Observer& observer)
    : GraphicsLayer(layerType, client)
    , m_observer(&observer)
{
    m_tiledBacking = makeUnique<WCTiledBacking>(*this);
    if (layerType == Type::Normal)
        client.tiledBackingUsageChanged(this, true);
    m_observer->graphicsLayerAdded(*this);
}

GraphicsLayerWC::~GraphicsLayerWC()
{
    willBeDestroyed();
    if (m_observer)
        m_observer->graphicsLayerRemoved(*this);
}

std::optional<PlatformLayerIdentifier> GraphicsLayerWC::primaryLayerID() const
{
    return m_layerID;
}

void GraphicsLayerWC::setNeedsDisplay()
{
    setNeedsDisplayInRect({ { }, m_size }, ClipToLayer);
}

void GraphicsLayerWC::setNeedsDisplayInRect(const WebCore::FloatRect& rect, ShouldClipToLayer shouldClip)
{
    if (!drawsContent())
        return;

    m_tiledBacking->setDirtyRect(rect);
    noteLayerPropertyChanged(WCLayerChange::Background);
    addRepaintRect(rect);
}

void GraphicsLayerWC::setContentsNeedsDisplay()
{
    // For example, if WebGL canvas changed, it needs flush to display.
    noteLayerPropertyChanged(WCLayerChange::PlatformLayer);
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

void GraphicsLayerWC::willModifyChildren()
{
    noteLayerPropertyChanged(WCLayerChange::Children);
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
    noteLayerPropertyChanged(WCLayerChange::Position);
}

void GraphicsLayerWC::syncPosition(const FloatPoint& point)
{
    if (point == m_position)
        return;
    GraphicsLayer::syncPosition(point);
    noteLayerPropertyChanged(WCLayerChange::Position, DontScheduleFlush);
}

void GraphicsLayerWC::setAnchorPoint(const FloatPoint3D& point)
{
    if (point == m_anchorPoint)
        return;
    GraphicsLayer::setAnchorPoint(point);
    noteLayerPropertyChanged(WCLayerChange::AnchorPoint);
}

void GraphicsLayerWC::setSize(const FloatSize& size)
{
    if (size == m_size)
        return;
    GraphicsLayer::setSize(size);
    m_tiledBacking->setSize(size);
    noteLayerPropertyChanged(WCLayerChange::Size);
}

void GraphicsLayerWC::setBoundsOrigin(const FloatPoint& origin)
{
    if (origin == m_boundsOrigin)
        return;
    GraphicsLayer::setBoundsOrigin(origin);
    noteLayerPropertyChanged(WCLayerChange::BoundsOrigin);
}

void GraphicsLayerWC::syncBoundsOrigin(const FloatPoint& origin)
{
    if (origin == m_boundsOrigin)
        return;
    GraphicsLayer::syncBoundsOrigin(origin);
    noteLayerPropertyChanged(WCLayerChange::BoundsOrigin, DontScheduleFlush);
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
    updateDebugIndicators();
}

void GraphicsLayerWC::setBackgroundColor(const WebCore::Color& value)
{
    if (value == backgroundColor())
        return;
    GraphicsLayer::setBackgroundColor(value);
    noteLayerPropertyChanged(WCLayerChange::Background);
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

void GraphicsLayerWC::setContentsRectClipsDescendants(bool contentsRectClipsDescendants)
{
    if (contentsRectClipsDescendants == m_contentsRectClipsDescendants)
        return;

    GraphicsLayer::setContentsRectClipsDescendants(contentsRectClipsDescendants);
    noteLayerPropertyChanged(WCLayerChange::ContentsRectClipsDescendants);
}

void GraphicsLayerWC::setDrawsContent(bool value)
{
    if (value == drawsContent())
        return;
    GraphicsLayer::setDrawsContent(value);
    noteLayerPropertyChanged(WCLayerChange::Background);
    updateDebugIndicators();
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
    updateDebugIndicators();
}

void GraphicsLayerWC::setContentsToPlatformLayerHost(WebCore::LayerHostingContextIdentifier identifier)
{
    if (m_hostIdentifier && *m_hostIdentifier == identifier)
        return;
    m_hostIdentifier = identifier;
    noteLayerPropertyChanged(WCLayerChange::RemoteFrame);
}

void GraphicsLayerWC::setContentsDisplayDelegate(RefPtr<WebCore::GraphicsLayerContentsDisplayDelegate>&& displayDelegate, ContentsLayerPurpose purpose)
{
    auto platformLayer = displayDelegate ? displayDelegate->platformLayer() : nullptr;
    setContentsToPlatformLayer(platformLayer, purpose);
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
    noteLayerPropertyChanged(WCLayerChange::ShowDebugBorder);
}

void GraphicsLayerWC::setDebugBorder(const Color& color, float width)
{
    m_debugBorderColor = color;
    m_debugBorderWidth = width;
    noteLayerPropertyChanged(WCLayerChange::DebugBorderColor);
    noteLayerPropertyChanged(WCLayerChange::DebugBorderWidth);
}

void GraphicsLayerWC::setShowRepaintCounter(bool show)
{
    if (isShowingRepaintCounter() == show)
        return;
    GraphicsLayer::setShowRepaintCounter(show);
    noteLayerPropertyChanged(WCLayerChange::ShowRepaintCounter);
}

static bool filtersCanBeComposited(const FilterOperations& filters)
{
    if (!filters.size())
        return false;
    for (const auto& filterOperation : filters) {
        if (filterOperation->type() == FilterOperation::Type::Reference)
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
    updateDebugIndicators();
    return canCompositeFilters;
}

void GraphicsLayerWC::setBackdropFiltersRect(const FloatRoundedRect& backdropFiltersRect)
{
    if (m_backdropFiltersRect == backdropFiltersRect)
        return;
    GraphicsLayer::setBackdropFiltersRect(backdropFiltersRect);
    noteLayerPropertyChanged(WCLayerChange::BackdropFiltersRect);
}

void GraphicsLayerWC::noteLayerPropertyChanged(OptionSet<WCLayerChange> flags, ScheduleFlushOrNot scheduleFlush)
{
    if (beingDestroyed())
        return;
    bool needsFlush = !m_uncommittedChanges;
    m_uncommittedChanges.add(flags);
    if (m_isFlushing)
        return;
    if (needsFlush && scheduleFlush == ScheduleFlush)
        client().notifyFlushRequired(this);
}

void GraphicsLayerWC::flushCompositingState(const FloatRect& passedVisibleRect)
{
    // passedVisibleRect doesn't contain the scrollbar area. Inflate it.
    FloatRect visibleRect = passedVisibleRect;
    visibleRect.inflate(20.f);
    TransformState state(TransformState::UnapplyInverseTransformDirection, FloatQuad(visibleRect));
    state.setSecondaryQuad(FloatQuad { visibleRect });
    recursiveCommitChanges(state);
}

void GraphicsLayerWC::flushCompositingStateForThisLayerOnly()
{
    if (!m_uncommittedChanges)
        return;
    WCLayerUpdateInfo update {
        .id = *primaryLayerID(),
        .changes = std::exchange(m_uncommittedChanges, { })
    };
    if (update.changes & WCLayerChange::Children) {
        update.children = WTF::map(children(), [](auto& layer) {
            return *layer->primaryLayerID();
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
    if (update.changes & WCLayerChange::Position)
        update.position = position();
    if (update.changes & WCLayerChange::AnchorPoint)
        update.anchorPoint = anchorPoint();
    if (update.changes & WCLayerChange::Size)
        update.size = size();
    if (update.changes & WCLayerChange::BoundsOrigin)
        update.boundsOrigin = boundsOrigin();
    if (update.changes & WCLayerChange::Preserves3D)
        update.preserves3D = preserves3D();
    if (update.changes & WCLayerChange::ContentsRect)
        update.contentsRect = contentsRect();
    if (update.changes & WCLayerChange::ContentsClippingRect)
        update.contentsClippingRect = contentsClippingRect();
    if (update.changes & WCLayerChange::ContentsRectClipsDescendants)
        update.contentsRectClipsDescendants = contentsRectClipsDescendants();
    if (update.changes & WCLayerChange::ContentsVisible)
        update.contentsVisible = contentsAreVisible();
    if (update.changes & WCLayerChange::BackfaceVisibility)
        update.backfaceVisibility = backfaceVisibility();
    if (update.changes & WCLayerChange::MasksToBounds)
        update.masksToBounds = masksToBounds();
    if (update.changes & WCLayerChange::Background) {
        update.background.color = backgroundColor();
        if (drawsContent() && contentsAreVisible()) {
            update.background.hasBackingStore = true;
            update.background.backingStoreSize = WebCore::expandedIntSize(size() * deviceScaleFactor());
            if (m_tiledBacking->paintAndFlush(update)) {
                incrementRepaintCount();
                update.changes.add(WCLayerChange::RepaintCount);
            }
        } else
            update.background.hasBackingStore = false;
    }
    if (update.changes & WCLayerChange::SolidColor)
        update.solidColor = m_solidColor;
    if (update.changes & WCLayerChange::ShowDebugBorder)
        update.showDebugBorder = isShowingDebugBorder();
    if (update.changes & WCLayerChange::DebugBorderColor)
        update.debugBorderColor = m_debugBorderColor;
    if (update.changes & WCLayerChange::DebugBorderWidth)
        update.debugBorderWidth = m_debugBorderWidth;
    if (update.changes & WCLayerChange::ShowRepaintCounter)
        update.showRepaintCounter = isShowingRepaintCounter();
    if (update.changes & WCLayerChange::RepaintCount)
        update.repaintCount = repaintCount();
    if (update.changes & WCLayerChange::Opacity)
        update.opacity = opacity();
    if (update.changes & WCLayerChange::Transform)
        update.transform = transform();
    if (update.changes & WCLayerChange::ChildrenTransform)
        update.childrenTransform = childrenTransform();
    if (update.changes & WCLayerChange::Filters)
        update.filters = filters();
    if (update.changes & WCLayerChange::BackdropFilters)
        update.backdropFilters = backdropFilters();
    if (update.changes & WCLayerChange::BackdropFiltersRect)
        update.backdropFiltersRect = backdropFiltersRect();
    if (update.changes & WCLayerChange::PlatformLayer) {
        update.platformLayer.hasLayer = m_platformLayer;
#if ENABLE(WEBGL)
        if (m_platformLayer)
            update.platformLayer.identifiers = static_cast<WCPlatformLayerGCGL*>(m_platformLayer)->takeContentBufferIdentifiers();
#endif
    }
    if (update.changes & WCLayerChange::RemoteFrame)
        update.hostIdentifier = m_hostIdentifier;
    m_observer->commitLayerUpdateInfo(WTFMove(update));
}

TiledBacking* GraphicsLayerWC::tiledBacking() const
{
    return m_tiledBacking->tiledBacking();
}

RefPtr<WebCore::ImageBuffer> GraphicsLayerWC::createImageBuffer(WebCore::FloatSize size, float deviceScaleFactor)
{
    return m_observer->createImageBuffer(size, deviceScaleFactor);
}

static inline bool accumulatesTransform(const GraphicsLayerWC& layer)
{
    return !layer.masksToBounds() && (layer.preserves3D() || (layer.parent() && layer.parent()->preserves3D()));
}

TransformationMatrix GraphicsLayerWC::transformByApplyingAnchorPoint(const TransformationMatrix& matrix) const
{
    if (matrix.isIdentity())
        return matrix;

    TransformationMatrix result;
    FloatPoint3D absoluteAnchorPoint(anchorPoint());
    absoluteAnchorPoint.scale(size().width(), size().height(), 1);
    result.translate3d(absoluteAnchorPoint.x(), absoluteAnchorPoint.y(), absoluteAnchorPoint.z());
    result.multiply(matrix);
    result.translate3d(-absoluteAnchorPoint.x(), -absoluteAnchorPoint.y(), -absoluteAnchorPoint.z());
    return result;
}

TransformationMatrix GraphicsLayerWC::layerTransform(const FloatPoint& position, const TransformationMatrix* customTransform) const
{
    TransformationMatrix transform;
    transform.translate(position.x(), position.y());

    const auto& currentTransform = customTransform ? *customTransform
        : (m_transform ? *m_transform : TransformationMatrix::identity);

    transform.multiply(transformByApplyingAnchorPoint(currentTransform));

    if (GraphicsLayer* parentLayer = parent()) {
        if (parentLayer->hasNonIdentityChildrenTransform()) {
            FloatPoint boundsOrigin = parentLayer->boundsOrigin();

            FloatPoint3D parentAnchorPoint(parentLayer->anchorPoint());
            parentAnchorPoint.scale(parentLayer->size().width(), parentLayer->size().height(), 1);
            parentAnchorPoint += boundsOrigin;

            transform.translateRight3d(-parentAnchorPoint.x(), -parentAnchorPoint.y(), -parentAnchorPoint.z());
            transform = parentLayer->childrenTransform() * transform;
            transform.translateRight3d(parentAnchorPoint.x(), parentAnchorPoint.y(), parentAnchorPoint.z());
        }
    }

    return transform;
}

GraphicsLayerWC::VisibleAndCoverageRects GraphicsLayerWC::computeVisibleAndCoverageRect(TransformState& state, bool preserves3D) const
{
    FloatPoint position = approximatePosition();
    client().customPositionForVisibleRectComputation(this, position);

    TransformationMatrix layerTransform;
    TransformationMatrix currentTransform;
    if (client().getCurrentTransform(this, currentTransform))
        layerTransform = this->layerTransform(position, &currentTransform);
    else
        layerTransform = this->layerTransform(position);

    bool applyWasClamped;
    TransformState::TransformAccumulation accumulation = preserves3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform;
    state.applyTransform(layerTransform, accumulation, &applyWasClamped);

    bool mapWasClamped;
    FloatRect clipRectForChildren = state.mappedQuad(&mapWasClamped).boundingBox();
    FloatPoint boundsOrigin = m_boundsOrigin;
    clipRectForChildren.move(boundsOrigin.x(), boundsOrigin.y());
    
    FloatRect clipRectForSelf(boundsOrigin, m_size);
    if (!applyWasClamped && !mapWasClamped)
        clipRectForSelf.intersect(clipRectForChildren);

    if (masksToBounds()) {
        ASSERT(accumulation == TransformState::FlattenTransform);
        // Flatten, and replace the quad in the TransformState with one that is clipped to this layer's bounds.
        state.flatten();
        state.setQuad(clipRectForSelf);
        if (state.isMappingSecondaryQuad())
            state.setSecondaryQuad(FloatQuad { clipRectForSelf });
    }

    FloatRect coverageRect = clipRectForSelf;
    auto quad = state.mappedSecondaryQuad(&mapWasClamped);
    if (quad && !mapWasClamped && !applyWasClamped)
        coverageRect = (*quad).boundingBox();

    return { clipRectForSelf, coverageRect, currentTransform };
}

void GraphicsLayerWC::recursiveCommitChanges(const TransformState& state)
{
    TransformState localState = state;
    SetForScope<bool> scopedIsFlushing(m_isFlushing, true);

    bool accumulateTransform = accumulatesTransform(*this);
    VisibleAndCoverageRects rects = computeVisibleAndCoverageRect(localState, accumulateTransform);
    bool needsToPaint = m_tiledBacking->updateCoverageRect(rects.coverageRect);
    if (needsToPaint)
        noteLayerPropertyChanged(WCLayerChange::Background, DontScheduleFlush);

    flushCompositingStateForThisLayerOnly();
    if (auto* mask = maskLayer())
        static_cast<GraphicsLayerWC*>(mask)->recursiveCommitChanges(localState);
    if (auto* replica = replicaLayer())
        static_cast<GraphicsLayerWC*>(replica)->recursiveCommitChanges(localState);
    for (auto& child : children())
        static_cast<GraphicsLayerWC*>(child.ptr())->recursiveCommitChanges(localState);
}

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
