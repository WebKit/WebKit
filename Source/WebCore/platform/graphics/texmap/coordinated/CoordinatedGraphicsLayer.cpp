/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 Copyright (C) 2010-2022 Apple Inc. All rights reserved.
 Copyright (C) 2012 Company 100, Inc.
 Copyright (C) 2012 Intel Corporation. All rights reserved.
 Copyright (C) 2017 Sony Interactive Entertainment Inc.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CoordinatedGraphicsLayer.h"

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedAnimatedBackingStoreClient.h"
#include "CoordinatedBackingStoreProxy.h"
#include "CoordinatedImageBackingStore.h"
#include "CoordinatedTileBuffer.h"
#include "FloatQuad.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerAsyncContentsDisplayDelegateTextureMapper.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "GraphicsLayerFactory.h"
#include "ScrollableArea.h"
#include "TextureMapperPlatformLayerProxyProvider.h"
#include "TransformOperation.h"
#include <algorithm>
#ifndef NDEBUG
#include <wtf/SetForScope.h>
#endif
#include <wtf/SystemTracing.h>
#include <wtf/text/CString.h>

#if USE(CAIRO)
#include "CairoUtilities.h"
#endif

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebCore {

Ref<GraphicsLayer> GraphicsLayer::create(GraphicsLayerFactory* factory, GraphicsLayerClient& client, Type layerType)
{
    if (!factory)
        return adoptRef(*new CoordinatedGraphicsLayer(layerType, client));

    return factory->createGraphicsLayer(layerType, client);
}

void CoordinatedGraphicsLayer::notifyFlushRequired()
{
    if (!m_coordinator)
        return;

    if (m_coordinator->isFlushingLayerChanges())
        return;

    client().notifyFlushRequired(this);
}

void CoordinatedGraphicsLayer::didChangeAnimations()
{
    m_animations.setTranslate(client().transformMatrixForProperty(AnimatedProperty::Translate));
    m_animations.setRotate(client().transformMatrixForProperty(AnimatedProperty::Rotate));
    m_animations.setScale(client().transformMatrixForProperty(AnimatedProperty::Scale));
    m_animations.setTransform(client().transformMatrixForProperty(AnimatedProperty::Transform));

    m_nicosia.delta.animationsChanged = true;
    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::didChangeChildren()
{
    m_nicosia.delta.childrenChanged = true;
    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::didChangeFilters()
{
    m_nicosia.delta.filtersChanged = true;
    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::didChangeBackdropFilters()
{
    m_nicosia.delta.backdropFiltersChanged = true;
    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::didChangeBackdropFiltersRect()
{
    m_nicosia.delta.backdropFiltersRectChanged = true;
    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::didUpdateTileBuffers()
{
    if (!isShowingRepaintCounter())
        return;

    auto repaintCount = incrementRepaintCount();
    m_nicosia.repaintCounter.count = repaintCount;
    m_nicosia.delta.repaintCounterChanged = true;
}

void CoordinatedGraphicsLayer::setShouldUpdateVisibleRect()
{
    m_shouldUpdateVisibleRect = true;
    for (auto& child : children())
        downcast<CoordinatedGraphicsLayer>(child.get()).setShouldUpdateVisibleRect();
    if (replicaLayer())
        downcast<CoordinatedGraphicsLayer>(*replicaLayer()).setShouldUpdateVisibleRect();
    if (m_backdropLayer)
        m_backdropLayer->setShouldUpdateVisibleRect();
}

void CoordinatedGraphicsLayer::didChangeGeometry(FlushNotification flushNotification)
{
    if (flushNotification == FlushNotification::Required)
        notifyFlushRequired();
    setShouldUpdateVisibleRect();
}

CoordinatedGraphicsLayer::CoordinatedGraphicsLayer(Type layerType, GraphicsLayerClient& client)
    : GraphicsLayer(layerType, client)
#ifndef NDEBUG
    , m_isPurging(false)
#endif
    , m_shouldUpdateVisibleRect(true)
    , m_movingVisibleRect(false)
    , m_pendingContentsScaleAdjustment(false)
    , m_pendingVisibleRectAdjustment(false)
    , m_coordinator(0)
    , m_animationStartedTimer(*this, &CoordinatedGraphicsLayer::animationStartedTimerFired)
    , m_requestPendingTileCreationTimer(RunLoop::main(), this, &CoordinatedGraphicsLayer::requestPendingTileCreationTimerFired)
{
    static Nicosia::PlatformLayer::LayerID nextLayerID = 1;
    m_id = nextLayerID++;

    m_nicosia.layer = Nicosia::CompositionLayer::create(m_id);

    // Enforce a complete flush on the first occasion.
    m_nicosia.delta.value = UINT_MAX;

#if USE(GLIB_EVENT_LOOP)
    m_requestPendingTileCreationTimer.setPriority(RunLoopSourcePriority::LayerFlushTimer);
#endif
}

CoordinatedGraphicsLayer::~CoordinatedGraphicsLayer()
{
    if (m_coordinator) {
        purgeBackingStores();
        if (m_backdropLayer)
            m_coordinator->detachLayer(m_backdropLayer.get());
        m_coordinator->detachLayer(this);
    }
    ASSERT(!m_imageBacking.store);
    ASSERT(!m_backingStore);
    ASSERT(!m_animatedBackingStoreClient);

    if (CoordinatedGraphicsLayer* parentLayer = downcast<CoordinatedGraphicsLayer>(parent()))
        parentLayer->didChangeChildren();
    willBeDestroyed();
}

bool CoordinatedGraphicsLayer::isCoordinatedGraphicsLayer() const
{
    return true;
}

Nicosia::PlatformLayer::LayerID CoordinatedGraphicsLayer::id() const
{
    return m_id;
}

auto CoordinatedGraphicsLayer::primaryLayerID() const -> std::optional<PlatformLayerIdentifier>
{
    return PlatformLayerIdentifier { ObjectIdentifier<PlatformLayerIdentifierType>(id()), Process::identifier() };
}

bool CoordinatedGraphicsLayer::setChildren(Vector<Ref<GraphicsLayer>>&& children)
{
    bool ok = GraphicsLayer::setChildren(WTFMove(children));
    if (!ok)
        return false;
    didChangeChildren();
    return true;
}

void CoordinatedGraphicsLayer::addChild(Ref<GraphicsLayer>&& layer)
{
    GraphicsLayer* rawLayer = layer.ptr();
    GraphicsLayer::addChild(WTFMove(layer));
    downcast<CoordinatedGraphicsLayer>(*rawLayer).setCoordinatorIncludingSubLayersIfNeeded(m_coordinator);
    didChangeChildren();
}

void CoordinatedGraphicsLayer::addChildAtIndex(Ref<GraphicsLayer>&& layer, int index)
{
    GraphicsLayer* rawLayer = layer.ptr();
    GraphicsLayer::addChildAtIndex(WTFMove(layer), index);
    downcast<CoordinatedGraphicsLayer>(*rawLayer).setCoordinatorIncludingSubLayersIfNeeded(m_coordinator);
    didChangeChildren();
}

void CoordinatedGraphicsLayer::addChildAbove(Ref<GraphicsLayer>&& layer, GraphicsLayer* sibling)
{
    GraphicsLayer* rawLayer = layer.ptr();
    GraphicsLayer::addChildAbove(WTFMove(layer), sibling);
    downcast<CoordinatedGraphicsLayer>(*rawLayer).setCoordinatorIncludingSubLayersIfNeeded(m_coordinator);
    didChangeChildren();
}

void CoordinatedGraphicsLayer::addChildBelow(Ref<GraphicsLayer>&& layer, GraphicsLayer* sibling)
{
    GraphicsLayer* rawLayer = layer.ptr();
    GraphicsLayer::addChildBelow(WTFMove(layer), sibling);
    downcast<CoordinatedGraphicsLayer>(*rawLayer).setCoordinatorIncludingSubLayersIfNeeded(m_coordinator);
    didChangeChildren();
}

bool CoordinatedGraphicsLayer::replaceChild(GraphicsLayer* oldChild, Ref<GraphicsLayer>&& newChild)
{
    GraphicsLayer* rawLayer = newChild.ptr();
    bool ok = GraphicsLayer::replaceChild(oldChild, WTFMove(newChild));
    if (!ok)
        return false;
    downcast<CoordinatedGraphicsLayer>(*rawLayer).setCoordinatorIncludingSubLayersIfNeeded(m_coordinator);
    didChangeChildren();
    return true;
}

void CoordinatedGraphicsLayer::willModifyChildren()
{
    didChangeChildren();
}

void CoordinatedGraphicsLayer::setEventRegion(EventRegion&& eventRegion)
{
    if (eventRegion == m_eventRegion)
        return;

    GraphicsLayer::setEventRegion(WTFMove(eventRegion));
    m_nicosia.delta.eventRegionChanged = true;

    notifyFlushRequired();
}

#if ENABLE(SCROLLING_THREAD)
void CoordinatedGraphicsLayer::setScrollingNodeID(std::optional<ScrollingNodeID> nodeID)
{
    if (scrollingNodeID() == nodeID)
        return;

    GraphicsLayer::setScrollingNodeID(nodeID);
    m_nicosia.delta.scrollingNodeChanged = true;
}
#endif // ENABLE(SCROLLING_THREAD)

void CoordinatedGraphicsLayer::setPosition(const FloatPoint& p)
{
    if (position() == p)
        return;

    GraphicsLayer::setPosition(p);
    m_nicosia.delta.positionChanged = true;
    didChangeGeometry();
}

void CoordinatedGraphicsLayer::syncPosition(const FloatPoint& p)
{
    if (position() == p)
        return;

    GraphicsLayer::syncPosition(p);
    didChangeGeometry(FlushNotification::NotRequired);
}

void CoordinatedGraphicsLayer::setAnchorPoint(const FloatPoint3D& p)
{
    if (anchorPoint() == p)
        return;

    GraphicsLayer::setAnchorPoint(p);
    m_nicosia.delta.anchorPointChanged = true;
    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setSize(const FloatSize& size)
{
    if (this->size() == size)
        return;

    GraphicsLayer::setSize(size);
    m_nicosia.delta.sizeChanged = true;

    if (maskLayer())
        maskLayer()->setSize(size);
    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setBoundsOrigin(const FloatPoint& boundsOrigin)
{
    if (this->boundsOrigin() == boundsOrigin)
        return;

    GraphicsLayer::setBoundsOrigin(boundsOrigin);
    m_nicosia.delta.boundsOriginChanged = true;
    didChangeGeometry();
}

void CoordinatedGraphicsLayer::syncBoundsOrigin(const FloatPoint& boundsOrigin)
{
    if (this->boundsOrigin() == boundsOrigin)
        return;

    GraphicsLayer::syncBoundsOrigin(boundsOrigin);
    didChangeGeometry(FlushNotification::NotRequired);
}

void CoordinatedGraphicsLayer::setTransform(const TransformationMatrix& t)
{
    if (transform() == t)
        return;

    GraphicsLayer::setTransform(t);
    m_nicosia.delta.transformChanged = true;

    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setChildrenTransform(const TransformationMatrix& t)
{
    if (childrenTransform() == t)
        return;

    GraphicsLayer::setChildrenTransform(t);
    m_nicosia.delta.childrenTransformChanged = true;

    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setPreserves3D(bool b)
{
    if (preserves3D() == b)
        return;

    GraphicsLayer::setPreserves3D(b);
    m_nicosia.delta.flagsChanged = true;

    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setMasksToBounds(bool b)
{
    if (masksToBounds() == b)
        return;
    GraphicsLayer::setMasksToBounds(b);
    m_nicosia.delta.flagsChanged = true;

    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setDrawsContent(bool b)
{
    if (drawsContent() == b)
        return;
    GraphicsLayer::setDrawsContent(b);
    m_nicosia.delta.flagsChanged = true;

    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::setContentsVisible(bool b)
{
    if (contentsAreVisible() == b)
        return;
    GraphicsLayer::setContentsVisible(b);
    m_nicosia.delta.flagsChanged = true;

    if (maskLayer())
        maskLayer()->setContentsVisible(b);

    if (m_backdropLayer)
        m_backdropLayer->setContentsVisible(b);

    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::setContentsOpaque(bool b)
{
    if (contentsOpaque() == b)
        return;

    GraphicsLayer::setContentsOpaque(b);
    m_nicosia.delta.flagsChanged = true;

    // Demand a repaint of the whole layer.
    if (!m_needsDisplay.completeLayer) {
        m_needsDisplay.completeLayer = true;
        m_needsDisplay.rects.clear();
        m_nicosia.delta.damageChanged = true;

        addRepaintRect({ { }, m_size });
    }

    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::setBackfaceVisibility(bool b)
{
    if (backfaceVisibility() == b)
        return;

    GraphicsLayer::setBackfaceVisibility(b);
    m_nicosia.delta.flagsChanged = true;

    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::setOpacity(float opacity)
{
    if (this->opacity() == opacity)
        return;

    GraphicsLayer::setOpacity(opacity);
    m_nicosia.delta.opacityChanged = true;

    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::setContentsRect(const FloatRect& r)
{
    if (contentsRect() == r)
        return;

    GraphicsLayer::setContentsRect(r);
    m_nicosia.delta.contentsRectChanged = true;

    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::setContentsTileSize(const FloatSize& s)
{
    if (contentsTileSize() == s)
        return;

    GraphicsLayer::setContentsTileSize(s);
    m_nicosia.delta.contentsTilingChanged = true;
    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::setContentsTilePhase(const FloatSize& p)
{
    if (contentsTilePhase() == p)
        return;

    GraphicsLayer::setContentsTilePhase(p);
    m_nicosia.delta.contentsTilingChanged = true;
    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::setContentsClippingRect(const FloatRoundedRect& roundedRect)
{
    if (contentsClippingRect() == roundedRect)
        return;

    GraphicsLayer::setContentsClippingRect(roundedRect);
    m_nicosia.delta.contentsClippingRectChanged = true;
    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::setContentsRectClipsDescendants(bool clips)
{
    if (contentsRectClipsDescendants() == clips)
        return;

    GraphicsLayer::setContentsRectClipsDescendants(clips);
    m_nicosia.delta.flagsChanged = true;
    notifyFlushRequired();
}

bool GraphicsLayer::supportsContentsTiling()
{
    return true;
}

void CoordinatedGraphicsLayer::setContentsNeedsDisplay()
{
    if (m_contentsLayer)
        m_contentsLayerNeedsUpdate = true;

    notifyFlushRequired();
    addRepaintRect(contentsRect());
}

void CoordinatedGraphicsLayer::markDamageRectsUnreliable()
{
    if (m_damagedRectsAreUnreliable)
        return;

    m_damagedRectsAreUnreliable = true;
    m_nicosia.delta.damageChanged = true;
}

void CoordinatedGraphicsLayer::setContentsToPlatformLayer(PlatformLayer* platformLayer, ContentsLayerPurpose)
{
    if (m_contentsLayer.get() == platformLayer)
        return;

    m_contentsLayer = platformLayer;
    m_nicosia.delta.contentLayerChanged = true;
    if (m_contentsLayer)
        m_contentsLayerNeedsUpdate = true;
    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::setContentsDisplayDelegate(RefPtr<GraphicsLayerContentsDisplayDelegate>&& displayDelegate, ContentsLayerPurpose purpose)
{
    PlatformLayer* platformLayer = displayDelegate ? displayDelegate->platformLayer() : nullptr;
    setContentsToPlatformLayer(platformLayer, purpose);
}

RefPtr<GraphicsLayerAsyncContentsDisplayDelegate> CoordinatedGraphicsLayer::createAsyncContentsDisplayDelegate(GraphicsLayerAsyncContentsDisplayDelegate* existing)
{
    if (existing) {
        static_cast<GraphicsLayerAsyncContentsDisplayDelegateTextureMapper*>(existing)->updateGraphicsLayer(*this);
        return existing;
    }
    return GraphicsLayerAsyncContentsDisplayDelegateTextureMapper::create(*this);
}

bool CoordinatedGraphicsLayer::filtersCanBeComposited(const FilterOperations& filters) const
{
    if (!filters.size())
        return false;

    return !filters.hasReferenceFilter();
}

bool CoordinatedGraphicsLayer::setFilters(const FilterOperations& newFilters)
{
    bool canCompositeFilters = filtersCanBeComposited(newFilters);
    if (filters() == newFilters)
        return canCompositeFilters;

    if (canCompositeFilters) {
        if (!GraphicsLayer::setFilters(newFilters))
            return false;
        didChangeFilters();
    } else if (filters().size()) {
        clearFilters();
        didChangeFilters();
    }

    return canCompositeFilters;
}

bool CoordinatedGraphicsLayer::setBackdropFilters(const FilterOperations& filters)
{
    bool canCompositeFilters = filtersCanBeComposited(filters);
    if (m_backdropFilters == filters)
        return canCompositeFilters;

    if (canCompositeFilters) {
        if (!GraphicsLayer::setBackdropFilters(filters))
            return false;
    } else
        clearBackdropFilters();

    didChangeBackdropFilters();

    return canCompositeFilters;
}

void CoordinatedGraphicsLayer::setBackdropFiltersRect(const FloatRoundedRect& backdropFiltersRect)
{
    if (m_backdropFiltersRect == backdropFiltersRect)
        return;

    GraphicsLayer::setBackdropFiltersRect(backdropFiltersRect);
    didChangeBackdropFiltersRect();
}

void CoordinatedGraphicsLayer::setContentsToSolidColor(const Color& color)
{
    if (m_solidColor == color)
        return;

    m_solidColor = color;
    m_nicosia.delta.solidColorChanged = true;

    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::setShowDebugBorder(bool show)
{
    if (isShowingDebugBorder() == show)
        return;

    GraphicsLayer::setShowDebugBorder(show);
    m_nicosia.debugBorder.visible = show;
    m_nicosia.delta.debugBorderChanged = true;

    if (m_nicosia.debugBorder.visible)
        updateDebugIndicators();

    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::setShowRepaintCounter(bool show)
{
    if (isShowingRepaintCounter() == show)
        return;

    GraphicsLayer::setShowRepaintCounter(show);
    m_nicosia.repaintCounter.visible = show;
    m_nicosia.delta.repaintCounterChanged = true;

    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::setContentsToImage(Image* image)
{
    if (image) {
        auto nativeImage = image->currentNativeImage();
        if (!nativeImage)
            return;

        if (m_imageBacking.store && m_imageBacking.store->isSameNativeImage(*nativeImage))
            return;

        m_pendingContentsImage = WTFMove(nativeImage);
    } else
        m_pendingContentsImage = nullptr;

    m_imageBacking.store = nullptr;

    GraphicsLayer::setContentsToImage(image);
    m_nicosia.delta.imageBackingChanged = true;

    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::setMaskLayer(RefPtr<GraphicsLayer>&& layer)
{
    if (layer == maskLayer())
        return;

    GraphicsLayer* rawLayer = layer.get();
    GraphicsLayer::setMaskLayer(WTFMove(layer));

    if (!rawLayer)
        return;

    rawLayer->setSize(size());
    rawLayer->setContentsVisible(contentsAreVisible());

    m_nicosia.delta.maskChanged = true;

    notifyFlushRequired();
}

bool CoordinatedGraphicsLayer::shouldDirectlyCompositeImage(Image* image) const
{
    if (!image || !image->isBitmapImage())
        return false;

    static constexpr float MaxDimenstionForDirectCompositing = 2000;
    if (image->width() > MaxDimenstionForDirectCompositing || image->height() > MaxDimenstionForDirectCompositing)
        return false;

    return true;
}

void CoordinatedGraphicsLayer::setReplicatedByLayer(RefPtr<GraphicsLayer>&& layer)
{
    if (layer == replicaLayer())
        return;

    GraphicsLayer::setReplicatedByLayer(WTFMove(layer));
    m_nicosia.delta.replicaChanged = true;
    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::setNeedsDisplay()
{
    if (!drawsContent() || !contentsAreVisible() || m_size.isEmpty() || m_needsDisplay.completeLayer)
        return;

    m_needsDisplay.completeLayer = true;
    m_needsDisplay.rects.clear();
    m_nicosia.delta.damageChanged = true;

    notifyFlushRequired();
    addRepaintRect({ { }, m_size });
}

void CoordinatedGraphicsLayer::setNeedsDisplayInRect(const FloatRect& initialRect, ShouldClipToLayer shouldClip)
{
    if (!drawsContent() || !contentsAreVisible() || m_size.isEmpty() || m_needsDisplay.completeLayer)
        return;

    auto rect = initialRect;
    if (shouldClip == ClipToLayer)
        rect.intersect({ { }, m_size });

    if (rect.isEmpty())
        return;

    auto& rects = m_needsDisplay.rects;
    bool alreadyRecorded = std::any_of(rects.begin(), rects.end(),
        [&](auto& dirtyRect) { return dirtyRect.contains(rect); });
    if (alreadyRecorded)
        return;

    rects.append(rect);
    m_nicosia.delta.damageChanged = true;

    notifyFlushRequired();
    addRepaintRect(rect);
}

void CoordinatedGraphicsLayer::flushCompositingState(const FloatRect& rect)
{
    if (CoordinatedGraphicsLayer* mask = downcast<CoordinatedGraphicsLayer>(maskLayer()))
        mask->flushCompositingStateForThisLayerOnly();

    if (CoordinatedGraphicsLayer* replica = downcast<CoordinatedGraphicsLayer>(replicaLayer()))
        replica->flushCompositingStateForThisLayerOnly();

    flushCompositingStateForThisLayerOnly();

    if (m_backdropLayer)
        m_backdropLayer->flushCompositingStateForThisLayerOnly();

    for (auto& child : children())
        child->flushCompositingState(rect);
}

void CoordinatedGraphicsLayer::setDebugBorder(const Color& color, float width)
{
    ASSERT(m_nicosia.debugBorder.visible);
    if (m_nicosia.debugBorder.color != color) {
        m_nicosia.debugBorder.color = color;
        m_nicosia.delta.debugBorderChanged = true;
    }

    if (m_nicosia.debugBorder.width != width) {
        m_nicosia.debugBorder.width = width;
        m_nicosia.delta.debugBorderChanged = true;
    }
}

bool CoordinatedGraphicsLayer::checkContentLayerUpdated()
{
    return std::exchange(m_contentsLayerUpdated, false);
}

void CoordinatedGraphicsLayer::clampToContentsRectIfRectIsInfinite(FloatRect& rect, const FloatSize& contentsSize)
{
    if (rect.width() >= LayoutUnit::nearlyMax() || rect.width() <= LayoutUnit::nearlyMin()) {
        rect.setX(0);
        rect.setWidth(contentsSize.width());
    }

    if (rect.height() >= LayoutUnit::nearlyMax() || rect.height() <= LayoutUnit::nearlyMin()) {
        rect.setY(0);
        rect.setHeight(contentsSize.height());
    }
}

void CoordinatedGraphicsLayer::flushCompositingStateForThisLayerOnly()
{
    // Whether it kicked or not, we don't need this timer running anymore.
    m_requestPendingTileCreationTimer.stop();

    // When we have a transform animation, we need to update visible rect every frame to adjust the visible rect of a backing store.
    bool hasActiveTransformAnimation = selfOrAncestorHasActiveTransformAnimation();
    if (hasActiveTransformAnimation)
        m_movingVisibleRect = true;

    // Sets the values.
    computePixelAlignment(m_adjustedPosition, m_adjustedSize, m_adjustedAnchorPoint, m_pixelAlignmentOffset);

    computeTransformedVisibleRect();

    if (m_contentsLayer && m_contentsLayerNeedsUpdate) {
        m_contentsLayerNeedsUpdate = false;
        m_contentsLayer->swapBuffersIfNeeded();
        m_contentsLayerUpdated = true;
    }

    // Only unset m_movingVisibleRect after we have updated the visible rect after the animation stopped.
    if (!hasActiveTransformAnimation)
        m_movingVisibleRect = false;

    // Determine the backing store presence. Content is painted later, in the updateContentBuffers() traversal.
    if (shouldHaveBackingStore()) {
        if (!m_backingStore) {
            m_backingStore = CoordinatedBackingStoreProxy::create(effectiveContentsScale());
            m_pendingVisibleRectAdjustment = true;
            m_nicosia.delta.backingStoreChanged = true;
        }
    } else if (m_backingStore) {
        m_backingStore = nullptr;
        m_nicosia.delta.backingStoreChanged = true;
    }

    if (hasActiveTransformAnimation && m_backingStore) {
        // The layer has a backingStore and a transformation animation. This means that we need to add an
        // AnimatedBackingStoreClient to check whether we need to update the backingStore due to the animation.
        // At this point we don't know the area covered by tiles available, so we just pass an empty rectangle
        // for that. The call to updateContentBuffers will calculate the tile coverage and set the appropriate
        // rectangle to the client.
        m_animatedBackingStoreClient = CoordinatedAnimatedBackingStoreClient::create(*this, m_coordinator->visibleContentsRect());
        m_nicosia.delta.animatedBackingStoreClientChanged = true;
    } else if (m_animatedBackingStoreClient) {
        m_animatedBackingStoreClient->invalidate();
        m_animatedBackingStoreClient = nullptr;
        m_nicosia.delta.animatedBackingStoreClientChanged = true;
    }

    if (m_pendingContentsImage)
        m_imageBacking.store = m_coordinator->imageBackingStore(m_pendingContentsImage.releaseNonNull());

    if (m_imageBacking.store) {
        bool wasVisible = m_imageBacking.isVisible;
        m_imageBacking.isVisible = transformedVisibleRect().intersects(IntRect(contentsRect()));
        if (wasVisible != m_imageBacking.isVisible)
            m_nicosia.delta.imageBackingChanged = true;
    }

    {
        m_nicosia.layer->accessPending(
            [this](Nicosia::CompositionLayer::LayerState& state)
            {
                // OR the local delta value into the layer's pending state delta. After that,
                // go through each local change and update the pending state accordingly.
                auto& localDelta = m_nicosia.delta;
                state.delta.value |= localDelta.value;

                if (localDelta.positionChanged)
                    state.position = m_adjustedPosition;
                if (localDelta.anchorPointChanged)
                    state.anchorPoint = m_adjustedAnchorPoint;
                if (localDelta.sizeChanged)
                    state.size = m_adjustedSize;
                if (localDelta.boundsOriginChanged)
                    state.boundsOrigin = boundsOrigin();

                if (localDelta.transformChanged)
                    state.transform = transform();
                if (localDelta.childrenTransformChanged)
                    state.childrenTransform = childrenTransform();

                if (localDelta.contentsRectChanged)
                    state.contentsRect = contentsRect();
                if (localDelta.contentsTilingChanged) {
                    state.contentsTilePhase = contentsTilePhase();
                    state.contentsTileSize = contentsTileSize();
                }
                if (localDelta.contentsClippingRectChanged)
                    state.contentsClippingRect = contentsClippingRect();

                if (localDelta.opacityChanged)
                    state.opacity = opacity();
                if (localDelta.solidColorChanged)
                    state.solidColor = m_solidColor;

                if (localDelta.filtersChanged)
                    state.filters = filters();

                bool madeBackdropLayer = false;
                if (localDelta.backdropFiltersChanged || needsBackdrop()) {
                    if (!needsBackdrop()) {
                        m_backdropLayer = nullptr;
                        state.backdropLayer = nullptr;
                    } else if (localDelta.backdropFiltersChanged) {
                        if (!m_backdropLayer) {
                            madeBackdropLayer = true;
                            m_backdropLayer = adoptRef(*new CoordinatedGraphicsLayer(Type::Normal, client()));
                            m_backdropLayer->setAnchorPoint(FloatPoint3D());
                            m_backdropLayer->setMasksToBounds(true);
                            m_backdropLayer->setName(MAKE_STATIC_STRING_IMPL("backdrop"));
                            if (m_coordinator)
                                m_coordinator->attachLayer(m_backdropLayer.get());
                        }
                        m_backdropLayer->setContentsVisible(m_contentsVisible);
                        m_backdropLayer->setFilters(m_backdropFilters);
                        state.backdropLayer = m_backdropLayer->m_nicosia.layer;
                    }
                }
                if ((localDelta.backdropFiltersRectChanged && m_backdropLayer) || madeBackdropLayer) {
                    m_backdropLayer->setSize(m_backdropFiltersRect.rect().size());
                    m_backdropLayer->setPosition(m_backdropFiltersRect.rect().location());
                }

                if (localDelta.backdropFiltersRectChanged)
                    state.backdropFiltersRect = m_backdropFiltersRect;

                if (localDelta.animationsChanged)
                    state.animations = m_animations;

                if (localDelta.childrenChanged) {
                    state.children = WTF::map(children(),
                        [](auto& child)
                        {
                            return downcast<CoordinatedGraphicsLayer>(child.get()).m_nicosia.layer;
                        });
                }

                if (localDelta.maskChanged) {
                    auto* mask = downcast<CoordinatedGraphicsLayer>(maskLayer());
                    state.mask = mask ? mask->m_nicosia.layer : nullptr;
                }

                if (localDelta.replicaChanged) {
                    auto* replica = downcast<CoordinatedGraphicsLayer>(replicaLayer());
                    state.replica = replica ? replica->m_nicosia.layer : nullptr;
                }

                if (localDelta.flagsChanged) {
                    state.flags.contentsOpaque = contentsOpaque();
                    state.flags.drawsContent = drawsContent();
                    state.flags.contentsVisible = contentsAreVisible();
                    state.flags.backfaceVisible = backfaceVisibility();
                    state.flags.masksToBounds = masksToBounds();
                    state.flags.contentsRectClipsDescendants = contentsRectClipsDescendants();
                    state.flags.preserves3D = preserves3D();
                }

                if (localDelta.repaintCounterChanged)
                    state.repaintCounter = m_nicosia.repaintCounter;
                if (localDelta.debugBorderChanged)
                    state.debugBorder = m_nicosia.debugBorder;

                if (localDelta.backingStoreChanged)
                    state.backingStore = m_backingStore;
                if (localDelta.contentLayerChanged)
                    state.contentLayer = m_contentsLayer;
                if (localDelta.imageBackingChanged) {
                    state.imageBacking.store = m_imageBacking.store;
                    state.imageBacking.isVisible = m_imageBacking.isVisible;
                }
                if (localDelta.animatedBackingStoreClientChanged)
                    state.animatedBackingStoreClient = m_animatedBackingStoreClient;
#if ENABLE(SCROLLING_THREAD)
                if (localDelta.scrollingNodeChanged)
                    state.scrollingNodeID = scrollingNodeID();
#endif
                if (localDelta.eventRegionChanged)
                    state.eventRegion = eventRegion();
                if (localDelta.damageChanged) {
                    state.damage = Damage();
                    if (m_needsDisplay.completeLayer)
                        state.damage.add(FloatRect({ }, m_size));
                    else {
                        for (const auto& rect : m_needsDisplay.rects)
                            state.damage.add(rect);
                    }
                    if (m_damagedRectsAreUnreliable)
                        state.damage.invalidate();
                }
            });
        m_nicosia.performLayerSync = !!m_nicosia.delta.value;
        m_nicosia.delta = { };
    }
}

bool CoordinatedGraphicsLayer::checkPendingStateChanges()
{
    return std::exchange(m_nicosia.performLayerSync, false);
}

void CoordinatedGraphicsLayer::deviceOrPageScaleFactorChanged()
{
    if (shouldHaveBackingStore())
        m_pendingContentsScaleAdjustment = true;
}

float CoordinatedGraphicsLayer::effectiveContentsScale() const
{
    return selfOrAncestorHaveNonAffineTransforms() ? 1 : deviceScaleFactor() * pageScaleFactor();
}

IntRect CoordinatedGraphicsLayer::transformedVisibleRect()
{
    // Non-invertible layers are not visible.
    if (!m_layerTransform.combined().isInvertible())
        return IntRect();

    // Return a projection of the visible rect (surface coordinates) onto the layer's plane (layer coordinates).
    // The resulting quad might be squewed and the visible rect is the bounding box of this quad,
    // so it might spread further than the real visible area (and then even more amplified by the cover rect multiplier).
    ASSERT(m_cachedInverseTransform == m_layerTransform.combined().inverse().value_or(TransformationMatrix()));
    FloatRect rect = m_cachedInverseTransform.clampedBoundsOfProjectedQuad(FloatQuad(m_coordinator->visibleContentsRect()));
    clampToContentsRectIfRectIsInfinite(rect, size());
    return enclosingIntRect(rect);
}

IntRect CoordinatedGraphicsLayer::transformedVisibleRectIncludingFuture()
{
    auto visibleRectIncludingFuture = transformedVisibleRect();
    if (m_cachedInverseTransform != m_cachedFutureInverseTransform) {
        FloatRect rect = m_cachedFutureInverseTransform.clampedBoundsOfProjectedQuad(FloatQuad(m_coordinator->visibleContentsRect()));
        clampToContentsRectIfRectIsInfinite(rect, size());
        visibleRectIncludingFuture.unite(enclosingIntRect(rect));
    }

    return visibleRectIncludingFuture;
}

void CoordinatedGraphicsLayer::requestBackingStoreUpdate()
{
    setNeedsVisibleRectAdjustment();
    notifyFlushRequired();
}

std::pair<bool, bool> CoordinatedGraphicsLayer::finalizeCompositingStateFlush()
{
    bool performLayerSync = false;
    bool contentLayerUpdated = false;

    if (auto* mask = downcast<CoordinatedGraphicsLayer>(maskLayer())) {
        mask->updateContentBuffers();
        performLayerSync |= mask->checkPendingStateChanges();
        contentLayerUpdated |= mask->checkContentLayerUpdated();
    }

    if (auto* replica = downcast<CoordinatedGraphicsLayer>(replicaLayer())) {
        replica->updateContentBuffers();
        performLayerSync |= replica->checkPendingStateChanges();
        contentLayerUpdated |= replica->checkContentLayerUpdated();
    }

    updateContentBuffers();
    performLayerSync |= checkPendingStateChanges();
    contentLayerUpdated |= checkContentLayerUpdated();

    for (auto& child : children()) {
        auto [childLayerSync, childContentUpdated] = downcast<CoordinatedGraphicsLayer>(child.get()).finalizeCompositingStateFlush();
        performLayerSync |= childLayerSync;
        contentLayerUpdated |= childContentUpdated;
    }

    return { performLayerSync, contentLayerUpdated };
}

void CoordinatedGraphicsLayer::updateContentBuffers()
{
    if (!m_backingStore)
        return;

#if PLATFORM(GTK) || PLATFORM(WPE)
    TraceScope traceScope(UpdateLayerContentBuffersStart, UpdateLayerContentBuffersEnd);
#endif

    // Address the content scale adjustment.
    if (m_pendingContentsScaleAdjustment) {
        if (m_backingStore->setContentsScale(effectiveContentsScale()))
            m_pendingVisibleRectAdjustment = true;
        m_pendingContentsScaleAdjustment = false;
    }

    // Bail if there's no painting recorded or enforced.
    if (!m_pendingVisibleRectAdjustment && !m_needsDisplay.completeLayer && m_needsDisplay.rects.isEmpty())
        return;

    IntRect contentsRect(IntPoint::zero(), IntSize(m_size));
    Vector<IntRect, 1> dirtyRegion;
    if (!m_needsDisplay.completeLayer) {
        dirtyRegion = m_needsDisplay.rects.map<Vector<IntRect, 1>>([](const FloatRect& rect) {
            return enclosingIntRect(rect);
        });
    } else
        dirtyRegion = { contentsRect };
    m_needsDisplay.completeLayer = false;
    m_needsDisplay.rects.clear();

    ASSERT(m_coordinator && m_coordinator->isFlushingLayerChanges());

    auto updateResult = m_backingStore->updateIfNeeded(transformedVisibleRectIncludingFuture(), contentsRect, m_pendingVisibleRectAdjustment, dirtyRegion, *this);
    m_pendingVisibleRectAdjustment = false;

    if (m_animatedBackingStoreClient)
        m_animatedBackingStoreClient->setCoverRect(m_backingStore->coverRect());

    if (updateResult.contains(CoordinatedBackingStoreProxy::UpdateResult::TilesChanged)) {
        m_nicosia.performLayerSync |= true;
        if (updateResult.contains(CoordinatedBackingStoreProxy::UpdateResult::BuffersChanged))
            didUpdateTileBuffers();
    }

    // Request a new update immediately if some tiles are still pending creation. Do this on a timer
    // as we're in a layer flush and flush requests at this point would be discarded.
    if (updateResult.contains(CoordinatedBackingStoreProxy::UpdateResult::TilesPending)) {
        setNeedsVisibleRectAdjustment();
        m_requestPendingTileCreationTimer.startOneShot(0_s);
    }
}

void CoordinatedGraphicsLayer::purgeBackingStores()
{
#ifndef NDEBUG
    SetForScope updateModeProtector(m_isPurging, true);
#endif

    m_backingStore = nullptr;

    if (m_animatedBackingStoreClient) {
        m_animatedBackingStoreClient->invalidate();
        m_animatedBackingStoreClient = nullptr;
    }

    m_imageBacking = { };

    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::invalidateCoordinator()
{
    m_coordinator = nullptr;
}

void CoordinatedGraphicsLayer::setCoordinatorIncludingSubLayersIfNeeded(CoordinatedGraphicsLayerClient* coordinator)
{
    if (!coordinator || m_coordinator == coordinator)
        return;

    // If the coordinators are different it means that we are attaching a layer that was created by a different
    // CompositingCoordinator than the current one. This happens because the layer was taken out of the tree
    // and then added back after AC was disabled and enabled again. We need to set the new coordinator to the
    // layer and its children.
    //
    // During each layer flush, the state stores the values that have changed since the previous one, and these
    // are updated once in the scene. When adding CoordinatedGraphicsLayers back to the tree, the fields that
    // are not updated during the next flush won't be sent to the scene, so they won't be updated there and the
    // rendering will fail.
    //
    // For example the drawsContent flag. This is set when the layer is created and is not updated anymore (unless
    // the content changes). When the layer is added back to the tree, the state won't reflect any change in the
    // flag value, so the scene won't update it and the layer won't be rendered.
    //
    // We need to update here the layer changeMask so the scene gets all the current values.
    m_nicosia.delta.value = UINT_MAX;

    m_coordinator = coordinator;
    m_coordinator->attachLayer(this);

    if (m_backdropLayer)
        m_coordinator->attachLayer(m_backdropLayer.get());

    for (auto& child : children())
        downcast<CoordinatedGraphicsLayer>(child.get()).setCoordinatorIncludingSubLayersIfNeeded(m_coordinator);
}

const RefPtr<Nicosia::CompositionLayer>& CoordinatedGraphicsLayer::compositionLayer() const
{
    return m_nicosia.layer;
}

void CoordinatedGraphicsLayer::setNeedsVisibleRectAdjustment()
{
    if (shouldHaveBackingStore())
        m_pendingVisibleRectAdjustment = true;
}

static inline bool isIntegral(float value)
{
    return static_cast<int>(value) == value;
}

FloatPoint CoordinatedGraphicsLayer::computePositionRelativeToBase()
{
    FloatPoint offset;
    for (const GraphicsLayer* currLayer = this; currLayer; currLayer = currLayer->parent())
        offset += (currLayer->position() - currLayer->boundsOrigin());

    return offset;
}

void CoordinatedGraphicsLayer::computePixelAlignment(FloatPoint& position, FloatSize& size, FloatPoint3D& anchorPoint, FloatSize& alignmentOffset)
{
    if (isIntegral(effectiveContentsScale())) {
        position = m_position;
        size = m_size;
        anchorPoint = m_anchorPoint;
        alignmentOffset = FloatSize();
        return;
    }

    FloatPoint positionRelativeToBase = computePositionRelativeToBase();

    FloatRect baseRelativeBounds(positionRelativeToBase, m_size);
    FloatRect scaledBounds = baseRelativeBounds;

    // Scale by the effective scale factor to compute the screen-relative bounds.
    scaledBounds.scale(effectiveContentsScale());

    // Round to integer boundaries.
    // NOTE: When using enclosingIntRect (as mac) it will have different sizes depending on position.
    FloatRect alignedBounds = enclosingIntRect(scaledBounds);

    // Convert back to layer coordinates.
    alignedBounds.scale(1 / effectiveContentsScale());

    // Convert back to layer coordinates.
    alignmentOffset = baseRelativeBounds.location() - alignedBounds.location();

    position = m_position - alignmentOffset;
    size = alignedBounds.size();

    // Now we have to compute a new anchor point which compensates for rounding.
    float anchorPointX = m_anchorPoint.x();
    float anchorPointY = m_anchorPoint.y();

    if (alignedBounds.width())
        anchorPointX = (baseRelativeBounds.width() * anchorPointX + alignmentOffset.width()) / alignedBounds.width();

    if (alignedBounds.height())
        anchorPointY = (baseRelativeBounds.height() * anchorPointY + alignmentOffset.height()) / alignedBounds.height();

    anchorPoint = FloatPoint3D(anchorPointX, anchorPointY, m_anchorPoint.z() * effectiveContentsScale());
}

void CoordinatedGraphicsLayer::computeTransformedVisibleRect()
{
    if (!m_shouldUpdateVisibleRect && !m_movingVisibleRect)
        return;

    m_shouldUpdateVisibleRect = false;
    TransformationMatrix currentTransform = transform();
    TransformationMatrix futureTransform = currentTransform;
    if (m_movingVisibleRect) {
        client().getCurrentTransform(this, currentTransform);
        TextureMapperAnimation::ApplicationResult futureApplicationResults;
        m_animations.apply(futureApplicationResults, MonotonicTime::now() + 50_ms, TextureMapperAnimation::KeepInternalState::Yes);
        futureTransform = futureApplicationResults.transform.value_or(currentTransform);
    }
    m_layerTransform.setLocalTransform(currentTransform);

    m_layerTransform.setAnchorPoint(m_adjustedAnchorPoint);
    m_layerTransform.setPosition(FloatPoint(m_adjustedPosition.x() - boundsOrigin().x(), m_adjustedPosition.y() - boundsOrigin().y()));
    m_layerTransform.setSize(m_adjustedSize);

    m_layerTransform.setFlattening(!preserves3D());
    m_layerTransform.setChildrenTransform(childrenTransform());
    m_layerTransform.combineTransforms(parent() ? downcast<CoordinatedGraphicsLayer>(*parent()).m_layerTransform.combinedForChildren() : TransformationMatrix());

    m_cachedCombinedTransform = m_layerTransform.combined();
    m_cachedInverseTransform = m_cachedCombinedTransform.inverse().value_or(TransformationMatrix());

    m_layerFutureTransform = m_layerTransform;
    m_cachedFutureInverseTransform = m_cachedInverseTransform;

    CoordinatedGraphicsLayer* parentLayer = downcast<CoordinatedGraphicsLayer>(parent());

    if (currentTransform != futureTransform || (parentLayer && parentLayer->m_layerTransform.combinedForChildren() != parentLayer->m_layerFutureTransform.combinedForChildren())) {
        m_layerFutureTransform.setLocalTransform(futureTransform);
        m_layerFutureTransform.combineTransforms(parentLayer ? parentLayer->m_layerFutureTransform.combinedForChildren() : TransformationMatrix());
        m_cachedFutureInverseTransform = m_layerFutureTransform.combined().inverse().value_or(TransformationMatrix());
    }

    // The combined transform will be used in tiledBackingStoreVisibleRect.
    setNeedsVisibleRectAdjustment();
}

bool CoordinatedGraphicsLayer::shouldHaveBackingStore() const
{
    // If the CSS opacity value is 0 and there's no animation over the opacity property, the layer is invisible.
    bool isInvisibleBecauseOpacityZero = !opacity() && !m_animations.hasActiveAnimationsOfType(AnimatedProperty::Opacity);

    // Check if there's a filter that sets the opacity to zero.
    bool hasOpacityZeroFilter = std::ranges::any_of(filters(), [](auto& operation) {
        return operation->type() == FilterOperation::Type::Opacity && !downcast<BasicComponentTransferFilterOperation>(operation.get()).amount();
    });

    // If there's a filter that sets opacity to 0 and the filters are not being animated, the layer is invisible.
    isInvisibleBecauseOpacityZero |= hasOpacityZeroFilter && !m_animations.hasActiveAnimationsOfType(AnimatedProperty::Filter);

    return drawsContent() && contentsAreVisible() && !m_size.isEmpty() && !isInvisibleBecauseOpacityZero;
}

bool CoordinatedGraphicsLayer::selfOrAncestorHasActiveTransformAnimation() const
{
    if (m_animations.hasActiveAnimationsOfType(AnimatedProperty::Transform))
        return true;

    if (!parent())
        return false;

    return downcast<CoordinatedGraphicsLayer>(*parent()).selfOrAncestorHasActiveTransformAnimation();
}

bool CoordinatedGraphicsLayer::selfOrAncestorHaveNonAffineTransforms() const
{
    if (!m_layerTransform.combined().isAffine())
        return true;

    if (!parent())
        return false;

    return downcast<CoordinatedGraphicsLayer>(*parent()).selfOrAncestorHaveNonAffineTransforms();
}

bool CoordinatedGraphicsLayer::addAnimation(const KeyframeValueList& valueList, const FloatSize& boxSize, const Animation* anim, const String& keyframesName, double delayAsNegativeTimeOffset)
{
    ASSERT(!keyframesName.isEmpty());

    if (!anim || anim->isEmptyOrZeroDuration() || valueList.size() < 2)
        return false;

    switch (valueList.property()) {
    case AnimatedProperty::WebkitBackdropFilter:
    case AnimatedProperty::Filter: {
        int listIndex = validateFilterOperations(valueList);
        if (listIndex < 0)
            return false;

        const auto& filters = static_cast<const FilterAnimationValue&>(valueList.at(listIndex)).value();
        if (!filtersCanBeComposited(filters))
            return false;
        break;
    }
    case AnimatedProperty::Opacity:
    case AnimatedProperty::Translate:
    case AnimatedProperty::Rotate:
    case AnimatedProperty::Scale:
    case AnimatedProperty::Transform:
        break;
    default:
        return false;
    }

    m_lastAnimationStartTime = MonotonicTime::now() - Seconds(delayAsNegativeTimeOffset);
    m_animations.add(TextureMapperAnimation(keyframesName, valueList, boxSize, *anim, m_lastAnimationStartTime, 0_s, TextureMapperAnimation::State::Playing));
    m_animationStartedTimer.startOneShot(0_s);
    didChangeAnimations();
    return true;
}

void CoordinatedGraphicsLayer::pauseAnimation(const String& animationName, double time)
{
    m_animations.pause(animationName, Seconds(time));
    didChangeAnimations();
}

void CoordinatedGraphicsLayer::removeAnimation(const String& animationName, std::optional<AnimatedProperty>)
{
    m_animations.remove(animationName);
    didChangeAnimations();
}

void CoordinatedGraphicsLayer::suspendAnimations(MonotonicTime time)
{
    m_animations.suspend(time);
    didChangeAnimations();
}

void CoordinatedGraphicsLayer::resumeAnimations()
{
    m_animations.resume();
    didChangeAnimations();
}

void CoordinatedGraphicsLayer::transformRelatedPropertyDidChange()
{
    if (m_animations.hasRunningTransformAnimations())
        didChangeAnimations();
}

void CoordinatedGraphicsLayer::animationStartedTimerFired()
{
    client().notifyAnimationStarted(this, emptyString(), m_lastAnimationStartTime);
}

void CoordinatedGraphicsLayer::requestPendingTileCreationTimerFired()
{
    notifyFlushRequired();
}

bool CoordinatedGraphicsLayer::usesContentsLayer() const
{
    return m_contentsLayer || m_pendingContentsImage || m_imageBacking.store;
}

static void dumpInnerLayer(TextStream& textStream, const String& label, CoordinatedGraphicsLayer* layer, OptionSet<LayerTreeAsTextOptions> options)
{
    if (!layer)
        return;

    textStream << indent << "(" << label << " ";
    if (options & LayerTreeAsTextOptions::Debug)
        textStream << " " << static_cast<void*>(layer);
    textStream << layer->boundsOrigin().x() << ", " << layer->boundsOrigin().y() << " " << layer->size().width() << " x " << layer->size().height();
    if (!layer->contentsAreVisible())
        textStream << " hidden";
    textStream << ")\n";
}

void CoordinatedGraphicsLayer::dumpAdditionalProperties(TextStream& textStream, OptionSet<LayerTreeAsTextOptions> options) const
{
    if (options & LayerTreeAsTextOptions::IncludeContentLayers)
        dumpInnerLayer(textStream, "backdrop layer"_s, m_backdropLayer.get(), options);
}

double CoordinatedGraphicsLayer::backingStoreMemoryEstimate() const
{
    return 0.0;
}

Vector<std::pair<String, double>> CoordinatedGraphicsLayer::acceleratedAnimationsForTesting(const Settings&) const
{
    Vector<std::pair<String, double>> animations;

    for (auto& animation : m_animations.animations())
        animations.append({ animatedPropertyIDAsString(animation.keyframes().property()), animation.state() == TextureMapperAnimation::State::Playing ? 1 : 0 });

    return animations;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
