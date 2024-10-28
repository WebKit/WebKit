/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

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


#pragma once

#if USE(COORDINATED_GRAPHICS)

#include "FloatPoint3D.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerTransform.h"
#include "Image.h"
#include "IntSize.h"
#include "NicosiaCompositionLayer.h"
#include "NicosiaPlatformLayer.h"
#include "TextureMapperAnimation.h"
#include "TransformationMatrix.h"
#include <wtf/Function.h>
#include <wtf/RunLoop.h>
#include <wtf/WorkerPool.h>
#include <wtf/text/StringHash.h>

#if USE(SKIA)
namespace WebCore {
class BitmapTexture;
class BitmapTexturePool;
class SkiaThreadedPaintingPool;
}
#endif

namespace WebCore {
class CoordinatedAnimatedBackingStoreClient;
class CoordinatedBackingStoreProxy;
class CoordinatedGraphicsLayer;
class CoordinatedImageBackingStore;
class CoordinatedTileBuffer;
class TextureMapperPlatformLayerProxy;
#if USE(CAIRO)
namespace Cairo {
class PaintingEngine;
}
#endif

class CoordinatedGraphicsLayerClient {
public:
    virtual bool isFlushingLayerChanges() const = 0;
    virtual FloatRect visibleContentsRect() const = 0;
    virtual void detachLayer(CoordinatedGraphicsLayer*) = 0;
    virtual void attachLayer(CoordinatedGraphicsLayer*) = 0;
#if USE(CAIRO)
    virtual Cairo::PaintingEngine& paintingEngine() = 0;
#elif USE(SKIA)
    virtual BitmapTexturePool* skiaAcceleratedBitmapTexturePool() const = 0;
    virtual SkiaThreadedPaintingPool* skiaThreadedPaintingPool() const = 0;
#endif

    virtual Ref<CoordinatedImageBackingStore> imageBackingStore(Ref<NativeImage>&&) = 0;
};

class WEBCORE_EXPORT CoordinatedGraphicsLayer : public GraphicsLayer {
public:
    explicit CoordinatedGraphicsLayer(Type, GraphicsLayerClient&);
    virtual ~CoordinatedGraphicsLayer();

    // FIXME: Merge these two methods.
    Nicosia::PlatformLayer::LayerID id() const;
    std::optional<PlatformLayerIdentifier> primaryLayerID() const override;

    // Reimplementations from GraphicsLayer.h.
    bool setChildren(Vector<Ref<GraphicsLayer>>&&) override;
    void addChild(Ref<GraphicsLayer>&&) override;
    void addChildAtIndex(Ref<GraphicsLayer>&&, int) override;
    void addChildAbove(Ref<GraphicsLayer>&&, GraphicsLayer*) override;
    void addChildBelow(Ref<GraphicsLayer>&&, GraphicsLayer*) override;
    bool replaceChild(GraphicsLayer*, Ref<GraphicsLayer>&&) override;
    void willModifyChildren() override;
    void setEventRegion(EventRegion&&) override;
#if ENABLE(SCROLLING_THREAD)
    void setScrollingNodeID(std::optional<ScrollingNodeID>) override;
#endif
    void setPosition(const FloatPoint&) override;
    void syncPosition(const FloatPoint&) override;
    void setAnchorPoint(const FloatPoint3D&) override;
    void setSize(const FloatSize&) override;
    void setBoundsOrigin(const FloatPoint&) override;
    void syncBoundsOrigin(const FloatPoint&) override;
    void setTransform(const TransformationMatrix&) override;
    void setChildrenTransform(const TransformationMatrix&) override;
    void setPreserves3D(bool) override;
    void setMasksToBounds(bool) override;
    void setDrawsContent(bool) override;
    void setContentsVisible(bool) override;
    void setContentsOpaque(bool) override;
    void setBackfaceVisibility(bool) override;
    void setOpacity(float) override;
    void setContentsRect(const FloatRect&) override;
    void setContentsTilePhase(const FloatSize&) override;
    void setContentsTileSize(const FloatSize&) override;
    void setContentsClippingRect(const FloatRoundedRect&) override;
    void setContentsRectClipsDescendants(bool) override;
    void setContentsToImage(Image*) override;
    void setContentsToSolidColor(const Color&) override;
    void setShowDebugBorder(bool) override;
    void setShowRepaintCounter(bool) override;
    bool shouldDirectlyCompositeImage(Image*) const override;
    void setContentsToPlatformLayer(PlatformLayer*, ContentsLayerPurpose) override;
    void setContentsDisplayDelegate(RefPtr<GraphicsLayerContentsDisplayDelegate>&&, ContentsLayerPurpose) override;
    RefPtr<GraphicsLayerAsyncContentsDisplayDelegate> createAsyncContentsDisplayDelegate(GraphicsLayerAsyncContentsDisplayDelegate*) override;
    void setMaskLayer(RefPtr<GraphicsLayer>&&) override;
    void setReplicatedByLayer(RefPtr<GraphicsLayer>&&) override;
    void setNeedsDisplay() override;
    void setNeedsDisplayInRect(const FloatRect&, ShouldClipToLayer = ClipToLayer) override;
    void setContentsNeedsDisplay() override;
    void markDamageRectsUnreliable() override;
    void deviceOrPageScaleFactorChanged() override;
    void flushCompositingState(const FloatRect&) override;
    void flushCompositingStateForThisLayerOnly() override;
    bool setFilters(const FilterOperations&) override;
    bool setBackdropFilters(const FilterOperations&) override;
    void setBackdropFiltersRect(const FloatRoundedRect&) override;
    bool addAnimation(const KeyframeValueList&, const FloatSize&, const Animation*, const String&, double) override;
    void pauseAnimation(const String&, double) override;
    void removeAnimation(const String&, std::optional<AnimatedProperty>) override;
    void suspendAnimations(MonotonicTime) override;
    void resumeAnimations() override;
    void transformRelatedPropertyDidChange() override;
    bool usesContentsLayer() const override;
    void dumpAdditionalProperties(WTF::TextStream&, OptionSet<LayerTreeAsTextOptions>) const override;

