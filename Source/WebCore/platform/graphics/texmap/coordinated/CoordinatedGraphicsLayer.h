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

#include "CoordinatedGraphicsState.h"
#include "CoordinatedImageBacking.h"
#include "FloatPoint3D.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerTransform.h"
#include "Image.h"
#include "IntSize.h"
#include "TextureMapperAnimation.h"
#include "TextureMapperPlatformLayer.h"
#include "TiledBackingStore.h"
#include "TiledBackingStoreClient.h"
#include "TransformationMatrix.h"
#if USE(GRAPHICS_SURFACE)
#include "GraphicsSurfaceToken.h"
#endif
#include <wtf/text/StringHash.h>

#if USE(COORDINATED_GRAPHICS)

namespace WebCore {
class CoordinatedGraphicsLayer;
class TextureMapperAnimations;
class ScrollableArea;

class CoordinatedGraphicsLayerClient {
public:
    virtual bool isFlushingLayerChanges() const = 0;
    virtual FloatRect visibleContentsRect() const = 0;
    virtual PassRefPtr<CoordinatedImageBacking> createImageBackingIfNeeded(Image*) = 0;
    virtual void detachLayer(CoordinatedGraphicsLayer*) = 0;
    virtual bool paintToSurface(const IntSize&, CoordinatedSurface::Flags, uint32_t& atlasID, IntPoint&, CoordinatedSurface::Client*) = 0;

