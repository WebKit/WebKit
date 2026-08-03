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
#include "CoordinatedCompositionReason.h"
#include "Damage.h"
#include "FloatPoint.h"
#include "FloatPoint3D.h"
#include "FloatSize.h"
#include "PlatformLayerIdentifier.h"
#include "TextureMapperAnimation.h"
#include "TransformationMatrix.h"
#include <wtf/EnumSet.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>

#if USE(SKIA)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/gpu/ganesh/GrContextThreadSafeProxy.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace WebCore {
class CoordinatedAnimatedBackingStoreClient;
class CoordinatedBackingStore;
class CoordinatedBackingStoreProxy;
class CoordinatedImageBackingStore;
class CoordinatedPlatformLayer;
class CoordinatedPlatformLayerBuffer;
class CoordinatedTileBuffer;
class GraphicsLayerCoordinated;
class NativeImage;
class TextureMapperLayer;

#if USE(SKIA)
class SkiaCompositingLayer;
class SkiaPaintingEngine;
class SkiaRecordingResult;
#endif
#if USE(CAIRO)
namespace Cairo {
class PaintingEngine;
}
#endif

class CoordinatedPlatformLayer : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<CoordinatedPlatformLayer> {
public:
    // FIXME: remove this client when a subclass is added for the WebProcess.
    class Client {
    public:
        virtual void attachLayer(CoordinatedPlatformLayer&) = 0;
        virtual void detachLayer(CoordinatedPlatformLayer&) = 0;
#if USE(CAIRO)
        virtual Cairo::PaintingEngine& paintingEngine() = 0;
#elif USE(SKIA)
        virtual SkiaPaintingEngine& paintingEngine() const = 0;
#endif
        virtual Ref<CoordinatedImageBackingStore> imageBackingStore(Ref<NativeImage>&&) = 0;
        virtual void notifyCompositionRequired() = 0;
        virtual bool isCompositionRequiredOrOngoing() const = 0;
        virtual void requestComposition(CompositionReason) = 0;
        virtual RunLoop* compositingRunLoop() const = 0;
        virtual int maxTextureSize() const = 0;
        virtual void willPaintTile() = 0;
        virtual void didPaintTile() = 0;
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

    TextureMapperLayer& ensureTarget();
#if USE(SKIA)
    SkiaCompositingLayer& ensureSkiaTarget();
    sk_sp<GrContextThreadSafeProxy> threadSafeGrContext() const;
#endif
    void invalidateTarget();

#if ENABLE(DAMAGE_TRACKING)
    void setDamagePropagationEnabled(bool enabled) { m_damagePropagationEnabled = enabled; }
    void setDamageInGlobalCoordinateSpace(std::shared_ptr<Damage> damage) { m_damageInGlobalCoordinateSpace = WTF::move(damage); }
#endif

    void setPosition(FloatPoint&&) WTF_REQUIRES_LOCK(m_lock);
    void setPositionForScrolling(const FloatPoint&);
    const FloatPoint& position() const WTF_REQUIRES_LOCK(m_lock);
    void setTopLeftPositionForScrolling(const FloatPoint&);
    FloatPoint topLeftPositionForScrolling();
    void setBoundsOrigin(const FloatPoint&) WTF_REQUIRES_LOCK(m_lock);
    void setBoundsOriginForScrolling(const FloatPoint&);
    const FloatPoint& boundsOrigin() const WTF_REQUIRES_LOCK(m_lock);
    void setAnchorPoint(FloatPoint3D&&) WTF_REQUIRES_LOCK(m_lock);
    const FloatPoint3D& anchorPoint() const WTF_REQUIRES_LOCK(m_lock);
    void setSize(FloatSize&&) WTF_REQUIRES_LOCK(m_lock);
    const FloatSize& size() const WTF_REQUIRES_LOCK(m_lock);
    FloatRect bounds() const WTF_REQUIRES_LOCK(m_lock);

