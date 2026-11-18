/*
 * Copyright (C) 2024 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedBackingStoreProxy.h"
#include "CoordinatedCompositionReason.h"
#include "Damage.h"
#include "FloatPoint.h"
#include "FloatPoint3D.h"
#include "FloatSize.h"
#include "PlatformLayerIdentifier.h"
#include "TransformationMatrix.h"
#include <wtf/Deque.h>
#include <wtf/EnumSet.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>

#if USE(TEXTURE_MAPPER)
#include "TextureMapperAnimation.h"
#else
#include "AcceleratedAnimations.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/gpu/ganesh/GrContextThreadSafeProxy.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

#if USE(TEXTURE_MAPPER)
#define REQUIRE_LOCK_OR_MAIN_THREAD WTF_REQUIRES_LOCK(m_lock)
#else
#define REQUIRE_LOCK_OR_MAIN_THREAD WTF_REQUIRES_CAPABILITY(mainThread)
#endif

namespace WebCore {
class CoordinatedBackingStore;
class CoordinatedImageBackingStore;
class CoordinatedPlatformLayer;
class CoordinatedPlatformLayerBuffer;
class CoordinatedTileBuffer;
class CoordinatedTileManager;
class GraphicsLayerCoordinated;
class NativeImage;

#if USE(TEXTURE_MAPPER)
class TextureMapperLayer;
#else
class SkiaCompositingLayer;
#endif

class CoordinatedPlatformLayer : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<CoordinatedPlatformLayer> {
public:
    // FIXME: remove this client when a subclass is added for the WebProcess.
    class Client {
    public:
        virtual void attachLayer(CoordinatedPlatformLayer&) = 0;
        virtual void detachLayer(CoordinatedPlatformLayer&) = 0;
        virtual CoordinatedTileManager& tileManager() const = 0;
        virtual Ref<CoordinatedImageBackingStore> imageBackingStore(Ref<NativeImage>&&) = 0;
        virtual void notifyCompositionRequired() = 0;
        virtual bool isCompositionRequiredOrOngoing() const = 0;
        virtual void requestComposition(CompositionReason) = 0;
        virtual void didChangeLayerPendingScrollingPosition(CoordinatedPlatformLayer&) = 0;
        virtual void didApplyLayerScrollingPositions() = 0;
        virtual RunLoop* compositingRunLoop() const = 0;
        virtual int maxTextureSize() const = 0;
        virtual void willPaintTile() = 0;
        virtual void didPaintTile() = 0;
#if USE(SKIA)
        virtual void didPaintPreRenderedTile(CoordinatedPlatformLayer&) = 0;
#endif
    };

    static Ref<CoordinatedPlatformLayer> create();
    static Ref<CoordinatedPlatformLayer> create(Client&);

    virtual ~CoordinatedPlatformLayer();

    PlatformLayerIdentifier id() const { return m_id; }
    Lock& lock() LIFETIME_BOUND WTF_RETURNS_LOCK(m_lock) { return m_lock; }

    Client& client() const { ASSERT(m_client); return *m_client; }
    void invalidateClient();

    void setOwner(GraphicsLayerCoordinated*);
    GraphicsLayerCoordinated* owner() const;

    CoordinatedTileManager& tileManager() const;

#if USE(TEXTURE_MAPPER)
    TextureMapperLayer& ensureTarget();
#else
    SkiaCompositingLayer& ensureTarget();
    sk_sp<GrContextThreadSafeProxy> threadSafeGrContext() const { return m_threadSafeGrContext; }
#endif
    void invalidateTarget();

#if ENABLE(DAMAGE_TRACKING)
    void setDamagePropagationEnabled(bool enabled) { m_damagePropagationEnabled = enabled; }
    void setDamageInGlobalCoordinateSpace(std::shared_ptr<Damage> damage) { m_damageInGlobalCoordinateSpace = WTF::move(damage); }
#endif

    void setPosition(const FloatPoint&) WTF_REQUIRES_LOCK(m_lock);
    void setPositionForScrolling(const FloatPoint&);
    FloatPoint effectivePositionForScrolling() const WTF_REQUIRES_LOCK(m_lock);
    void setTopLeftPositionForScrolling(const FloatPoint&);
    FloatPoint topLeftPositionForScrolling();
    void setBoundsOrigin(const FloatPoint&) WTF_REQUIRES_LOCK(m_lock);
    void setBoundsOriginForScrolling(const FloatPoint&);
    FloatPoint effectiveBoundsOriginForScrolling() const WTF_REQUIRES_LOCK(m_lock);
    void setAnchorPoint(const FloatPoint3D&) WTF_REQUIRES_LOCK(m_lock);
    const FloatPoint3D& anchorPoint() const WTF_REQUIRES_LOCK(m_lock);
    void setSize(const FloatSize&) WTF_REQUIRES_LOCK(m_lock);
    const FloatSize& size() const WTF_REQUIRES_LOCK(m_lock);
    FloatRect bounds() const WTF_REQUIRES_LOCK(m_lock);

    void setTransform(const TransformationMatrix&) WTF_REQUIRES_LOCK(m_lock);
    const TransformationMatrix& transform() const WTF_REQUIRES_LOCK(m_lock);
    void setChildrenTransform(const TransformationMatrix&) REQUIRE_LOCK_OR_MAIN_THREAD;
    void didUpdateLayerTransform();

    void setVisibleRect(const FloatRect&);
    void setTransformedVisibleRect(IntRect&&);

#if ENABLE(SCROLLING_THREAD)
    void setScrollingNodeID(std::optional<ScrollingNodeID>) WTF_REQUIRES_LOCK(m_lock);
    const Markable<ScrollingNodeID>& scrollingNodeID() const WTF_REQUIRES_LOCK(m_lock);
#endif

    void setDrawsContent(bool);
    void setMasksToBounds(bool) WTF_REQUIRES_LOCK(m_lock);
    bool masksToBounds() const WTF_REQUIRES_LOCK(m_lock);
    void setPreserves3D(bool) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setBackfaceVisibility(bool) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setBackgroundColor(const Color&) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setOpacity(float) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setBlendMode(BlendMode) REQUIRE_LOCK_OR_MAIN_THREAD;

    void setContentsVisible(bool) REQUIRE_LOCK_OR_MAIN_THREAD;
#if USE(TEXTURE_MAPPER)
    bool contentsVisible() const WTF_REQUIRES_LOCK(m_lock);
#endif
    void setContentsOpaque(bool) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setContentsRect(const FloatRect&) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setContentsRectClipsDescendants(bool) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setContentsClippingRect(const FloatRoundedRect&) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setContentsClipShapePath(const Path&) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setContentsScale(float) REQUIRE_LOCK_OR_MAIN_THREAD;
#if USE(TEXTURE_MAPPER)
    float contentsScale() const WTF_REQUIRES_LOCK(m_lock);
#else
    float contentsScale() const;
#endif
    void setContentsBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&&, std::optional<Damage>&& = std::nullopt) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setScrollbarContentsBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&&, const FloatRect&);
    void clearScrollbarContentsBuffer();
    enum class RequireComposition : bool { No, Yes };
    void setVideoContentsBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&&, RequireComposition);
    void clearVideoContentsBuffer();
    void discardVideoContentsBuffer();
#if ENABLE(VIDEO) && USE(GSTREAMER_GL)
    void replaceCurrentContentsBufferWithCopy();
#endif
    void setContentsImage(NativeImage*) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setContentsColor(const Color&) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setContentsTileSize(const FloatSize&) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setContentsTilePhase(const FloatSize&) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setDirtyRegion(Damage&&) REQUIRE_LOCK_OR_MAIN_THREAD;

    void setFilters(const FilterOperations&) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setMask(CoordinatedPlatformLayer*) WTF_REQUIRES_LOCK(m_lock);
    CoordinatedPlatformLayer* mask() const WTF_REQUIRES_LOCK(m_lock);
    void setReplica(CoordinatedPlatformLayer*) REQUIRE_LOCK_OR_MAIN_THREAD;
#if USE(TEXTURE_MAPPER)
    void setBackdrop(CoordinatedPlatformLayer*) WTF_REQUIRES_LOCK(m_lock);
    void notifyBackdropFiltersChanged() WTF_REQUIRES_LOCK(m_lock);
#else
    void setBackdropFilters(const FilterOperations&) REQUIRE_LOCK_OR_MAIN_THREAD;
#endif
    void setBackdropRect(const FloatRoundedRect&) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setBackdropShapePath(const Path&) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setIsBackdropRoot(bool) REQUIRE_LOCK_OR_MAIN_THREAD;

#if USE(TEXTURE_MAPPER)
    void setAnimations(const TextureMapperAnimations&) WTF_REQUIRES_LOCK(m_lock);
#else
    void setAnimations(const AcceleratedAnimations&) REQUIRE_LOCK_OR_MAIN_THREAD;
#endif

    RefPtr<CoordinatedPlatformLayer> parent() const WTF_REQUIRES_LOCK(m_lock);

    void setChildren(Vector<Ref<CoordinatedPlatformLayer>>&&) WTF_REQUIRES_LOCK(m_lock);
    const Vector<Ref<CoordinatedPlatformLayer>>& children() const WTF_REQUIRES_LOCK(m_lock);

    void setEventRegion(const EventRegion&) WTF_REQUIRES_LOCK(m_lock);
    const EventRegion& eventRegion() const WTF_REQUIRES_LOCK(m_lock);

    void setClipPath(const Path&, WindRule) REQUIRE_LOCK_OR_MAIN_THREAD;

    void setDebugBorder(Color&&, float) REQUIRE_LOCK_OR_MAIN_THREAD;
    void setShowRepaintCounter(bool) REQUIRE_LOCK_OR_MAIN_THREAD;

    void updateContents(bool affectedByTransformAnimation) REQUIRE_LOCK_OR_MAIN_THREAD;
    void updateBackingStore();

    void commitPendingScrollingPosition();
    void flushScrollingPositionChanges(const OptionSet<CompositionReason>&);

#if !USE(TEXTURE_MAPPER)
    bool commitPendingRenderingUpdateProperties();
#endif

    void flushCompositingState(const OptionSet<CompositionReason>&);

#if USE(CAIRO)
    bool hasPendingTilesCreation() const { assertIsMainThread(); return m_pendingTilesCreation; }
#endif
    bool hasPendingBackingStoreTileUpdates() const;
    void processPendingBackingStoreTileUpdates();
#if USE(SKIA)
    bool processPendingBackingStoreTileUpdatesIncludingPreRendered(const Function<bool()>& shouldStopProcessingTiles);
#endif
    bool isCompositionRequiredOrOngoing() const;
    void requestComposition(CompositionReason);
    void didApplyLayerScrollingPositions();
    RunLoop* compositingRunLoop() const;
    int maxTextureSize() const;

    void willPaintTile();
    void didPaintTile();
    void didPaintPreRenderedTile();
    void waitUntilPaintingFinished();

private:
    explicit CoordinatedPlatformLayer(Client*);

    void removeFromParent();

    void notifyCompositionRequired();
    void notifyScrollingPositionChanged();

    bool needsBackingStore() const;
    void purgeBackingStores();

#if ENABLE(DAMAGE_TRACKING)
    void addDamage(Damage&&) WTF_REQUIRES_CAPABILITY(mainThread);
#endif
    void damageWholeLayer() REQUIRE_LOCK_OR_MAIN_THREAD;

#if USE(TEXTURE_MAPPER)
    void flushCompositingStateOnTarget(const OptionSet<CompositionReason>&, TextureMapperLayer&);
#else
    const FloatSize& sizeOnMainThread() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS { assertIsMainThread(); return m_size; }

    void flushCompositingStateProperties(const OptionSet<CompositionReason>&);
    void flushCompositingStateScrollbarContents(const OptionSet<CompositionReason>&);
    void flushCompositingStateVideoContents(const OptionSet<CompositionReason>&);
#endif

    enum class Change : uint8_t {
        AnchorPoint,
        Animations,
        Backdrop,
        BackdropRect,
        BackdropRoot,
        BackdropShapePath,
        BackfaceVisibility,
        BackgroundColor,
        BackingStore,
        BackingStoreTiles,
        BlendMode,
        BoundsOrigin,
        Children,
        ChildrenTransform,
        ClipPath,
        ContentsBuffer,
        ContentsClippingRect,
        ContentsClipShapePath,
        ContentsColor,
        ContentsImage,
        ContentsOpaque,
        ContentsRect,
        ContentsRectClipsDescendants,
        ContentsTiling,
        ContentsVisible,
#if ENABLE(DAMAGE_TRACKING)
        Damage,
#endif
        DebugIndicators,
        Filters,
        Mask,
        MasksToBounds,
        Opacity,
        Position,
        Preserves3D,
        Replica,
        Size,
        Transform,
    };

#if !USE(TEXTURE_MAPPER)
    void addPendingChange(Change);

    struct RenderingUpdateProperties {
        RenderingUpdateProperties() = default;
        RenderingUpdateProperties(RenderingUpdateProperties&&) = default;
        RenderingUpdateProperties& operator=(RenderingUpdateProperties&&) = default;
        RenderingUpdateProperties(const RenderingUpdateProperties&) = delete;
        RenderingUpdateProperties& operator=(const RenderingUpdateProperties&) = delete;
        ~RenderingUpdateProperties();

        EnumSet<Change> changes;
        FloatPoint position;
        FloatPoint boundsOrigin;
        FloatPoint3D anchorPoint { 0.5f, 0.5f, 0 };
        FloatSize size;
        TransformationMatrix transform;
        TransformationMatrix childrenTransform;
        bool masksToBounds { false };
        bool preserves3D { false };
        bool backfaceVisibility { true };
        float opacity { 1. };
        BlendMode blendMode { BlendMode::Normal };
        bool contentsVisible { true };
        bool contentsOpaque  { false };
        FloatRect contentsRect;
        bool contentsRectClipsDescendants { false };
        FloatRoundedRect contentsClippingRect;
        Path contentsClipShapePath;
        Color backgroundColor;
        Color contentsColor;
        FloatSize contentsTileSize;
        FloatSize contentsTilePhase;
        RefPtr<CoordinatedImageBackingStore> imageBackingStore;
        std::unique_ptr<CoordinatedPlatformLayerBuffer> contentsBuffer;
        bool hasBackingStore { false };
        RefPtr<CoordinatedAnimatedBackingStoreClient> animatedBackingStoreClient;
        std::optional<CoordinatedBackingStoreProxy::Update> tilesUpdate;
        float contentsScale { 1. };
        Path clipPath;
        WindRule clipPathWindRule { WindRule::NonZero };
        FilterOperations filters;
        RefPtr<CoordinatedPlatformLayer> mask;
        RefPtr<CoordinatedPlatformLayer> replica;
        FilterOperations backdropFilters;
        FloatRoundedRect backdropRect;
        Path backdropShapePath;
        bool isBackdropRoot { false };
        AcceleratedAnimations animations;
        Vector<Ref<CoordinatedPlatformLayer>> children;
        Color debugBorderColor;
        float debugBorderWidth { 0 };
        int repaintCount { -1 };
#if ENABLE(DAMAGE_TRACKING)
        std::optional<Damage> damage;
#endif
    };
#endif

    // FIXME: remove the client when a subclass is added for the WebProcess.
    Client* m_client { nullptr };

    const PlatformLayerIdentifier m_id;

    GraphicsLayerCoordinated* m_owner WTF_GUARDED_BY_CAPABILITY(mainThread) { nullptr };
#if USE(TEXTURE_MAPPER)
    std::unique_ptr<TextureMapperLayer> m_target;
#else
    RefPtr<SkiaCompositingLayer> m_target;
    sk_sp<GrContextThreadSafeProxy> m_threadSafeGrContext;
#endif

    // Accessed only from the main thread.
    bool m_drawsContent WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
#if USE(CAIRO)
    bool m_pendingTilesCreation WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
#endif
    bool m_needsTilesUpdate WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    FloatRect m_visibleRect WTF_GUARDED_BY_CAPABILITY(mainThread);
    IntRect m_transformedVisibleRect WTF_GUARDED_BY_CAPABILITY(mainThread);
    Vector<IntRect, 1> m_dirtyRegion WTF_GUARDED_BY_CAPABILITY(mainThread);
#if !USE(TEXTURE_MAPPER)
    EnumSet<Change> m_pendingChanges WTF_GUARDED_BY_CAPABILITY(mainThread);
    TransformationMatrix m_childrenTransform WTF_GUARDED_BY_CAPABILITY(mainThread);
    bool m_preserves3D WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    bool m_backfaceVisibility WTF_GUARDED_BY_CAPABILITY(mainThread) { true };
    float m_opacity WTF_GUARDED_BY_CAPABILITY(mainThread) { 1. };
    BlendMode m_blendMode WTF_GUARDED_BY_CAPABILITY(mainThread) { BlendMode::Normal };
    bool m_contentsVisible WTF_GUARDED_BY_CAPABILITY(mainThread) { true };
    bool m_contentsOpaque WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    FloatRect m_contentsRect WTF_GUARDED_BY_CAPABILITY(mainThread);
    bool m_contentsRectClipsDescendants WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    FloatRoundedRect m_contentsClippingRect WTF_GUARDED_BY_CAPABILITY(mainThread);
    Path m_contentsClipShapePath WTF_GUARDED_BY_CAPABILITY(mainThread);
    Color m_backgroundColor WTF_GUARDED_BY_CAPABILITY(mainThread);
    Color m_contentsColor WTF_GUARDED_BY_CAPABILITY(mainThread);
    FloatSize m_contentsTileSize WTF_GUARDED_BY_CAPABILITY(mainThread);
    FloatSize m_contentsTilePhase WTF_GUARDED_BY_CAPABILITY(mainThread);
    RefPtr<CoordinatedBackingStoreProxy> m_backingStoreProxy WTF_GUARDED_BY_CAPABILITY(mainThread);
    std::optional<CoordinatedBackingStoreProxy::Update> m_tilesUpdate WTF_GUARDED_BY_CAPABILITY(mainThread);
    RefPtr<CoordinatedImageBackingStore> m_imageBackingStore WTF_GUARDED_BY_CAPABILITY(mainThread);
    std::unique_ptr<CoordinatedPlatformLayerBuffer> m_contentsBuffer WTF_GUARDED_BY_CAPABILITY(mainThread);
    struct {
        Path path;
        WindRule windRule { WindRule::NonZero };
    } m_clipPath WTF_GUARDED_BY_CAPABILITY(mainThread);
    FilterOperations m_filters WTF_GUARDED_BY_CAPABILITY(mainThread);
    RefPtr<CoordinatedPlatformLayer> m_replica WTF_GUARDED_BY_CAPABILITY(mainThread);
    FilterOperations m_backdropFilters WTF_GUARDED_BY_CAPABILITY(mainThread);
    FloatRoundedRect m_backdropRect WTF_GUARDED_BY_CAPABILITY(mainThread);
    Path m_backdropShapePath WTF_GUARDED_BY_CAPABILITY(mainThread);
    bool m_isBackdropRoot WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    AcceleratedAnimations m_animations WTF_GUARDED_BY_CAPABILITY(mainThread);
    Color m_debugBorderColor WTF_GUARDED_BY_CAPABILITY(mainThread);
    float m_debugBorderWidth WTF_GUARDED_BY_CAPABILITY(mainThread) { 0 };
    int m_repaintCount WTF_GUARDED_BY_CAPABILITY(mainThread) { -1 };
#if ENABLE(DAMAGE_TRACKING)
    std::optional<Damage> m_damage WTF_GUARDED_BY_CAPABILITY(mainThread);
#endif
#endif

#if ENABLE(DAMAGE_TRACKING)
    bool m_damagePropagationEnabled { false };
    std::shared_ptr<Damage> m_damageInGlobalCoordinateSpace;
#endif

    mutable Lock m_lock;
    FloatPoint m_position WTF_GUARDED_BY_LOCK(m_lock);
    FloatPoint3D m_anchorPoint WTF_GUARDED_BY_LOCK(m_lock) { 0.5f, 0.5f, 0 };
    FloatSize m_size WTF_GUARDED_BY_LOCK(m_lock);
    FloatPoint m_boundsOrigin WTF_GUARDED_BY_LOCK(m_lock);
    TransformationMatrix m_transform WTF_GUARDED_BY_LOCK(m_lock);
    EventRegion m_eventRegion WTF_GUARDED_BY_LOCK(m_lock);
    bool m_masksToBounds WTF_GUARDED_BY_LOCK(m_lock) { false };
    RefPtr<CoordinatedPlatformLayer> m_mask WTF_GUARDED_BY_LOCK(m_lock);
    ThreadSafeWeakPtr<CoordinatedPlatformLayer> m_parent WTF_GUARDED_BY_LOCK(m_lock);
    Vector<Ref<CoordinatedPlatformLayer>> m_children WTF_GUARDED_BY_LOCK(m_lock);
#if ENABLE(SCROLLING_THREAD)
    Markable<ScrollingNodeID> m_scrollingNodeID WTF_GUARDED_BY_LOCK(m_lock);
#endif
#if USE(TEXTURE_MAPPER)
    EnumSet<Change> m_pendingChanges WTF_GUARDED_BY_LOCK(m_lock);
    TransformationMatrix m_childrenTransform WTF_GUARDED_BY_LOCK(m_lock);
    bool m_preserves3D WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_backfaceVisibility WTF_GUARDED_BY_LOCK(m_lock) { true };
    float m_opacity WTF_GUARDED_BY_LOCK(m_lock) { 1. };
    BlendMode m_blendMode WTF_GUARDED_BY_LOCK(m_lock) { BlendMode::Normal };
    bool m_contentsVisible WTF_GUARDED_BY_LOCK(m_lock) { true };
    bool m_contentsOpaque WTF_GUARDED_BY_LOCK(m_lock) { false };
    FloatRect m_contentsRect WTF_GUARDED_BY_LOCK(m_lock);
    bool m_contentsRectClipsDescendants WTF_GUARDED_BY_LOCK(m_lock) { false };
    FloatRoundedRect m_contentsClippingRect WTF_GUARDED_BY_LOCK(m_lock);
    Path m_contentsClipShapePath WTF_GUARDED_BY_LOCK(m_lock);
    Color m_backgroundColor WTF_GUARDED_BY_LOCK(m_lock);
    Color m_contentsColor WTF_GUARDED_BY_LOCK(m_lock);
    FloatSize m_contentsTileSize WTF_GUARDED_BY_LOCK(m_lock);
    FloatSize m_contentsTilePhase WTF_GUARDED_BY_LOCK(m_lock);
    float m_contentsScale WTF_GUARDED_BY_LOCK(m_lock) { 1. };
    RefPtr<CoordinatedBackingStoreProxy> m_backingStoreProxy WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<CoordinatedBackingStore> m_backingStore WTF_GUARDED_BY_LOCK(m_lock);
    struct {
        RefPtr<CoordinatedImageBackingStore> current;
        RefPtr<CoordinatedImageBackingStore> committed;
    } m_imageBackingStore WTF_GUARDED_BY_LOCK(m_lock);
    struct {
        std::unique_ptr<CoordinatedPlatformLayerBuffer> pending;
        std::unique_ptr<CoordinatedPlatformLayerBuffer> committed;
        bool hasCommitted { false };
    } m_contentsBuffer WTF_GUARDED_BY_LOCK(m_lock);
    struct {
        Path path;
        WindRule windRule;
    } m_clipPath WTF_GUARDED_BY_LOCK(m_lock);
    FilterOperations m_filters WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<CoordinatedPlatformLayer> m_replica WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<CoordinatedPlatformLayer> m_backdrop WTF_GUARDED_BY_LOCK(m_lock);
    FloatRoundedRect m_backdropRect WTF_GUARDED_BY_LOCK(m_lock);
    Path m_backdropShapePath WTF_GUARDED_BY_LOCK(m_lock);
    bool m_isBackdropRoot WTF_GUARDED_BY_LOCK(m_lock) { false };
    TextureMapperAnimations m_animations WTF_GUARDED_BY_LOCK(m_lock);
    Color m_debugBorderColor WTF_GUARDED_BY_LOCK(m_lock);
    float m_debugBorderWidth WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    int m_repaintCount WTF_GUARDED_BY_LOCK(m_lock) { -1 };
#else
    std::atomic<float> m_contentsScale { 1. };
#endif

    struct {
        Lock lock;
        std::optional<std::unique_ptr<CoordinatedPlatformLayerBuffer>> buffer WTF_GUARDED_BY_LOCK(lock);
        std::optional<FloatRect> rect WTF_GUARDED_BY_LOCK(lock);
    } m_scrollbarContents;
    struct {
        Lock lock;
        std::optional<std::unique_ptr<CoordinatedPlatformLayerBuffer>> buffer WTF_GUARDED_BY_LOCK(lock);
    } m_videoContents;

    struct {
        mutable Lock lock;
        std::optional<FloatPoint> pendingPosition WTF_GUARDED_BY_LOCK(lock);
        std::optional<FloatPoint> committedPosition WTF_GUARDED_BY_LOCK(lock);
        std::optional<FloatPoint> currentPosition WTF_GUARDED_BY_LOCK(lock);
        std::optional<FloatPoint> pendingBoundsOrigin WTF_GUARDED_BY_LOCK(lock);
        std::optional<FloatPoint> committedBoundsOrigin WTF_GUARDED_BY_LOCK(lock);
        std::optional<FloatPoint> currentBoundsOrigin WTF_GUARDED_BY_LOCK(lock);
        bool didNotifyPositionChange WTF_GUARDED_BY_LOCK(lock) { false };
    } m_scrollingPosition;

#if !USE(TEXTURE_MAPPER)
    Lock m_committedRenderingUpdatesLock;
    Deque<RenderingUpdateProperties> m_committedRenderingUpdates WTF_GUARDED_BY_LOCK(m_committedRenderingUpdatesLock);
#endif
};

} // namespace WebCore

#undef REQUIRE_LOCK_OR_MAIN_THREAD

#endif // USE(COORDINATED_GRAPHICS)