    virtual void syncLayerState(CoordinatedLayerID, CoordinatedGraphicsLayerState&) = 0;
};

class CoordinatedGraphicsLayer : public GraphicsLayer
    , public TiledBackingStoreClient
#if USE(COORDINATED_GRAPHICS_THREADED)
    , public TextureMapperPlatformLayer::Client
#endif
    , public CoordinatedImageBacking::Host {
public:
    explicit CoordinatedGraphicsLayer(Type, GraphicsLayerClient&);
    virtual ~CoordinatedGraphicsLayer();

    PlatformLayerID primaryLayerID() const override { return id(); }

    // Reimplementations from GraphicsLayer.h.
    virtual bool setChildren(const Vector<GraphicsLayer*>&) override;
    virtual void addChild(GraphicsLayer*) override;
    virtual void addChildAtIndex(GraphicsLayer*, int) override;
    virtual void addChildAbove(GraphicsLayer*, GraphicsLayer*) override;
    virtual void addChildBelow(GraphicsLayer*, GraphicsLayer*) override;
    virtual bool replaceChild(GraphicsLayer*, GraphicsLayer*) override;
    virtual void removeFromParent() override;
    virtual void setPosition(const FloatPoint&) override;
    virtual void setAnchorPoint(const FloatPoint3D&) override;
    virtual void setSize(const FloatSize&) override;
    virtual void setTransform(const TransformationMatrix&) override;
    virtual void setChildrenTransform(const TransformationMatrix&) override;
    virtual void setPreserves3D(bool) override;
    virtual void setMasksToBounds(bool) override;
    virtual void setDrawsContent(bool) override;
    virtual void setContentsVisible(bool) override;
    virtual void setContentsOpaque(bool) override;
    virtual void setBackfaceVisibility(bool) override;
    virtual void setOpacity(float) override;
    virtual void setContentsRect(const FloatRect&) override;
    virtual void setContentsTilePhase(const FloatSize&) override;
    virtual void setContentsTileSize(const FloatSize&) override;
    virtual void setContentsToImage(Image*) override;
    virtual void setContentsToSolidColor(const Color&) override;
    virtual void setShowDebugBorder(bool) override;
    virtual void setShowRepaintCounter(bool) override;
    virtual bool shouldDirectlyCompositeImage(Image*) const override;
    virtual void setContentsToPlatformLayer(PlatformLayer*, ContentsLayerPurpose) override;
    virtual void setMaskLayer(GraphicsLayer*) override;
    virtual void setReplicatedByLayer(GraphicsLayer*) override;
    virtual void setNeedsDisplay() override;
    virtual void setNeedsDisplayInRect(const FloatRect&, ShouldClipToLayer = ClipToLayer) override;
    virtual void setContentsNeedsDisplay() override;
    virtual void deviceOrPageScaleFactorChanged() override;
    virtual void flushCompositingState(const FloatRect&, bool) override;
    virtual void flushCompositingStateForThisLayerOnly(bool) override;
    virtual bool setFilters(const FilterOperations&) override;
    virtual bool addAnimation(const KeyframeValueList&, const FloatSize&, const Animation*, const String&, double) override;
    virtual void pauseAnimation(const String&, double) override;
    virtual void removeAnimation(const String&) override;
    virtual void suspendAnimations(double time) override;
    virtual void resumeAnimations() override;
    virtual bool usesContentsLayer() const override { return m_platformLayer || m_compositedImage; }

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
    virtual void tiledBackingStorePaint(GraphicsContext&, const IntRect&) override;
    virtual void didUpdateTileBuffers() override;
    virtual void tiledBackingStoreHasPendingTileCreation() override;
    virtual void createTile(uint32_t tileID, float) override;
    virtual void updateTile(uint32_t tileID, const SurfaceUpdateInfo&, const IntRect&) override;
    virtual void removeTile(uint32_t tileID) override;
    virtual bool paintToSurface(const IntSize&, uint32_t& /* atlasID */, IntPoint&, CoordinatedSurface::Client*) override;

    void setCoordinator(CoordinatedGraphicsLayerClient*);

    void setNeedsVisibleRectAdjustment();
    void purgeBackingStores();

    CoordinatedGraphicsLayer* findFirstDescendantWithContentsRecursively();

private:
#if USE(GRAPHICS_SURFACE)
    enum PendingPlatformLayerOperation {
        None = 0x00,
        CreatePlatformLayer = 0x01,
        DestroyPlatformLayer = 0x02,
        SyncPlatformLayer = 0x04,
        CreateAndSyncPlatformLayer = CreatePlatformLayer | SyncPlatformLayer,
        RecreatePlatformLayer = CreateAndSyncPlatformLayer | DestroyPlatformLayer
    };

    void destroyPlatformLayerIfNeeded();
    void createPlatformLayerIfNeeded();
#endif
    void syncPlatformLayer();
#if USE(COORDINATED_GRAPHICS_THREADED)
    void platformLayerWillBeDestroyed() override;
    void setPlatformLayerNeedsDisplay() override;
#endif

    virtual void setDebugBorder(const Color&, float width) override;

    bool fixedToViewport() const { return m_fixedToViewport; }

    void didChangeLayerState();
    void didChangeAnimations();
    void didChangeGeometry();
    void didChangeChildren();
    void didChangeFilters();
    void didChangeImageBacking();

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

    bool notifyFlushRequired();

    // CoordinatedImageBacking::Host
    virtual bool imageBackingVisible() override;
    bool shouldHaveBackingStore() const;
    bool selfOrAncestorHasActiveTransformAnimation() const;
    bool selfOrAncestorHaveNonAffineTransforms();
    void adjustContentsScale();

    void setShouldUpdateVisibleRect();
    float effectiveContentsScale();

    void animationStartedTimerFired();

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
#if USE(GRAPHICS_SURFACE)
    bool m_isValidPlatformLayer : 1;
    unsigned m_pendingPlatformLayerOperation : 3;
#endif
#if USE(COORDINATED_GRAPHICS_THREADED)
    bool m_shouldSyncPlatformLayer : 1;
#endif

    CoordinatedGraphicsLayerClient* m_coordinator;
    std::unique_ptr<TiledBackingStore> m_mainBackingStore;
    std::unique_ptr<TiledBackingStore> m_previousBackingStore;

    RefPtr<Image> m_compositedImage;
    NativeImagePtr m_compositedNativeImagePtr;
    RefPtr<CoordinatedImageBacking> m_coordinatedImageBacking;

    PlatformLayer* m_platformLayer;
#if USE(GRAPHICS_SURFACE)
    IntSize m_platformLayerSize;
    GraphicsSurfaceToken m_platformLayerToken;
#endif
    Timer m_animationStartedTimer;
    TextureMapperAnimations m_animations;
    double m_lastAnimationStartTime;

    ScrollableArea* m_scrollableArea;
};

CoordinatedGraphicsLayer* toCoordinatedGraphicsLayer(GraphicsLayer*);

} // namespace WebCore
#endif // USE(COORDINATED_GRAPHICS)

#endif // CoordinatedGraphicsLayer_h