    void setTransform(const TransformationMatrix&) WTF_REQUIRES_LOCK(m_lock);
    const TransformationMatrix& transform() const WTF_REQUIRES_LOCK(m_lock);
    void setChildrenTransform(const TransformationMatrix&) WTF_REQUIRES_LOCK(m_lock);
    const TransformationMatrix& childrenTransform() const WTF_REQUIRES_LOCK(m_lock);
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
    void setPreserves3D(bool) WTF_REQUIRES_LOCK(m_lock);
    void setBackfaceVisibility(bool) WTF_REQUIRES_LOCK(m_lock);
    void setOpacity(float) WTF_REQUIRES_LOCK(m_lock);
    void setBlendMode(BlendMode) WTF_REQUIRES_LOCK(m_lock);

    void setContentsVisible(bool) WTF_REQUIRES_LOCK(m_lock);
    bool contentsVisible() const WTF_REQUIRES_LOCK(m_lock);
    void setContentsOpaque(bool) WTF_REQUIRES_LOCK(m_lock);
    void setContentsRect(const FloatRect&) WTF_REQUIRES_LOCK(m_lock);
    void setContentsRectClipsDescendants(bool) WTF_REQUIRES_LOCK(m_lock);
    void setContentsClippingRect(const FloatRoundedRect&) WTF_REQUIRES_LOCK(m_lock);
    void setContentsScale(float) WTF_REQUIRES_LOCK(m_lock);
    float contentsScale() const WTF_REQUIRES_LOCK(m_lock);
    enum class RequireComposition : bool { No, Yes };
    void setContentsBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&&, std::optional<Damage>&& = std::nullopt, RequireComposition = RequireComposition::Yes) WTF_REQUIRES_LOCK(m_lock);
#if ENABLE(VIDEO) && USE(GSTREAMER)
    void replaceCurrentContentsBufferWithCopy();
#endif
    void setContentsImage(NativeImage*) WTF_REQUIRES_LOCK(m_lock);
    void setContentsColor(const Color&) WTF_REQUIRES_LOCK(m_lock);
    void setContentsTileSize(const FloatSize&) WTF_REQUIRES_LOCK(m_lock);
    void setContentsTilePhase(const FloatSize&) WTF_REQUIRES_LOCK(m_lock);
    void setDirtyRegion(Damage&&) WTF_REQUIRES_LOCK(m_lock);

    void setFilters(const FilterOperations&) WTF_REQUIRES_LOCK(m_lock);
    void setMask(CoordinatedPlatformLayer*) WTF_REQUIRES_LOCK(m_lock);
    CoordinatedPlatformLayer* mask() const WTF_REQUIRES_LOCK(m_lock);
    void setReplica(CoordinatedPlatformLayer*) WTF_REQUIRES_LOCK(m_lock);
    void setBackdrop(CoordinatedPlatformLayer*) WTF_REQUIRES_LOCK(m_lock);
    void notifyBackdropFiltersChanged() WTF_REQUIRES_LOCK(m_lock);
    void setBackdropRect(const FloatRoundedRect&) WTF_REQUIRES_LOCK(m_lock);
    void setIsBackdropRoot(bool) WTF_REQUIRES_LOCK(m_lock);

    void setAnimations(const TextureMapperAnimations&) WTF_REQUIRES_LOCK(m_lock);

    RefPtr<CoordinatedPlatformLayer> parent() const WTF_REQUIRES_LOCK(m_lock);

    void setChildren(Vector<Ref<CoordinatedPlatformLayer>>&&) WTF_REQUIRES_LOCK(m_lock);
    const Vector<Ref<CoordinatedPlatformLayer>>& children() const WTF_REQUIRES_LOCK(m_lock);

    void setEventRegion(const EventRegion&) WTF_REQUIRES_LOCK(m_lock);
    const EventRegion& eventRegion() const WTF_REQUIRES_LOCK(m_lock);

