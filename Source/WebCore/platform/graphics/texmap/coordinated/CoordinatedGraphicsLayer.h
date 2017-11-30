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


#ifndef CoordinatedGraphicsLayer_h
#define CoordinatedGraphicsLayer_h

#if USE(COORDINATED_GRAPHICS)

#include "CoordinatedGraphicsState.h"
#include "CoordinatedImageBacking.h"
#include "FloatPoint3D.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerTransform.h"
#include "Image.h"
#include "IntSize.h"
#include "NicosiaBuffer.h"
#include "TextureMapperAnimation.h"
#include "TiledBackingStore.h"
#include "TiledBackingStoreClient.h"
#include "TransformationMatrix.h"
#include <wtf/text/StringHash.h>

namespace Nicosia {
class PaintingEngine;
}

namespace WebCore {
class CoordinatedGraphicsLayer;
class TextureMapperAnimations;
class ScrollableArea;

class CoordinatedGraphicsLayerClient {
public:
    virtual bool isFlushingLayerChanges() const = 0;
    virtual FloatRect visibleContentsRect() const = 0;
    virtual Ref<CoordinatedImageBacking> createImageBackingIfNeeded(Image&) = 0;
    virtual void detachLayer(CoordinatedGraphicsLayer*) = 0;
    virtual Ref<Nicosia::Buffer> getCoordinatedBuffer(const IntSize&, Nicosia::Buffer::Flags, uint32_t&, IntRect&) = 0;
    virtual Nicosia::PaintingEngine& paintingEngine() = 0;

