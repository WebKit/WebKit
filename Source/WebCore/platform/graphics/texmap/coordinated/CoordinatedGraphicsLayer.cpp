/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "FloatQuad.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerFactory.h"
#include "ScrollableArea.h"
#include "TextureMapperPlatformLayerProxyProvider.h"
#include <wtf/CurrentTime.h>
#ifndef NDEBUG
#include <wtf/SetForScope.h>
#endif
#include <wtf/text/CString.h>

namespace WebCore {

std::unique_ptr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerFactory* factory, GraphicsLayerClient& client, Type layerType)
{
    if (!factory)
        return std::make_unique<CoordinatedGraphicsLayer>(layerType, client);

    return factory->createGraphicsLayer(layerType, client);
}

static CoordinatedLayerID toCoordinatedLayerID(GraphicsLayer* layer)
{
    return is<CoordinatedGraphicsLayer>(layer) ? downcast<CoordinatedGraphicsLayer>(*layer).id() : 0;
}

void CoordinatedGraphicsLayer::notifyFlushRequired()
{
    if (!m_coordinator)
        return;

    if (m_coordinator->isFlushingLayerChanges())
        return;

    client().notifyFlushRequired(this);
}

void CoordinatedGraphicsLayer::didChangeLayerState()
{
    m_shouldSyncLayerState = true;
    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::didChangeAnimations()
{
    m_shouldSyncAnimations = true;
    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::didChangeChildren()
{
    m_shouldSyncChildren = true;
    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::didChangeFilters()
{
    m_shouldSyncFilters = true;
    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::didChangeImageBacking()
{
    m_shouldSyncImageBacking = true;
    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::setShouldUpdateVisibleRect()
{
    m_shouldUpdateVisibleRect = true;
    for (auto& child : children())
        downcast<CoordinatedGraphicsLayer>(*child).setShouldUpdateVisibleRect();
    if (replicaLayer())
        downcast<CoordinatedGraphicsLayer>(*replicaLayer()).setShouldUpdateVisibleRect();
}

void CoordinatedGraphicsLayer::didChangeGeometry()
{
    didChangeLayerState();
    setShouldUpdateVisibleRect();
}

CoordinatedGraphicsLayer::CoordinatedGraphicsLayer(Type layerType, GraphicsLayerClient& client)
    : GraphicsLayer(layerType, client)
#ifndef NDEBUG
    , m_isPurging(false)
#endif
    , m_shouldUpdateVisibleRect(true)
    , m_shouldSyncLayerState(true)
    , m_shouldSyncChildren(true)
    , m_shouldSyncFilters(true)
    , m_shouldSyncImageBacking(true)
    , m_shouldSyncAnimations(true)
    , m_fixedToViewport(false)
    , m_movingVisibleRect(false)
    , m_pendingContentsScaleAdjustment(false)
    , m_pendingVisibleRectAdjustment(false)
#if USE(COORDINATED_GRAPHICS_THREADED)
    , m_shouldSyncPlatformLayer(false)
    , m_shouldUpdatePlatformLayer(false)
#endif
    , m_coordinator(0)
    , m_compositedNativeImagePtr(0)
    , m_platformLayer(0)
    , m_animationStartedTimer(*this, &CoordinatedGraphicsLayer::animationStartedTimerFired)
    , m_scrollableArea(0)
{
    static CoordinatedLayerID nextLayerID = 1;
    m_id = nextLayerID++;
}

CoordinatedGraphicsLayer::~CoordinatedGraphicsLayer()
{
    if (m_coordinator) {
        purgeBackingStores();
        m_coordinator->detachLayer(this);
    }
    ASSERT(!m_coordinatedImageBacking);
    ASSERT(!m_mainBackingStore);
    willBeDestroyed();
}

bool CoordinatedGraphicsLayer::setChildren(const Vector<GraphicsLayer*>& children)
{
    bool ok = GraphicsLayer::setChildren(children);
    if (!ok)
        return false;
    didChangeChildren();
    return true;
}

void CoordinatedGraphicsLayer::addChild(GraphicsLayer* layer)
{
    GraphicsLayer::addChild(layer);
    didChangeChildren();
}

void CoordinatedGraphicsLayer::addChildAtIndex(GraphicsLayer* layer, int index)
{
    GraphicsLayer::addChildAtIndex(layer, index);
    didChangeChildren();
}

void CoordinatedGraphicsLayer::addChildAbove(GraphicsLayer* layer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildAbove(layer, sibling);
    didChangeChildren();
}

void CoordinatedGraphicsLayer::addChildBelow(GraphicsLayer* layer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(layer, sibling);
    didChangeChildren();
}

bool CoordinatedGraphicsLayer::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    bool ok = GraphicsLayer::replaceChild(oldChild, newChild);
    if (!ok)
        return false;
    didChangeChildren();
    return true;
}

void CoordinatedGraphicsLayer::removeFromParent()
{
    if (CoordinatedGraphicsLayer* parentLayer = downcast<CoordinatedGraphicsLayer>(parent()))
        parentLayer->didChangeChildren();
    GraphicsLayer::removeFromParent();
}

void CoordinatedGraphicsLayer::setPosition(const FloatPoint& p)
{
    if (position() == p)
        return;

    GraphicsLayer::setPosition(p);
    m_layerState.positionChanged = true;
    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setAnchorPoint(const FloatPoint3D& p)
{
    if (anchorPoint() == p)
        return;

    GraphicsLayer::setAnchorPoint(p);
    m_layerState.anchorPointChanged = true;
    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setSize(const FloatSize& size)
{
    if (this->size() == size)
        return;

    GraphicsLayer::setSize(size);
    m_layerState.sizeChanged = true;

    if (maskLayer())
        maskLayer()->setSize(size);
    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setTransform(const TransformationMatrix& t)
{
    if (transform() == t)
        return;

    GraphicsLayer::setTransform(t);
    m_layerState.transformChanged = true;

    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setChildrenTransform(const TransformationMatrix& t)
{
    if (childrenTransform() == t)
        return;

    GraphicsLayer::setChildrenTransform(t);
    m_layerState.childrenTransformChanged = true;

    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setPreserves3D(bool b)
{
    if (preserves3D() == b)
        return;

    GraphicsLayer::setPreserves3D(b);
    m_layerState.preserves3D = b;
    m_layerState.flagsChanged = true;

    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setMasksToBounds(bool b)
{
    if (masksToBounds() == b)
        return;
    GraphicsLayer::setMasksToBounds(b);
    m_layerState.masksToBounds = b;
    m_layerState.flagsChanged = true;

    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setDrawsContent(bool b)
{
    if (drawsContent() == b)
        return;
    GraphicsLayer::setDrawsContent(b);
    m_layerState.drawsContent = b;
    m_layerState.flagsChanged = true;

    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setContentsVisible(bool b)
{
    if (contentsAreVisible() == b)
        return;
    GraphicsLayer::setContentsVisible(b);
    m_layerState.contentsVisible = b;
    m_layerState.flagsChanged = true;

    if (maskLayer())
        maskLayer()->setContentsVisible(b);

    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setContentsOpaque(bool b)
{
    if (contentsOpaque() == b)
        return;
    if (m_mainBackingStore)
        m_mainBackingStore->setSupportsAlpha(!b);
    GraphicsLayer::setContentsOpaque(b);
    m_layerState.contentsOpaque = b;
    m_layerState.flagsChanged = true;

    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setBackfaceVisibility(bool b)
{
    if (backfaceVisibility() == b)
        return;

    GraphicsLayer::setBackfaceVisibility(b);
    m_layerState.backfaceVisible = b;
    m_layerState.flagsChanged = true;

    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setOpacity(float opacity)
{
    if (this->opacity() == opacity)
        return;

    GraphicsLayer::setOpacity(opacity);
    m_layerState.opacity = opacity;
    m_layerState.opacityChanged = true;

    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setContentsRect(const FloatRect& r)
{
    if (contentsRect() == r)
        return;

    GraphicsLayer::setContentsRect(r);
    m_layerState.contentsRect = r;
    m_layerState.contentsRectChanged = true;

    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setContentsTileSize(const FloatSize& s)
{
    if (contentsTileSize() == s)
        return;

    GraphicsLayer::setContentsTileSize(s);
    m_layerState.contentsTileSize = s;
    m_layerState.contentsTilingChanged = true;
    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setContentsTilePhase(const FloatSize& p)
{
    if (contentsTilePhase() == p)
        return;

    GraphicsLayer::setContentsTilePhase(p);
    m_layerState.contentsTilePhase = p;
    m_layerState.contentsTilingChanged = true;
    didChangeLayerState();
}

bool GraphicsLayer::supportsContentsTiling()
{
    return true;
}

void CoordinatedGraphicsLayer::setContentsNeedsDisplay()
{
#if USE(COORDINATED_GRAPHICS_THREADED)
    if (m_platformLayer)
        m_shouldUpdatePlatformLayer = true;
#endif

    notifyFlushRequired();
    addRepaintRect(contentsRect());
}

void CoordinatedGraphicsLayer::setContentsToPlatformLayer(PlatformLayer* platformLayer, ContentsLayerPurpose)
{
#if USE(COORDINATED_GRAPHICS_THREADED)
    if (m_platformLayer != platformLayer)
        m_shouldSyncPlatformLayer = true;

    m_platformLayer = platformLayer;
    notifyFlushRequired();
#else
    UNUSED_PARAM(platformLayer);
#endif
}

bool CoordinatedGraphicsLayer::filtersCanBeComposited(const FilterOperations& filters) const
{
    if (!filters.size())
        return false;

    for (const auto& filterOperation : filters.operations()) {
        if (filterOperation->type() == FilterOperation::REFERENCE)
            return false;
    }

    return true;
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

void CoordinatedGraphicsLayer::setContentsToSolidColor(const Color& color)
{
    if (m_layerState.solidColor == color)
        return;

    m_layerState.solidColor = color;
    m_layerState.solidColorChanged = true;

    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setShowDebugBorder(bool show)
{
    if (isShowingDebugBorder() == show)
        return;

    GraphicsLayer::setShowDebugBorder(show);
    m_layerState.debugVisuals.showDebugBorders = show;
    m_layerState.debugVisualsChanged = true;

    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setShowRepaintCounter(bool show)
{
    if (isShowingRepaintCounter() == show)
        return;

    GraphicsLayer::setShowRepaintCounter(show);
    m_layerState.debugVisuals.showRepaintCounter = show;
    m_layerState.debugVisualsChanged = true;

    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setContentsToImage(Image* image)
{
    NativeImagePtr nativeImagePtr = image ? image->nativeImageForCurrentFrame() : nullptr;
    if (m_compositedImage == image && m_compositedNativeImagePtr == nativeImagePtr)
        return;

    m_compositedImage = image;
    m_compositedNativeImagePtr = nativeImagePtr;

    GraphicsLayer::setContentsToImage(image);
    didChangeImageBacking();
}

void CoordinatedGraphicsLayer::setMaskLayer(GraphicsLayer* layer)
{
    if (layer == maskLayer())
        return;

    GraphicsLayer::setMaskLayer(layer);

    if (!layer)
        return;

    layer->setSize(size());
    layer->setContentsVisible(contentsAreVisible());
    auto& coordinatedLayer = downcast<CoordinatedGraphicsLayer>(*layer);
    coordinatedLayer.didChangeLayerState();

    m_layerState.mask = coordinatedLayer.id();
    m_layerState.maskChanged = true;

    didChangeLayerState();
}

bool CoordinatedGraphicsLayer::shouldDirectlyCompositeImage(Image* image) const
{
    if (!image || !image->isBitmapImage())
        return false;

    enum { MaxDimenstionForDirectCompositing = 2000 };
    if (image->width() > MaxDimenstionForDirectCompositing || image->height() > MaxDimenstionForDirectCompositing)
        return false;

    return true;
}

void CoordinatedGraphicsLayer::setReplicatedByLayer(GraphicsLayer* layer)
{
    if (layer == replicaLayer())
        return;

    GraphicsLayer::setReplicatedByLayer(layer);
    m_layerState.replica = toCoordinatedLayerID(layer);
    m_layerState.replicaChanged = true;
    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setNeedsDisplay()
{
    setNeedsDisplayInRect(FloatRect(FloatPoint(), size()));
}

void CoordinatedGraphicsLayer::setNeedsDisplayInRect(const FloatRect& rect, ShouldClipToLayer)
{
    if (m_mainBackingStore)
        m_mainBackingStore->invalidate(IntRect(rect));

    didChangeLayerState();

    addRepaintRect(rect);
}

void CoordinatedGraphicsLayer::setScrollableArea(ScrollableArea* scrollableArea)
{
    bool oldScrollable = isScrollable();
    m_scrollableArea = scrollableArea;
    if (oldScrollable == isScrollable())
        return;

    m_layerState.isScrollable = isScrollable();
    m_layerState.flagsChanged = true;
    didChangeLayerState();
}

void CoordinatedGraphicsLayer::commitScrollOffset(const IntSize& offset)
{
    if (!isScrollable() || offset.isZero())
        return;

    m_scrollableArea->notifyScrollPositionChanged(m_scrollableArea->scrollPosition() + offset);
    m_layerState.committedScrollOffset += offset;
    m_layerState.committedScrollOffsetChanged = true;
    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setFixedToViewport(bool isFixed)
{
    if (m_fixedToViewport == isFixed)
        return;

    m_fixedToViewport = isFixed;
    m_layerState.fixedToViewport = isFixed;
    m_layerState.flagsChanged = true;

    didChangeLayerState();
}

void CoordinatedGraphicsLayer::flushCompositingState(const FloatRect& rect)
{
    if (CoordinatedGraphicsLayer* mask = downcast<CoordinatedGraphicsLayer>(maskLayer()))
        mask->flushCompositingStateForThisLayerOnly();

    if (CoordinatedGraphicsLayer* replica = downcast<CoordinatedGraphicsLayer>(replicaLayer()))
        replica->flushCompositingStateForThisLayerOnly();

    flushCompositingStateForThisLayerOnly();

    for (auto& child : children())
        child->flushCompositingState(rect);
}

void CoordinatedGraphicsLayer::syncChildren()
{
    if (!m_shouldSyncChildren)
        return;
    m_shouldSyncChildren = false;
    m_layerState.childrenChanged = true;
    m_layerState.children.clear();
    for (auto& child : children())
        m_layerState.children.append(toCoordinatedLayerID(child));
}

void CoordinatedGraphicsLayer::syncFilters()
{
    if (!m_shouldSyncFilters)
        return;
    m_shouldSyncFilters = false;

    m_layerState.filters = GraphicsLayer::filters();
    m_layerState.filtersChanged = true;
}

void CoordinatedGraphicsLayer::syncImageBacking()
{
    if (!m_shouldSyncImageBacking)
        return;
    m_shouldSyncImageBacking = false;

    if (m_compositedNativeImagePtr) {
        ASSERT(!shouldHaveBackingStore());
        ASSERT(m_compositedImage);

        bool imageInstanceReplaced = m_coordinatedImageBacking && (m_coordinatedImageBacking->id() != CoordinatedImageBacking::getCoordinatedImageBackingID(m_compositedImage.get()));
        if (imageInstanceReplaced)
            releaseImageBackingIfNeeded();

        if (!m_coordinatedImageBacking) {
            m_coordinatedImageBacking = m_coordinator->createImageBackingIfNeeded(*m_compositedImage);
            m_coordinatedImageBacking->addHost(this);
            m_layerState.imageID = m_coordinatedImageBacking->id();
        }

        m_coordinatedImageBacking->markDirty();
        m_layerState.imageChanged = true;
    } else
        releaseImageBackingIfNeeded();
}

void CoordinatedGraphicsLayer::syncLayerState()
{
    if (!m_shouldSyncLayerState)
        return;
    m_shouldSyncLayerState = false;

    m_layerState.childrenTransform = childrenTransform();
    m_layerState.contentsRect = contentsRect();
    m_layerState.mask = toCoordinatedLayerID(maskLayer());
    m_layerState.opacity = opacity();
    m_layerState.replica = toCoordinatedLayerID(replicaLayer());
    m_layerState.transform = transform();

    m_layerState.anchorPoint = m_adjustedAnchorPoint;
    m_layerState.pos = m_adjustedPosition;
    m_layerState.size = m_adjustedSize;

    if (m_layerState.flagsChanged) {
        m_layerState.contentsOpaque = contentsOpaque();
        m_layerState.drawsContent = drawsContent();
        m_layerState.contentsVisible = contentsAreVisible();
        m_layerState.backfaceVisible = backfaceVisibility();
        m_layerState.masksToBounds = masksToBounds();
        m_layerState.preserves3D = preserves3D();
        m_layerState.fixedToViewport = fixedToViewport();
        m_layerState.isScrollable = isScrollable();
    }

    if (m_layerState.debugVisualsChanged) {
        m_layerState.debugVisuals.showDebugBorders = isShowingDebugBorder();
        m_layerState.debugVisuals.showRepaintCounter = isShowingRepaintCounter();
    }

    if (m_layerState.debugVisuals.showDebugBorders)
        updateDebugIndicators();
}

void CoordinatedGraphicsLayer::setDebugBorder(const Color& color, float width)
{
    ASSERT(m_layerState.debugVisuals.showDebugBorders);
    if (m_layerState.debugVisuals.debugBorderColor != color) {
        m_layerState.debugVisuals.debugBorderColor = color;
        m_layerState.debugVisualsChanged = true;
    }

    if (m_layerState.debugVisuals.debugBorderWidth != width) {
        m_layerState.debugVisuals.debugBorderWidth = width;
        m_layerState.debugVisualsChanged = true;
    }
}

void CoordinatedGraphicsLayer::syncAnimations()
{
    if (!m_shouldSyncAnimations)
        return;

    m_shouldSyncAnimations = false;
    m_layerState.animations = m_animations.getActiveAnimations();
    m_layerState.animationsChanged = true;
}

void CoordinatedGraphicsLayer::syncPlatformLayer()
{
#if USE(COORDINATED_GRAPHICS_THREADED)
    if (!m_shouldSyncPlatformLayer)
        return;

    m_shouldSyncPlatformLayer = false;
    m_layerState.platformLayerChanged = true;
    if (m_platformLayer)
        m_layerState.platformLayerProxy = m_platformLayer->proxy();
#endif
}

void CoordinatedGraphicsLayer::updatePlatformLayer()
{
#if USE(COORDINATED_GRAPHICS_THREADED)
    if (!m_shouldUpdatePlatformLayer)
        return;

    m_shouldUpdatePlatformLayer = false;
    m_layerState.platformLayerUpdated = true;
    if (m_platformLayer)
        m_platformLayer->swapBuffersIfNeeded();
#endif
}

void CoordinatedGraphicsLayer::flushCompositingStateForThisLayerOnly()
{
    // When we have a transform animation, we need to update visible rect every frame to adjust the visible rect of a backing store.
    bool hasActiveTransformAnimation = selfOrAncestorHasActiveTransformAnimation();
    if (hasActiveTransformAnimation)
        m_movingVisibleRect = true;

    // Sets the values.
    computePixelAlignment(m_adjustedPosition, m_adjustedSize, m_adjustedAnchorPoint, m_pixelAlignmentOffset);

    syncImageBacking();
    syncLayerState();
    syncAnimations();
    computeTransformedVisibleRect();
    syncChildren();
    syncFilters();
    syncPlatformLayer();
    updatePlatformLayer();

    // Only unset m_movingVisibleRect after we have updated the visible rect after the animation stopped.
    if (!hasActiveTransformAnimation)
        m_movingVisibleRect = false;
}

void CoordinatedGraphicsLayer::syncPendingStateChangesIncludingSubLayers()
{
    if (m_layerState.hasPendingChanges()) {
        m_coordinator->syncLayerState(m_id, m_layerState);
        resetLayerState();
    }

    if (maskLayer())
        downcast<CoordinatedGraphicsLayer>(*maskLayer()).syncPendingStateChangesIncludingSubLayers();

    for (auto& child : children())
        downcast<CoordinatedGraphicsLayer>(*child).syncPendingStateChangesIncludingSubLayers();
}

void CoordinatedGraphicsLayer::resetLayerState()
{
    m_layerState.changeMask = 0;
    m_layerState.tilesToCreate.clear();
    m_layerState.tilesToRemove.clear();
    m_layerState.tilesToUpdate.clear();
    m_layerState.committedScrollOffset = IntSize();
}

bool CoordinatedGraphicsLayer::imageBackingVisible()
{
    ASSERT(m_coordinatedImageBacking);
    return transformedVisibleRect().intersects(IntRect(contentsRect()));
}

void CoordinatedGraphicsLayer::releaseImageBackingIfNeeded()
{
    if (!m_coordinatedImageBacking)
        return;

    ASSERT(m_coordinator);
    m_coordinatedImageBacking->removeHost(this);
    m_coordinatedImageBacking = nullptr;
    m_layerState.imageID = InvalidCoordinatedImageBackingID;
    m_layerState.imageChanged = true;
}

CoordinatedGraphicsLayer* CoordinatedGraphicsLayer::findFirstDescendantWithContentsRecursively()
{
    if (shouldHaveBackingStore())
        return this;

    for (auto& child : children()) {
        if (CoordinatedGraphicsLayer* layer = downcast<CoordinatedGraphicsLayer>(*child).findFirstDescendantWithContentsRecursively())
            return layer;
    }

    return nullptr;
}

void CoordinatedGraphicsLayer::setVisibleContentRectTrajectoryVector(const FloatPoint& trajectoryVector)
{
    if (!m_mainBackingStore)
        return;

    m_mainBackingStore->setTrajectoryVector(trajectoryVector);
    setNeedsVisibleRectAdjustment();
}

void CoordinatedGraphicsLayer::deviceOrPageScaleFactorChanged()
{
    if (shouldHaveBackingStore())
        m_pendingContentsScaleAdjustment = true;
}

float CoordinatedGraphicsLayer::effectiveContentsScale()
{
    return selfOrAncestorHaveNonAffineTransforms() ? 1 : deviceScaleFactor() * pageScaleFactor();
}

void CoordinatedGraphicsLayer::adjustContentsScale()
{
    ASSERT(shouldHaveBackingStore());
    if (!m_mainBackingStore || m_mainBackingStore->contentsScale() == effectiveContentsScale())
        return;

    // Between creating the new backing store and painting the content,
    // we do not want to drop the previous one as that might result in
    // briefly seeing flickering as the old tiles may be dropped before
    // something replaces them.
    m_previousBackingStore = WTFMove(m_mainBackingStore);

    // No reason to save the previous backing store for non-visible areas.
    m_previousBackingStore->removeAllNonVisibleTiles(transformedVisibleRect(), IntRect(0, 0, size().width(), size().height()));
}

void CoordinatedGraphicsLayer::createBackingStore()
{
    m_mainBackingStore = std::make_unique<TiledBackingStore>(this, effectiveContentsScale());
    m_mainBackingStore->setSupportsAlpha(!contentsOpaque());
}

void CoordinatedGraphicsLayer::tiledBackingStorePaint(GraphicsContext& context, const IntRect& rect)
{
    if (rect.isEmpty())
        return;
    paintGraphicsLayerContents(context, rect);
}

void CoordinatedGraphicsLayer::didUpdateTileBuffers()
{
    if (!isShowingRepaintCounter())
        return;

    m_layerState.repaintCount = incrementRepaintCount();
    m_layerState.repaintCountChanged = true;
}

void CoordinatedGraphicsLayer::tiledBackingStoreHasPendingTileCreation()
{
    setNeedsVisibleRectAdjustment();
    notifyFlushRequired();
}

static void clampToContentsRectIfRectIsInfinite(FloatRect& rect, const FloatSize& contentsSize)
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

bool CoordinatedGraphicsLayer::paintToSurface(const IntSize& size, uint32_t& atlas, IntPoint& offset, CoordinatedSurface::Client& client)
{
    ASSERT(m_coordinator);
    ASSERT(m_coordinator->isFlushingLayerChanges());
    return m_coordinator->paintToSurface(size, contentsOpaque() ? CoordinatedSurface::NoFlags : CoordinatedSurface::SupportsAlpha, atlas, offset, client);
}

void CoordinatedGraphicsLayer::createTile(uint32_t tileID, float scaleFactor)
{
    ASSERT(m_coordinator);
    ASSERT(m_coordinator->isFlushingLayerChanges());

    TileCreationInfo creationInfo;
    creationInfo.tileID = tileID;
    creationInfo.scale = scaleFactor;
    m_layerState.tilesToCreate.append(creationInfo);
}

void CoordinatedGraphicsLayer::updateTile(uint32_t tileID, const SurfaceUpdateInfo& updateInfo, const IntRect& tileRect)
{
    ASSERT(m_coordinator);
    ASSERT(m_coordinator->isFlushingLayerChanges());

    TileUpdateInfo tileUpdateInfo;
    tileUpdateInfo.tileID = tileID;
    tileUpdateInfo.tileRect = tileRect;
    tileUpdateInfo.updateInfo = updateInfo;
    m_layerState.tilesToUpdate.append(tileUpdateInfo);
}

void CoordinatedGraphicsLayer::removeTile(uint32_t tileID)
{
    ASSERT(m_coordinator);
    ASSERT(m_coordinator->isFlushingLayerChanges() || m_isPurging);
    m_layerState.tilesToRemove.append(tileID);
}

void CoordinatedGraphicsLayer::updateContentBuffersIncludingSubLayers()
{
    if (CoordinatedGraphicsLayer* mask = downcast<CoordinatedGraphicsLayer>(maskLayer()))
        mask->updateContentBuffers();

    if (CoordinatedGraphicsLayer* replica = downcast<CoordinatedGraphicsLayer>(replicaLayer()))
        replica->updateContentBuffers();

    updateContentBuffers();

    for (auto& child : children())
        downcast<CoordinatedGraphicsLayer>(*child).updateContentBuffersIncludingSubLayers();
}

void CoordinatedGraphicsLayer::updateContentBuffers()
{
    if (!shouldHaveBackingStore()) {
        m_mainBackingStore = nullptr;
        m_previousBackingStore = nullptr;
        return;
    }

    if (m_pendingContentsScaleAdjustment) {
        adjustContentsScale();
        m_pendingContentsScaleAdjustment = false;
    }

    // This is the only place we (re)create the main tiled backing store, once we
    // have a remote client and we are ready to send our data to the UI process.
    if (!m_mainBackingStore) {
        createBackingStore();
        m_pendingVisibleRectAdjustment = true;
    }

    if (m_pendingVisibleRectAdjustment) {
        m_pendingVisibleRectAdjustment = false;
        m_mainBackingStore->createTilesIfNeeded(transformedVisibleRect(), IntRect(0, 0, size().width(), size().height()));
    }

    m_mainBackingStore->updateTileBuffers();

    // The previous backing store is kept around to avoid flickering between
    // removing the existing tiles and painting the new ones. The first time
    // the visibleRect is full painted we remove the previous backing store.
    if (m_mainBackingStore->visibleAreaIsCovered())
        m_previousBackingStore = nullptr;
}

void CoordinatedGraphicsLayer::purgeBackingStores()
{
#ifndef NDEBUG
    SetForScope<bool> updateModeProtector(m_isPurging, true);
#endif
    m_mainBackingStore = nullptr;
    m_previousBackingStore = nullptr;

    releaseImageBackingIfNeeded();

    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setCoordinator(CoordinatedGraphicsLayerClient* coordinator)
{
    m_coordinator = coordinator;
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
        offset += currLayer->position();

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
    if (m_movingVisibleRect)
        client().getCurrentTransform(this, currentTransform);
    m_layerTransform.setLocalTransform(currentTransform);

    m_layerTransform.setAnchorPoint(m_adjustedAnchorPoint);
    m_layerTransform.setPosition(m_adjustedPosition);
    m_layerTransform.setSize(m_adjustedSize);

    m_layerTransform.setFlattening(!preserves3D());
    m_layerTransform.setChildrenTransform(childrenTransform());
    m_layerTransform.combineTransforms(parent() ? downcast<CoordinatedGraphicsLayer>(*parent()).m_layerTransform.combinedForChildren() : TransformationMatrix());

    m_cachedInverseTransform = m_layerTransform.combined().inverse().value_or(TransformationMatrix());

    // The combined transform will be used in tiledBackingStoreVisibleRect.
    setNeedsVisibleRectAdjustment();
}

bool CoordinatedGraphicsLayer::shouldHaveBackingStore() const
{
    return drawsContent() && contentsAreVisible() && !m_size.isEmpty();
}

bool CoordinatedGraphicsLayer::selfOrAncestorHasActiveTransformAnimation() const
{
    if (m_animations.hasActiveAnimationsOfType(AnimatedPropertyTransform))
        return true;

    if (!parent())
        return false;

    return downcast<CoordinatedGraphicsLayer>(*parent()).selfOrAncestorHasActiveTransformAnimation();
}

bool CoordinatedGraphicsLayer::selfOrAncestorHaveNonAffineTransforms()
{
    if (m_animations.hasActiveAnimationsOfType(AnimatedPropertyTransform))
        return true;

    if (!m_layerTransform.combined().isAffine())
        return true;

    if (!parent())
        return false;

    return downcast<CoordinatedGraphicsLayer>(*parent()).selfOrAncestorHaveNonAffineTransforms();
}

bool CoordinatedGraphicsLayer::addAnimation(const KeyframeValueList& valueList, const FloatSize& boxSize, const Animation* anim, const String& keyframesName, double delayAsNegativeTimeOffset)
{
    ASSERT(!keyframesName.isEmpty());

    if (!anim || anim->isEmptyOrZeroDuration() || valueList.size() < 2 || (valueList.property() != AnimatedPropertyTransform && valueList.property() != AnimatedPropertyOpacity && valueList.property() != AnimatedPropertyFilter))
        return false;

    if (valueList.property() == AnimatedPropertyFilter) {
        int listIndex = validateFilterOperations(valueList);
        if (listIndex < 0)
            return false;

        const auto& filters = static_cast<const FilterAnimationValue&>(valueList.at(listIndex)).value();
        if (!filtersCanBeComposited(filters))
            return false;
    }

    bool listsMatch = false;
    bool ignoredHasBigRotation;

    if (valueList.property() == AnimatedPropertyTransform)
        listsMatch = validateTransformOperations(valueList, ignoredHasBigRotation) >= 0;

    m_lastAnimationStartTime = monotonicallyIncreasingTime() - delayAsNegativeTimeOffset;
    m_animations.add(TextureMapperAnimation(keyframesName, valueList, boxSize, *anim, listsMatch, m_lastAnimationStartTime, 0, TextureMapperAnimation::AnimationState::Playing));
    m_animationStartedTimer.startOneShot(0_s);
    didChangeAnimations();
    return true;
}

void CoordinatedGraphicsLayer::pauseAnimation(const String& animationName, double time)
{
    m_animations.pause(animationName, time);
    didChangeAnimations();
}

void CoordinatedGraphicsLayer::removeAnimation(const String& animationName)
{
    m_animations.remove(animationName);
    didChangeAnimations();
}

void CoordinatedGraphicsLayer::suspendAnimations(double time)
{
    m_animations.suspend(time);
    didChangeAnimations();
}

void CoordinatedGraphicsLayer::resumeAnimations()
{
    m_animations.resume();
    didChangeAnimations();
}

void CoordinatedGraphicsLayer::animationStartedTimerFired()
{
    client().notifyAnimationStarted(this, "", m_lastAnimationStartTime);
}

#if USE(COORDINATED_GRAPHICS_THREADED)
void CoordinatedGraphicsLayer::platformLayerWillBeDestroyed()
{
}

void CoordinatedGraphicsLayer::setPlatformLayerNeedsDisplay()
{
}
#endif

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