    std::pair<bool, bool> finalizeCompositingStateFlush();

    FloatPoint computePositionRelativeToBase();
    void computePixelAlignment(FloatPoint& position, FloatSize&, FloatPoint3D& anchorPoint, FloatSize& alignmentOffset);

    IntRect transformedVisibleRect();
    IntRect transformedVisibleRectIncludingFuture();

    void invalidateCoordinator();
    void setCoordinatorIncludingSubLayersIfNeeded(CoordinatedGraphicsLayerClient*);

    void setNeedsVisibleRectAdjustment();
    void purgeBackingStores();

    const RefPtr<Nicosia::CompositionLayer>& compositionLayer() const;

    void requestBackingStoreUpdate();

    double backingStoreMemoryEstimate() const override;

    Vector<std::pair<String, double>> acceleratedAnimationsForTesting(const Settings&) const final;

#if USE(SKIA)
    void paintIntoGraphicsContext(GraphicsContext&, const IntRect&) const;
#endif

    float effectiveContentsScale() const;

    static void clampToContentsRectIfRectIsInfinite(FloatRect&, const FloatSize&);

    Ref<CoordinatedTileBuffer> paintTile(const IntRect& dirtyRect);

private:
    enum class FlushNotification {
        Required,
        NotRequired,
    };

    bool isCoordinatedGraphicsLayer() const override;

    void setDebugBorder(const Color&, float width) override;

    void didChangeAnimations();
    void didChangeGeometry(FlushNotification = FlushNotification::Required);
    void didChangeChildren();
    void didChangeFilters();
    void didChangeBackdropFilters();
    void didChangeBackdropFiltersRect();
    void didUpdateTileBuffers();

    void computeTransformedVisibleRect();
    void updateContentBuffers();

    bool checkPendingStateChanges();
    bool checkContentLayerUpdated();

    void notifyFlushRequired();

    bool shouldHaveBackingStore() const;
    bool selfOrAncestorHasActiveTransformAnimation() const;
    bool selfOrAncestorHaveNonAffineTransforms() const;

    void setShouldUpdateVisibleRect();

    void animationStartedTimerFired();
    void requestPendingTileCreationTimerFired();

    bool filtersCanBeComposited(const FilterOperations&) const;

    Nicosia::PlatformLayer::LayerID m_id;
    GraphicsLayerTransform m_layerTransform;
    GraphicsLayerTransform m_layerFutureTransform;
    TransformationMatrix m_cachedInverseTransform;
    TransformationMatrix m_cachedFutureInverseTransform;
    TransformationMatrix m_cachedCombinedTransform;
    FloatSize m_pixelAlignmentOffset;
    FloatSize m_adjustedSize;
    FloatPoint m_adjustedPosition;
    FloatPoint3D m_adjustedAnchorPoint;

    Color m_solidColor;

#ifndef NDEBUG
    bool m_isPurging;
#endif
    bool m_shouldUpdateVisibleRect: 1;
    bool m_movingVisibleRect : 1;
    bool m_pendingContentsScaleAdjustment : 1;
    bool m_pendingVisibleRectAdjustment : 1;

    CoordinatedGraphicsLayerClient* m_coordinator;

    struct {
        bool completeLayer { false };
        Vector<FloatRect> rects;
    } m_needsDisplay;
    bool m_damagedRectsAreUnreliable { false };

    Timer m_animationStartedTimer;
    RunLoop::Timer m_requestPendingTileCreationTimer;
    TextureMapperAnimations m_animations;
    MonotonicTime m_lastAnimationStartTime;

    struct {
        RefPtr<Nicosia::CompositionLayer> layer;
        Nicosia::CompositionLayer::LayerState::Delta delta;
        Nicosia::CompositionLayer::LayerState::RepaintCounter repaintCounter;
        Nicosia::CompositionLayer::LayerState::DebugBorder debugBorder;
        bool performLayerSync { false };
    } m_nicosia;

    RefPtr<CoordinatedBackingStoreProxy> m_backingStore;
    RefPtr<CoordinatedAnimatedBackingStoreClient> m_animatedBackingStoreClient;

    RefPtr<NativeImage> m_pendingContentsImage;
    struct {
        RefPtr<CoordinatedImageBackingStore> store;
        bool isVisible { false };
    } m_imageBacking;

    RefPtr<TextureMapperPlatformLayerProxy> m_contentsLayer;
    bool m_contentsLayerNeedsUpdate { false };
    bool m_contentsLayerUpdated { false };

    RefPtr<CoordinatedGraphicsLayer> m_backdropLayer;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_GRAPHICSLAYER(WebCore::CoordinatedGraphicsLayer, isCoordinatedGraphicsLayer())

#endif // USE(COORDINATED_GRAPHICS)
