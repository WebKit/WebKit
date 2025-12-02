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
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>

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
class TextureMapper;
class TextureMapperLayer;

#if USE(SKIA)
class SkiaPaintingEngine;
class SkiaRecordingResult;
#endif
#if USE(CAIRO)
namespace Cairo {
class PaintingEngine;
}
#endif

class CoordinatedPlatformLayer : public ThreadSafeRefCounted<CoordinatedPlatformLayer> {
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
    Lock& lock() { return m_lock; }

    Client& client() const { ASSERT(m_client); return *m_client; }
    void invalidateClient();

    void setOwner(GraphicsLayerCoordinated*);
    GraphicsLayerCoordinated* owner() const;

    TextureMapperLayer& ensureTarget();
    TextureMapperLayer* target() const;
    void invalidateTarget();

#if ENABLE(DAMAGE_TRACKING)
    void setDamagePropagationEnabled(bool enabled) { m_damagePropagationEnabled = enabled; }
    void setDamageInGlobalCoordinateSpace(std::shared_ptr<Damage> damage) { m_damageInGlobalCoordinateSpace = WTFMove(damage); }
#endif

    void setPosition(FloatPoint&&);
    enum class ForcePositionSync : bool { No, Yes };
    void setPositionForScrolling(const FloatPoint&, ForcePositionSync = ForcePositionSync::No);
    const FloatPoint& position() const;
    void setTopLeftPositionForScrolling(const FloatPoint&, ForcePositionSync = ForcePositionSync::No);
    FloatPoint topLeftPositionForScrolling();
    void setBoundsOrigin(const FloatPoint&);
    void setBoundsOriginForScrolling(const FloatPoint&);
    const FloatPoint& boundsOrigin() const;
    void setAnchorPoint(FloatPoint3D&&);
    const FloatPoint3D& anchorPoint() const;
    void setSize(FloatSize&&);
    const FloatSize& size() const;
    FloatRect bounds() const;

    void setTransform(const TransformationMatrix&);
    const TransformationMatrix& transform() const;
    void setChildrenTransform(const TransformationMatrix&);
    const TransformationMatrix& childrenTransform() const;
    void didUpdateLayerTransform();

    void setVisibleRect(const FloatRect&);
    const FloatRect& visibleRect() const;
    void setTransformedVisibleRect(IntRect&& visibleRect, IntRect&& visibleRectIncludingFuture);

#if ENABLE(SCROLLING_THREAD)
    void setScrollingNodeID(std::optional<ScrollingNodeID>);
    const Markable<ScrollingNodeID>& scrollingNodeID() const;
#endif

    void setDrawsContent(bool);
    void setMasksToBounds(bool);
    void setPreserves3D(bool);
    void setBackfaceVisibility(bool);
    void setOpacity(float);

    void setContentsVisible(bool);
    bool contentsVisible() const;
    void setContentsOpaque(bool);
    void setContentsRect(const FloatRect&);
    void setContentsRectClipsDescendants(bool);
    void setContentsClippingRect(const FloatRoundedRect&);
    void setContentsScale(float);
    enum class RequireComposition : bool { No, Yes };
    void setContentsBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&&, std::optional<Damage>&& = std::nullopt, RequireComposition = RequireComposition::Yes);
#if ENABLE(VIDEO) && USE(GSTREAMER)
    void replaceCurrentContentsBufferWithCopy();
#endif
    void setContentsBufferNeedsDisplay();
    void setContentsImage(NativeImage*);
    void setContentsColor(const Color&);
    void setContentsTileSize(const FloatSize&);
    void setContentsTilePhase(const FloatSize&);
    void setDirtyRegion(Damage&&);

    void setFilters(const FilterOperations&);
    void setMask(CoordinatedPlatformLayer*);
    void setReplica(CoordinatedPlatformLayer*);
    void setBackdrop(CoordinatedPlatformLayer*);
    void setBackdropRect(const FloatRoundedRect&);

    void setAnimations(const TextureMapperAnimations&);

    void setChildren(Vector<Ref<CoordinatedPlatformLayer>>&&);
    const Vector<Ref<CoordinatedPlatformLayer>>& children() const;

    void setEventRegion(const EventRegion&);
    const EventRegion& eventRegion() const;

    void setDebugBorder(Color&&, float);
    void setShowRepaintCounter(bool);

    void updateContents(bool affectedByTransformAnimation);
    void updateBackingStore();

    void flushCompositingState(const OptionSet<CompositionReason>&, TextureMapper&);

    bool hasPendingTilesCreation() const { return m_pendingTilesCreation; }
    bool isCompositionRequiredOrOngoing() const;
    void requestComposition(CompositionReason);
    RunLoop* compositingRunLoop() const;
    int maxTextureSize() const;

    Ref<CoordinatedTileBuffer> paint(const IntRect&);