    void setClipPath(const Path&, WindRule) WTF_REQUIRES_LOCK(m_lock);

    void setDebugBorder(Color&&, float) WTF_REQUIRES_LOCK(m_lock);
    void setShowRepaintCounter(bool) WTF_REQUIRES_LOCK(m_lock);

    void updateContents(bool affectedByTransformAnimation) WTF_REQUIRES_LOCK(m_lock);
    void updateBackingStore();

    void flushPendingState();
    void flushPositionChanges(const OptionSet<CompositionReason>&, bool = false);
    void flushCompositingState(const OptionSet<CompositionReason>&, bool = false);

    bool hasPendingTilesCreation() const { assertIsMainThread(); return m_pendingTilesCreation; }
    bool hasPendingBackingStoreTileUpdates() const;
    void processPendingBackingStoreTileUpdates();
    bool isCompositionRequiredOrOngoing() const;
    void requestComposition(CompositionReason);
    RunLoop* compositingRunLoop() const;
    int maxTextureSize() const;

    Ref<CoordinatedTileBuffer> paint(const IntRect&) WTF_REQUIRES_LOCK(m_lock);
#if USE(SKIA)
    Ref<SkiaRecordingResult> record(const IntRect&, unsigned dirtyTilesCount) WTF_REQUIRES_LOCK(m_lock);
    Ref<CoordinatedTileBuffer> replay(Ref<SkiaRecordingResult>&&, const IntRect&, const IntRect&) WTF_REQUIRES_LOCK(m_lock);
#endif
    void willPaintTile();
    void didPaintTile();
    void waitUntilPaintingComplete();

private:
    explicit CoordinatedPlatformLayer(Client*);

    void removeFromParent();

    void notifyCompositionRequired();

    bool needsBackingStore() const;
    void purgeBackingStores();

#if ENABLE(DAMAGE_TRACKING)
    void addDamage(Damage&&) WTF_REQUIRES_LOCK(m_lock);
#endif
    void damageWholeLayer() WTF_REQUIRES_LOCK(m_lock);

    void flushCompositingStateOnTarget(const OptionSet<CompositionReason>&, TextureMapperLayer&);
#if USE(SKIA)
    void flushCompositingStateOnSkiaTarget(const OptionSet<CompositionReason>&, SkiaCompositingLayer&);
#endif

    enum class Change : uint8_t {
        AnchorPoint,
        Animations,
        Backdrop,
        BackdropRect,
        BackdropRoot,
        BackfaceVisibility,
        BackingStore,
        BlendMode,
        BoundsOrigin,
        Children,
        ChildrenTransform,
        ClipPath,
        ContentsBuffer,
        ContentsClippingRect,
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

    // FIXME: remove the client when a subclass is added for the WebProcess.
    Client* m_client { nullptr };

    const PlatformLayerIdentifier m_id;

    GraphicsLayerCoordinated* m_owner WTF_GUARDED_BY_CAPABILITY(mainThread) { nullptr };
    std::unique_ptr<TextureMapperLayer> m_target;
#if USE(SKIA)
    RefPtr<SkiaCompositingLayer> m_skiaTarget;
#endif

    // Accessed only from the main thread.
    bool m_drawsContent WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    bool m_pendingTilesCreation WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    bool m_needsTilesUpdate WTF_GUARDED_BY_CAPABILITY(mainThread) { false };
    FloatRect m_visibleRect WTF_GUARDED_BY_CAPABILITY(mainThread);
    IntRect m_transformedVisibleRect WTF_GUARDED_BY_CAPABILITY(mainThread);
    Vector<IntRect, 1> m_dirtyRegion WTF_GUARDED_BY_CAPABILITY(mainThread);

#if ENABLE(DAMAGE_TRACKING)
    bool m_damagePropagationEnabled { false };
    std::shared_ptr<Damage> m_damageInGlobalCoordinateSpace;
#endif