    virtual void syncLayerState(CoordinatedLayerID, CoordinatedGraphicsLayerState&) = 0;
};

class CoordinatedGraphicsLayer : public GraphicsLayer
    , public TiledBackingStoreClient
    , public CoordinatedImageBacking::Host {
public:
    explicit CoordinatedGraphicsLayer(Type, GraphicsLayerClient&);
    virtual ~CoordinatedGraphicsLayer();

    PlatformLayerID primaryLayerID() const override { return id(); }

    // Reimplementations from GraphicsLayer.h.
    bool setChildren(const Vector<GraphicsLayer*>&) override;
    void addChild(GraphicsLayer*) override;
    void addChildAtIndex(GraphicsLayer*, int) override;
    void addChildAbove(GraphicsLayer*, GraphicsLayer*) override;
    void addChildBelow(GraphicsLayer*, GraphicsLayer*) override;
    bool replaceChild(GraphicsLayer*, GraphicsLayer*) override;
    void removeFromParent() override;
    void setPosition(const FloatPoint&) override;
    void setAnchorPoint(const FloatPoint3D&) override;
    void setSize(const FloatSize&) override;
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
    void setContentsToImage(Image*) override;
    void setContentsToSolidColor(const Color&) override;
    void setShowDebugBorder(bool) override;
    void setShowRepaintCounter(bool) override;
    bool shouldDirectlyCompositeImage(Image*) const override;
    void setContentsToPlatformLayer(PlatformLayer*, ContentsLayerPurpose) override;
    void setMaskLayer(GraphicsLayer*) override;
    void setReplicatedByLayer(GraphicsLayer*) override;
    void setNeedsDisplay() override;
    void setNeedsDisplayInRect(const FloatRect&, ShouldClipToLayer = ClipToLayer) override;
    void setContentsNeedsDisplay() override;
    void deviceOrPageScaleFactorChanged() override;
    void flushCompositingState(const FloatRect&) override;
    void flushCompositingStateForThisLayerOnly() override;
    bool setFilters(const FilterOperations&) override;
    bool addAnimation(const KeyframeValueList&, const FloatSize&, const Animation*, const String&, double) override;
    void pauseAnimation(const String&, double) override;
    void removeAnimation(const String&) override;
    void suspendAnimations(double time) override;
    void resumeAnimations() override;
    bool usesContentsLayer() const override { return m_platformLayer || m_compositedImage; }

    void syncPendingStateChangesIncludingSubLayers();
    void updateContentBuffersIncludingSubLayers();

    FloatPoint computePositionRelativeToBase();
    void computePixelAlignment(FloatPoint& position, FloatSize&, FloatPoint3D& anchorPoint, FloatSize& alignmentOffset);

    void setVisibleContentRectTrajectoryVector(const FloatPoint&);

    void setScrollableArea(ScrollableArea*);
    bool isScrollable() const { return !!m_scrollableArea; }
    void commitScrollOffset(const IntSize&);

    CoordinatedLayerID id() const { return m_id; }

    void setFixedToViewport(bool isFixed);

    IntRect coverRect() const { return m_mainBackingStore ? m_mainBackingStore->mapToContents(m_mainBackingStore->coverRect()) : IntRect(); }
    IntRect transformedVisibleRect();

    // TiledBackingStoreClient
    void tiledBackingStoreHasPendingTileCreation() override;
    void createTile(uint32_t tileID, float) override;
    void updateTile(uint32_t tileID, const SurfaceUpdateInfo&, const IntRect&) override;
    void removeTile(uint32_t tileID) override;

    void setCoordinator(CoordinatedGraphicsLayerClient*);

    void setNeedsVisibleRectAdjustment();
    void purgeBackingStores();

    CoordinatedGraphicsLayer* findFirstDescendantWithContentsRecursively();

private:
    bool isCoordinatedGraphicsLayer() const override { return true; }

    void syncPlatformLayer();
    void updatePlatformLayer();

    void setDebugBorder(const Color&, float width) override;

    bool fixedToViewport() const { return m_fixedToViewport; }

    void didChangeLayerState();
    void didChangeAnimations();
    void didChangeGeometry();
    void didChangeChildren();
    void didChangeFilters();
    void didChangeImageBacking();
    void didUpdateTileBuffers();

    void resetLayerState();
    void syncLayerState();
    void syncAnimations();
    void syncChildren();
    void syncFilters();
    void syncImageBacking();
    void computeTransformedVisibleRect();
    void updateContentBuffers();

    void createBackingStore();
    void releaseImageBackingIfNeeded();

    void notifyFlushRequired();

    // CoordinatedImageBacking::Host
    bool imageBackingVisible() override;
    bool shouldHaveBackingStore() const;
    bool selfOrAncestorHasActiveTransformAnimation() const;
    bool selfOrAncestorHaveNonAffineTransforms();
    void adjustContentsScale();

    void setShouldUpdateVisibleRect();
    float effectiveContentsScale();

    void animationStartedTimerFired();

    bool filtersCanBeComposited(const FilterOperations&) const;

    CoordinatedLayerID m_id;
    CoordinatedGraphicsLayerState m_layerState;
    GraphicsLayerTransform m_layerTransform;
    TransformationMatrix m_cachedInverseTransform;
    FloatSize m_pixelAlignmentOffset;
    FloatSize m_adjustedSize;
    FloatPoint m_adjustedPosition;
    FloatPoint3D m_adjustedAnchorPoint;

#ifndef NDEBUG
    bool m_isPurging;
#endif
    bool m_shouldUpdateVisibleRect: 1;
    bool m_shouldSyncLayerState: 1;
    bool m_shouldSyncChildren: 1;
    bool m_shouldSyncFilters: 1;
    bool m_shouldSyncImageBacking: 1;
    bool m_shouldSyncAnimations: 1;
    bool m_fixedToViewport : 1;
    bool m_movingVisibleRect : 1;
    bool m_pendingContentsScaleAdjustment : 1;
    bool m_pendingVisibleRectAdjustment : 1;
#if USE(COORDINATED_GRAPHICS_THREADED)
    bool m_shouldSyncPlatformLayer : 1;
    bool m_shouldUpdatePlatformLayer : 1;
#endif

    CoordinatedGraphicsLayerClient* m_coordinator;
    std::unique_ptr<TiledBackingStore> m_mainBackingStore;
    std::unique_ptr<TiledBackingStore> m_previousBackingStore;

    RefPtr<Image> m_compositedImage;
    NativeImagePtr m_compositedNativeImagePtr;
    RefPtr<CoordinatedImageBacking> m_coordinatedImageBacking;

    PlatformLayer* m_platformLayer;
    Timer m_animationStartedTimer;
    TextureMapperAnimations m_animations;
    double m_lastAnimationStartTime { 0.0 };

    ScrollableArea* m_scrollableArea;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_GRAPHICSLAYER(WebCore::CoordinatedGraphicsLayer, isCoordinatedGraphicsLayer())

#endif // USE(COORDINATED_GRAPHICS)

#endif // CoordinatedGraphicsLayer_h