#if USE(SKIA)
    Ref<SkiaRecordingResult> record(const IntRect&);
    Ref<CoordinatedTileBuffer> replay(const RefPtr<SkiaRecordingResult>&, const IntRect&);
#endif
    void willPaintTile();
    void didPaintTile();
    void waitUntilPaintingComplete();

private:
    explicit CoordinatedPlatformLayer(Client*);

    void notifyCompositionRequired();

    bool needsBackingStore() const;
    void purgeBackingStores();

#if ENABLE(DAMAGE_TRACKING)
    void addDamage(Damage&&);
#endif

    enum class Change : uint32_t {
        Position                     = 1 << 0,
        BoundsOrigin                 = 1 << 1,
        AnchorPoint                  = 1 << 2,
        Size                         = 1 << 3,
        Transform                    = 1 << 4,
        ChildrenTransform            = 1 << 5,
        DrawsContent                 = 1 << 6,
        MasksToBounds                = 1 << 7,
        Preserves3D                  = 1 << 8,
        BackfaceVisibility           = 1 << 9,
        Opacity                      = 1 << 10,
        Children                     = 1 << 11,
        BackingStore                 = 1 << 12,
        ContentsVisible              = 1 << 13,
        ContentsOpaque               = 1 << 14,
        ContentsRect                 = 1 << 15,
        ContentsRectClipsDescendants = 1 << 16,
        ContentsClippingRect         = 1 << 17,
        ContentsTiling               = 1 << 18,
        ContentsBuffer               = 1 << 19,
        ContentsImage                = 1 << 20,
        ContentsColor                = 1 << 21,
        Filters                      = 1 << 22,
        Mask                         = 1 << 23,
        Replica                      = 1 << 24,
        Backdrop                     = 1 << 25,
        BackdropRect                 = 1 << 26,
        Animations                   = 1 << 27,
        DebugIndicators              = 1 << 28,
#if ENABLE(DAMAGE_TRACKING)
        Damage                       = 1 << 29,
#endif
#if ENABLE(SCROLLING_THREAD)
        ScrollingNode                = 1 << 30
#endif
    };

    // FIXME: remove the client when a subclass is added for the WebProcess.
    Client* m_client { nullptr };

    const PlatformLayerIdentifier m_id;

    GraphicsLayerCoordinated* m_owner { nullptr };
    std::unique_ptr<TextureMapperLayer> m_target;
    bool m_pendingTilesCreation { false };
    bool m_needsTilesUpdate { false };

#if ENABLE(DAMAGE_TRACKING)
    bool m_damagePropagationEnabled { false };
    std::shared_ptr<Damage> m_damageInGlobalCoordinateSpace;
#endif

    Lock m_lock;
    OptionSet<Change> m_pendingChanges WTF_GUARDED_BY_LOCK(m_lock);
    FloatPoint m_position WTF_GUARDED_BY_LOCK(m_lock);
    FloatPoint3D m_anchorPoint WTF_GUARDED_BY_LOCK(m_lock) { 0.5f, 0.5f, 0 };
    FloatSize m_size WTF_GUARDED_BY_LOCK(m_lock);
    FloatPoint m_boundsOrigin WTF_GUARDED_BY_LOCK(m_lock);
    TransformationMatrix m_transform WTF_GUARDED_BY_LOCK(m_lock);
    TransformationMatrix m_childrenTransform WTF_GUARDED_BY_LOCK(m_lock);
    FloatRect m_visibleRect WTF_GUARDED_BY_LOCK(m_lock);
    IntRect m_transformedVisibleRect WTF_GUARDED_BY_LOCK(m_lock);
    IntRect m_transformedVisibleRectIncludingFuture WTF_GUARDED_BY_LOCK(m_lock);
    bool m_drawsContent WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_masksToBounds WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_preserves3D WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_backfaceVisibility WTF_GUARDED_BY_LOCK(m_lock) { true };
    float m_opacity WTF_GUARDED_BY_LOCK(m_lock) { 1. };
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
    } m_contentsBuffer WTF_GUARDED_BY_LOCK(m_lock);
    Vector<IntRect, 1> m_dirtyRegion WTF_GUARDED_BY_LOCK(m_lock);
    FilterOperations m_filters WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<CoordinatedPlatformLayer> m_mask WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<CoordinatedPlatformLayer> m_replica WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<CoordinatedPlatformLayer> m_backdrop WTF_GUARDED_BY_LOCK(m_lock);
    FloatRoundedRect m_backdropRect WTF_GUARDED_BY_LOCK(m_lock);
    TextureMapperAnimations m_animations WTF_GUARDED_BY_LOCK(m_lock);
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
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