    mutable Lock m_lock;
    EnumSet<Change> m_pendingChanges WTF_GUARDED_BY_LOCK(m_lock);
    FloatPoint m_position WTF_GUARDED_BY_LOCK(m_lock);
    FloatPoint3D m_anchorPoint WTF_GUARDED_BY_LOCK(m_lock) { 0.5f, 0.5f, 0 };
    FloatSize m_size WTF_GUARDED_BY_LOCK(m_lock);
    FloatPoint m_boundsOrigin WTF_GUARDED_BY_LOCK(m_lock);
    TransformationMatrix m_transform WTF_GUARDED_BY_LOCK(m_lock);
    TransformationMatrix m_childrenTransform WTF_GUARDED_BY_LOCK(m_lock);
    bool m_masksToBounds WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_preserves3D WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_backfaceVisibility WTF_GUARDED_BY_LOCK(m_lock) { true };
    float m_opacity WTF_GUARDED_BY_LOCK(m_lock) { 1. };
    BlendMode m_blendMode WTF_GUARDED_BY_LOCK(m_lock) { BlendMode::Normal };
    bool m_contentsVisible WTF_GUARDED_BY_LOCK(m_lock) { true };
    bool m_contentsOpaque WTF_GUARDED_BY_LOCK(m_lock) { false };
    FloatRect m_contentsRect WTF_GUARDED_BY_LOCK(m_lock);
    bool m_contentsRectClipsDescendants WTF_GUARDED_BY_LOCK(m_lock) { false };
    FloatRoundedRect m_contentsClippingRect WTF_GUARDED_BY_LOCK(m_lock);
    Color m_contentsColor WTF_GUARDED_BY_LOCK(m_lock);
    FloatSize m_contentsTileSize WTF_GUARDED_BY_LOCK(m_lock);
    FloatSize m_contentsTilePhase WTF_GUARDED_BY_LOCK(m_lock);
    float m_contentsScale WTF_GUARDED_BY_LOCK(m_lock) { 1. };
    RefPtr<CoordinatedBackingStoreProxy> m_backingStoreProxy WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<CoordinatedBackingStore> m_backingStore WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<CoordinatedAnimatedBackingStoreClient> m_animatedBackingStoreClient WTF_GUARDED_BY_LOCK(m_lock);
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
    RefPtr<CoordinatedPlatformLayer> m_mask WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<CoordinatedPlatformLayer> m_replica WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<CoordinatedPlatformLayer> m_backdrop WTF_GUARDED_BY_LOCK(m_lock);
    FloatRoundedRect m_backdropRect WTF_GUARDED_BY_LOCK(m_lock);
    bool m_isBackdropRoot WTF_GUARDED_BY_LOCK(m_lock) { false };
    TextureMapperAnimations m_animations WTF_GUARDED_BY_LOCK(m_lock);
    ThreadSafeWeakPtr<CoordinatedPlatformLayer> m_parent WTF_GUARDED_BY_LOCK(m_lock);
    Vector<Ref<CoordinatedPlatformLayer>> m_children WTF_GUARDED_BY_LOCK(m_lock);
    EventRegion m_eventRegion WTF_GUARDED_BY_LOCK(m_lock);
    Color m_debugBorderColor WTF_GUARDED_BY_LOCK(m_lock);
    float m_debugBorderWidth WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    int m_repaintCount WTF_GUARDED_BY_LOCK(m_lock) { -1 };
#if ENABLE(DAMAGE_TRACKING)
    std::optional<Damage> m_damage WTF_GUARDED_BY_LOCK(m_lock);
#endif
#if ENABLE(SCROLLING_THREAD)
    Markable<ScrollingNodeID> m_scrollingNodeID WTF_GUARDED_BY_LOCK(m_lock);
#endif

    struct {
        std::optional<FloatPoint> position;
        std::optional<FloatPoint> positionForScrolling;
        std::optional<FloatPoint> boundsOrigin;
        std::optional<FloatPoint> boundsOriginForScrolling;
    } m_pendingState WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
