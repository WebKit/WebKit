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
#include "NicosiaBackingStoreTextureMapperImpl.h"
#include "NicosiaCompositionLayerTextureMapperImpl.h"
#include "NicosiaContentLayerTextureMapperImpl.h"
#include "NicosiaImageBackingTextureMapperImpl.h"
#include "NicosiaPaintingContext.h"
#include "NicosiaPaintingEngine.h"
#include "ScrollableArea.h"
#include "TextureMapperPlatformLayerProxyProvider.h"
#include "TiledBackingStore.h"
#ifndef NDEBUG
#include <wtf/SetForScope.h>
#endif
#include <wtf/text/CString.h>

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
}

void CoordinatedGraphicsLayer::didChangeGeometry()
{
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
    , m_shouldUpdatePlatformLayer(false)
    , m_coordinator(0)
    , m_compositedNativeImagePtr(0)
    , m_animationStartedTimer(*this, &CoordinatedGraphicsLayer::animationStartedTimerFired)
    , m_requestPendingTileCreationTimer(RunLoop::main(), this, &CoordinatedGraphicsLayer::requestPendingTileCreationTimerFired)
{
    static Nicosia::PlatformLayer::LayerID nextLayerID = 1;
    m_id = nextLayerID++;

    m_nicosia.layer = Nicosia::CompositionLayer::create(m_id,
        Nicosia::CompositionLayerTextureMapperImpl::createFactory());

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
        m_coordinator->detachLayer(this);
    }
    ASSERT(!m_nicosia.imageBacking);
    ASSERT(!m_nicosia.backingStore);
    if (m_animatedBackingStoreHost)
        m_animatedBackingStoreHost->layerWillBeDestroyed();
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

auto CoordinatedGraphicsLayer::primaryLayerID() const -> PlatformLayerID
{
    return id();
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
    m_nicosia.delta.positionChanged = true;
    didChangeGeometry();
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

bool GraphicsLayer::supportsContentsTiling()
{
    return true;
}

void CoordinatedGraphicsLayer::setContentsNeedsDisplay()
{
#if USE(COORDINATED_GRAPHICS) && USE(NICOSIA)
    if (m_nicosia.contentLayer)
        m_shouldUpdatePlatformLayer = true;
#endif

    notifyFlushRequired();
    addRepaintRect(contentsRect());
}

void CoordinatedGraphicsLayer::setContentsToPlatformLayer(PlatformLayer* platformLayer, ContentsLayerPurpose)
{
#if USE(COORDINATED_GRAPHICS) && USE(NICOSIA)
    auto* contentLayer = downcast<Nicosia::ContentLayer>(platformLayer);
    if (m_nicosia.contentLayer != contentLayer) {
        m_nicosia.contentLayer = contentLayer;
        m_nicosia.delta.contentLayerChanged = true;
        if (m_nicosia.contentLayer)
            m_shouldUpdatePlatformLayer = true;
    }
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
    NativeImagePtr nativeImagePtr = image ? image->nativeImageForCurrentFrame() : nullptr;
    if (m_compositedImage == image && m_compositedNativeImagePtr == nativeImagePtr)
        return;

    m_compositedImage = image;
    m_compositedNativeImagePtr = nativeImagePtr;

    GraphicsLayer::setContentsToImage(image);
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

    enum { MaxDimenstionForDirectCompositing = 2000 };
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

    if (rects.size() < 32)
        rects.append(rect);
    else
        rects[0].unite(rect);

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

void CoordinatedGraphicsLayer::updatePlatformLayer()
{
    if (!m_shouldUpdatePlatformLayer)
        return;

    m_shouldUpdatePlatformLayer = false;
#if USE(COORDINATED_GRAPHICS) && USE(NICOSIA)
    if (m_nicosia.contentLayer)
        downcast<Nicosia::ContentLayerTextureMapperImpl>(m_nicosia.contentLayer->impl()).swapBuffersIfNeeded();
#endif
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

class CoordinatedAnimatedBackingStoreClient final : public Nicosia::AnimatedBackingStoreClient {
public:
    static Ref<CoordinatedAnimatedBackingStoreClient> create(RefPtr<CoordinatedGraphicsLayer::AnimatedBackingStoreHost>&& host, const FloatRect& visibleRect, const FloatRect& coverRect, const FloatSize& size, float contentsScale)
    {
        return adoptRef(*new CoordinatedAnimatedBackingStoreClient(WTFMove(host), visibleRect, coverRect, size, contentsScale));
    }

    ~CoordinatedAnimatedBackingStoreClient() = default;

    void setCoverRect(const IntRect& rect) { m_coverRect = rect; }
    void requestBackingStoreUpdateIfNeeded(const TransformationMatrix& transform) final
    {
        ASSERT(!isMainThread());

        // Calculate the contents rectangle of the layer in backingStore coordinates.
        FloatRect contentsRect = { { 0, 0 }, m_size };
        contentsRect.scale(m_contentsScale);

        // If the area covered by tiles (the coverRect, already in backingStore coordinates) covers the whole
        // layer contents then we don't need to do anything.
        if (m_coverRect.contains(contentsRect))
            return;

        // Non-invertible layers are not visible.
        if (!transform.isInvertible())
            return;

        // Calculate the inverse of the layer transformation. The inverse transform will have the inverse of the
        // scaleFactor applied, so we need to scale it back.
        TransformationMatrix inverse = transform.inverse().valueOr(TransformationMatrix()).scale(m_contentsScale);

        // Apply the inverse transform to the visible rectangle, so we have the visible rectangle in layer coordinates.
        FloatRect rect = inverse.clampedBoundsOfProjectedQuad(FloatQuad(m_visibleRect));
        clampToContentsRectIfRectIsInfinite(rect, m_size);
        FloatRect transformedVisibleRect = enclosingIntRect(rect);

        // Convert the calculated visible rectangle to backingStore coordinates.
        transformedVisibleRect.scale(m_contentsScale);

        // Restrict the calculated visible rect to the contents rectangle of the layer.
        transformedVisibleRect.intersect(contentsRect);

        // If the coverRect doesn't contain the calculated visible rectangle we need to request a backingStore
        // update to render more tiles.
        if (!m_coverRect.contains(transformedVisibleRect)) {
            callOnMainThread([protectedHost = m_host.copyRef()]() {
                protectedHost->requestBackingStoreUpdate();
            });
        }
    }

private:
    CoordinatedAnimatedBackingStoreClient(RefPtr<CoordinatedGraphicsLayer::AnimatedBackingStoreHost>&& host, const FloatRect& visibleRect, const FloatRect& coverRect, const FloatSize& size, float contentsScale)
        : Nicosia::AnimatedBackingStoreClient(Type::Coordinated)
        , m_host(WTFMove(host))
        , m_visibleRect(visibleRect)
        , m_coverRect(coverRect)
        , m_size(size)
        , m_contentsScale(contentsScale)
    { }

    RefPtr<CoordinatedGraphicsLayer::AnimatedBackingStoreHost> m_host;
    FloatRect m_visibleRect;
    FloatRect m_coverRect;
    FloatSize m_size;
    float m_contentsScale;
};

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
    updatePlatformLayer();

    // Only unset m_movingVisibleRect after we have updated the visible rect after the animation stopped.
    if (!hasActiveTransformAnimation)
        m_movingVisibleRect = false;

    // Determine the backing store presence. Content is painted later, in the updateContentBuffers() traversal.
    if (shouldHaveBackingStore()) {
        if (!m_nicosia.backingStore) {
            m_nicosia.backingStore = Nicosia::BackingStore::create(Nicosia::BackingStoreTextureMapperImpl::createFactory());
            m_nicosia.delta.backingStoreChanged = true;
        }
    } else if (m_nicosia.backingStore) {
        auto& layerState = downcast<Nicosia::BackingStoreTextureMapperImpl>(m_nicosia.backingStore->impl()).layerState();
        layerState.isPurging = true;
        layerState.mainBackingStore = nullptr;

        m_nicosia.backingStore = nullptr;
        m_nicosia.delta.backingStoreChanged = true;
    }

    if (hasActiveTransformAnimation && m_nicosia.backingStore) {
        // The layer has a backingStore and a transformation animation. This means that we need to add an
        // AnimatedBackingStoreClient to check whether we need to update the backingStore due to the animation.
        // At this point we don't know the area covered by tiles available, so we just pass an empty rectangle
        // for that. The call to updateContentBuffers will calculate the tile coverage and set the appropriate
        // rectangle to the client.
        if (!m_animatedBackingStoreHost)
            m_animatedBackingStoreHost = AnimatedBackingStoreHost::create(*this);
        m_nicosia.animatedBackingStoreClient = CoordinatedAnimatedBackingStoreClient::create(m_animatedBackingStoreHost.copyRef(), m_coordinator->visibleContentsRect(), { }, m_size, effectiveContentsScale());
    }
    // Each layer flush changes the AnimatedBackingStoreClient, being it null or a real one.
    m_nicosia.delta.animatedBackingStoreClientChanged = true;

    // Determine image backing presence according to the composited image source.
    if (m_compositedNativeImagePtr) {
        ASSERT(m_compositedImage);
        auto& image = *m_compositedImage;
        uintptr_t imageID = reinterpret_cast<uintptr_t>(&image);
        uintptr_t nativeImageID = reinterpret_cast<uintptr_t>(m_compositedNativeImagePtr.get());

        // Respawn the ImageBacking object if the underlying image changed.
        if (m_nicosia.imageBacking) {
            auto& impl = downcast<Nicosia::ImageBackingTextureMapperImpl>(m_nicosia.imageBacking->impl());
            if (impl.layerState().imageID != imageID) {
                impl.layerState().update = Nicosia::ImageBackingTextureMapperImpl::Update { };
                m_nicosia.imageBacking = nullptr;
            }
        }
        if (!m_nicosia.imageBacking) {
            m_nicosia.imageBacking = Nicosia::ImageBacking::create(Nicosia::ImageBackingTextureMapperImpl::createFactory());
            m_nicosia.delta.imageBackingChanged = true;
        }

        // Update the image contents only when the image layer is visible and the native image changed.
        auto& impl = downcast<Nicosia::ImageBackingTextureMapperImpl>(m_nicosia.imageBacking->impl());
        auto& layerState = impl.layerState();
        layerState.imageID = imageID;
        layerState.update.isVisible = transformedVisibleRect().intersects(IntRect(contentsRect()));
        if (layerState.update.isVisible && layerState.nativeImageID != nativeImageID) {
            auto buffer = Nicosia::Buffer::create(IntSize(image.size()),
                !image.currentFrameKnownToBeOpaque() ? Nicosia::Buffer::SupportsAlpha : Nicosia::Buffer::NoFlags);
            Nicosia::PaintingContext::paint(buffer,
                [&image](GraphicsContext& context)
                {
                    IntRect rect { { }, IntSize { image.size() } };
                    context.drawImage(image, rect, rect, ImagePaintingOptions(CompositeCopy));
                });
            layerState.nativeImageID = nativeImageID;
            layerState.update.buffer = WTFMove(buffer);
            m_nicosia.delta.imageBackingChanged = true;
        }
    } else if (m_nicosia.imageBacking) {
        auto& layerState = downcast<Nicosia::ImageBackingTextureMapperImpl>(m_nicosia.imageBacking->impl()).layerState();
        layerState.update = Nicosia::ImageBackingTextureMapperImpl::Update { };
        m_nicosia.imageBacking = nullptr;
        m_nicosia.delta.imageBackingChanged = true;
    }

    {
        m_nicosia.layer->updateState(
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

                if (localDelta.opacityChanged)
                    state.opacity = opacity();
                if (localDelta.solidColorChanged)
                    state.solidColor = m_solidColor;

                if (localDelta.filtersChanged)
                    state.filters = filters();
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
                    state.flags.preserves3D = preserves3D();
                }

                if (localDelta.repaintCounterChanged)
                    state.repaintCounter = m_nicosia.repaintCounter;
                if (localDelta.debugBorderChanged)
                    state.debugBorder = m_nicosia.debugBorder;

                if (localDelta.backingStoreChanged)
                    state.backingStore = m_nicosia.backingStore;
                if (localDelta.contentLayerChanged)
                    state.contentLayer = m_nicosia.contentLayer;
                if (localDelta.imageBackingChanged)
                    state.imageBacking = m_nicosia.imageBacking;
                if (localDelta.animatedBackingStoreClientChanged)
                    state.animatedBackingStoreClient = m_nicosia.animatedBackingStoreClient;
            });
        m_nicosia.performLayerSync = !!m_nicosia.delta.value;
        m_nicosia.delta = { };
    }
}

void CoordinatedGraphicsLayer::syncPendingStateChangesIncludingSubLayers()
{
    if (m_nicosia.performLayerSync)
        m_coordinator->syncLayerState();
    m_nicosia.performLayerSync = false;

    if (maskLayer())
        downcast<CoordinatedGraphicsLayer>(*maskLayer()).syncPendingStateChangesIncludingSubLayers();

    for (auto& child : children())
        downcast<CoordinatedGraphicsLayer>(child.get()).syncPendingStateChangesIncludingSubLayers();
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

IntRect CoordinatedGraphicsLayer::transformedVisibleRect()
{
    // Non-invertible layers are not visible.
    if (!m_layerTransform.combined().isInvertible())
        return IntRect();

    // Return a projection of the visible rect (surface coordinates) onto the layer's plane (layer coordinates).
    // The resulting quad might be squewed and the visible rect is the bounding box of this quad,
    // so it might spread further than the real visible area (and then even more amplified by the cover rect multiplier).
    ASSERT(m_cachedInverseTransform == m_layerTransform.combined().inverse().valueOr(TransformationMatrix()));
    FloatRect rect = m_cachedInverseTransform.clampedBoundsOfProjectedQuad(FloatQuad(m_coordinator->visibleContentsRect()));
    clampToContentsRectIfRectIsInfinite(rect, size());
    return enclosingIntRect(rect);
}

void CoordinatedGraphicsLayer::requestBackingStoreUpdate()
{
    setNeedsVisibleRectAdjustment();
    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::updateContentBuffersIncludingSubLayers()
{
    if (CoordinatedGraphicsLayer* mask = downcast<CoordinatedGraphicsLayer>(maskLayer()))
        mask->updateContentBuffers();

    if (CoordinatedGraphicsLayer* replica = downcast<CoordinatedGraphicsLayer>(replicaLayer()))
        replica->updateContentBuffers();

    updateContentBuffers();

    for (auto& child : children())
        downcast<CoordinatedGraphicsLayer>(child.get()).updateContentBuffersIncludingSubLayers();
}

void CoordinatedGraphicsLayer::updateContentBuffers()
{
    if (!m_nicosia.backingStore)
        return;

    // Prepare for painting on the impl-contained backing store. isFlushing is used there
    // for internal sanity checks.
    auto& impl = downcast<Nicosia::BackingStoreTextureMapperImpl>(m_nicosia.backingStore->impl());
    auto& layerState = impl.layerState();
    layerState.isFlushing = true;

    // Helper lambda that finished the flush update and determines layer sync necessity.
    auto finishUpdate =
        [this, &layerState] {
            auto& update = layerState.update;
            m_nicosia.performLayerSync |= !update.tilesToCreate.isEmpty()
                || !update.tilesToRemove.isEmpty() || !update.tilesToUpdate.isEmpty();
            layerState.isFlushing = false;
        };

    // Address the content scale adjustment.
    if (m_pendingContentsScaleAdjustment) {
        if (layerState.mainBackingStore && layerState.mainBackingStore->contentsScale() != effectiveContentsScale()) {
            // Discard the TiledBackingStore object to reconstruct it with new content scale.
            layerState.mainBackingStore = nullptr;
        }
        m_pendingContentsScaleAdjustment = false;
    }

    // Ensure the TiledBackingStore object, and enforce a complete repaint if it's not been present yet.
    if (!layerState.mainBackingStore) {
        layerState.mainBackingStore = makeUnique<TiledBackingStore>(impl, effectiveContentsScale());
        m_pendingVisibleRectAdjustment = true;
    }

    // Bail if there's no painting recorded or enforced.
    if (!m_pendingVisibleRectAdjustment && !m_needsDisplay.completeLayer && m_needsDisplay.rects.isEmpty()) {
        finishUpdate();
        return;
    }

    if (!m_needsDisplay.completeLayer) {
        for (auto& rect : m_needsDisplay.rects)
            layerState.mainBackingStore->invalidate(enclosingIntRect(rect));
    } else
        layerState.mainBackingStore->invalidate({ { }, IntSize { m_size } });

    m_needsDisplay.completeLayer = false;
    m_needsDisplay.rects.clear();

    if (m_pendingVisibleRectAdjustment) {
        m_pendingVisibleRectAdjustment = false;
        layerState.mainBackingStore->createTilesIfNeeded(transformedVisibleRect(), IntRect(0, 0, m_size.width(), m_size.height()));
    }

    if (is<CoordinatedAnimatedBackingStoreClient>(m_nicosia.animatedBackingStoreClient)) {
        // Determine the coverRect and set it to the client.
        downcast<CoordinatedAnimatedBackingStoreClient>(*m_nicosia.animatedBackingStoreClient).setCoverRect(layerState.mainBackingStore->coverRect());
    }

    ASSERT(m_coordinator && m_coordinator->isFlushingLayerChanges());

    // With all the affected tiles created and/or invalidated, we can finally paint them.
    auto dirtyTiles = layerState.mainBackingStore->dirtyTiles();
    if (!dirtyTiles.isEmpty()) {
        bool didUpdateTiles = false;

        for (auto& tileReference : dirtyTiles) {
            auto& tile = tileReference.get();
            tile.ensureTileID();

            auto& tileRect = tile.rect();
            auto& dirtyRect = tile.dirtyRect();

            auto coordinatedBuffer = Nicosia::Buffer::create(dirtyRect.size(), contentsOpaque() ? Nicosia::Buffer::NoFlags : Nicosia::Buffer::SupportsAlpha);
            SurfaceUpdateInfo updateInfo;
            updateInfo.updateRect = dirtyRect;
            updateInfo.updateRect.move(-tileRect.x(), -tileRect.y());
            updateInfo.buffer = coordinatedBuffer.copyRef();

            if (!m_coordinator->paintingEngine().paint(*this, WTFMove(coordinatedBuffer),
                dirtyRect, layerState.mainBackingStore->mapToContents(dirtyRect),
                IntRect { { 0, 0 }, dirtyRect.size() }, layerState.mainBackingStore->contentsScale()))
                continue;

            impl.updateTile(tile.tileID(), updateInfo, tileRect);

            tile.markClean();
            didUpdateTiles |= true;
        }

        if (didUpdateTiles)
            didUpdateTileBuffers();
    }

    // Request a new update immediately if some tiles are still pending creation. Do this on a timer
    // as we're in a layer flush and flush requests at this point would be discarded.
    if (layerState.hasPendingTileCreation) {
        setNeedsVisibleRectAdjustment();
        m_requestPendingTileCreationTimer.startOneShot(0_s);
    }

    finishUpdate();
}

void CoordinatedGraphicsLayer::purgeBackingStores()
{
#ifndef NDEBUG
    SetForScope<bool> updateModeProtector(m_isPurging, true);
#endif
    if (m_nicosia.backingStore) {
        auto& layerState = downcast<Nicosia::BackingStoreTextureMapperImpl>(m_nicosia.backingStore->impl()).layerState();
        layerState.isPurging = true;
        layerState.mainBackingStore = nullptr;

        m_nicosia.backingStore = nullptr;
    }

    if (m_nicosia.imageBacking) {
        auto& layerState = downcast<Nicosia::ImageBackingTextureMapperImpl>(m_nicosia.imageBacking->impl()).layerState();
        layerState.imageID = 0;
        layerState.nativeImageID = 0;
        layerState.update = { };

        m_nicosia.imageBacking = nullptr;
    }

    notifyFlushRequired();
}

void CoordinatedGraphicsLayer::setCoordinator(CoordinatedGraphicsLayerClient* coordinator)
{
    m_coordinator = coordinator;
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

    coordinator->attachLayer(this);
    for (auto& child : children())
        downcast<CoordinatedGraphicsLayer>(child.get()).setCoordinatorIncludingSubLayersIfNeeded(coordinator);
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

    m_cachedInverseTransform = m_layerTransform.combined().inverse().valueOr(TransformationMatrix());

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

    m_lastAnimationStartTime = MonotonicTime::now() - Seconds(delayAsNegativeTimeOffset);
    m_animations.add(Nicosia::Animation(keyframesName, valueList, boxSize, *anim, listsMatch, m_lastAnimationStartTime, 0_s, Nicosia::Animation::AnimationState::Playing));
    m_animationStartedTimer.startOneShot(0_s);
    didChangeAnimations();
    return true;
}

void CoordinatedGraphicsLayer::pauseAnimation(const String& animationName, double time)
{
    m_animations.pause(animationName, Seconds(time));
    didChangeAnimations();
}

void CoordinatedGraphicsLayer::removeAnimation(const String& animationName)
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

void CoordinatedGraphicsLayer::animationStartedTimerFired()
{
    client().notifyAnimationStarted(this, "", m_lastAnimationStartTime);
}

void CoordinatedGraphicsLayer::requestPendingTileCreationTimerFired()
{
    notifyFlushRequired();
}

bool CoordinatedGraphicsLayer::usesContentsLayer() const
{
    return m_nicosia.contentLayer || m_compositedImage;
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ANIMATEDBACKINGSTORECLIENT(WebCore::CoordinatedAnimatedBackingStoreClient, type() == Nicosia::AnimatedBackingStoreClient::Type::Coordinated)

#endif // USE(COORDINATED_GRAPHICS)
