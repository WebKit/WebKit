/*
 * Copyright (C) 2026 Igalia S.L.
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

#pragma once

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "BoxExtents.h"
#include "Color.h"
#include "CoordinatedBackingStoreProxy.h"
#include "Damage.h"
#include "FloatPoint.h"
#include "FloatPoint3D.h"
#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "IntSize.h"
#include "SkiaCompositingLayerImageSetBatch.h"
#include "SkiaCompositingLayerOverlapRegions.h"
#include "SkiaDamageRegion.h"
#include "TextureMapperAnimation.h"
#include "TransformationMatrix.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColorFilter.h>
#include <skia/core/SkM44.h>
#include <skia/core/SkPath.h>
#include <skia/effects/SkImageFilters.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/CString.h>

namespace WebCore {
class CoordinatedAnimatedBackingStoreClient;
class CoordinatedImageBackingStore;
class CoordinatedPlatformLayerBuffer;
class FilterOperations;
class SkiaBackingStore;

class SkiaCompositingLayer final : public RefCountedAndCanMakeWeakPtr<SkiaCompositingLayer> {
    WTF_MAKE_TZONE_ALLOCATED(SkiaCompositingLayer);
public:
    static Ref<SkiaCompositingLayer> create();
    ~SkiaCompositingLayer();

    void invalidate();

    void setSize(const FloatSize&);
    void setPosition(const FloatPoint& point) { m_position = point; }
    void setAnchorPoint(const FloatPoint3D& point) { m_anchorPoint = point; }
    void setBoundsOrigin(const FloatPoint& point) { m_boundsOrigin = point; }
    void setTransform(const TransformationMatrix& matrix) { m_transform = matrix; }
    void setChildrenTransform(const TransformationMatrix& matrix) { m_childrenTransform = matrix; }
    void setPreserves3D(bool preserves3D) { m_preserves3D = preserves3D; }
    void setBackfaceVisibility(bool visible) { m_backfaceVisibility = visible; }
    void setContentsVisible(bool visible) { m_contentsVisible = visible; }
    void setContentsOpaque(bool opaque) { m_contentsOpaque = opaque; }
    void setMasksToBounds(bool masksToBounds) { m_masksToBounds = masksToBounds; }
    void setContentsClippingRect(const FloatRoundedRect& rect) { m_contentsClippingRect = rect; }
    void setContentsRectClipsDescendants(bool clips) { m_contentsRectClipsDescendants = clips; }
    void setOpacity(float);
    void setBlendMode(BlendMode);
    void setContentsRect(const FloatRect& rect) { m_contentsRect = rect; }
    void setAnimations(const TextureMapperAnimations& animations) { m_animations = animations; }
    void setContentsTiling(const FloatSize& size, const FloatSize& phase) { m_contentsTiling = { size, phase }; }
    void setClipPath(SkPath&& clipPath) { m_clipPath = WTF::move(clipPath); }
    void setMask(RefPtr<SkiaCompositingLayer>&&);
    void setReplica(RefPtr<SkiaCompositingLayer>&&);
    void setFilters(const FilterOperations&);
    void setBackdropFilters(const FilterOperations&);
    void setBackdropFiltersRect(const FloatRoundedRect&);
    void setIsBackdropRoot(bool isBackdropRoot) { m_isBackdropRoot = isBackdropRoot; }
    void setChildren(Vector<Ref<SkiaCompositingLayer>>&&);

#if ENABLE(DAMAGE_TRACKING)
    void setDamagePropagationEnabled(bool enabled) { m_damagePropagationEnabled = enabled; }
    void addDamage(Damage&&);
#endif

    void setUseBackingStore(bool, CoordinatedAnimatedBackingStoreClient* = nullptr);
    void updateBackingStore(CoordinatedBackingStoreProxy::Update&&, float);
    bool hasPendingBackingStoreTileUpdates() const;
    void processPendingTileUpdates();
    void setImageBackingStore(CoordinatedImageBackingStore*);
    void setContentsBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&&);
    CoordinatedPlatformLayerBuffer* contentsBuffer() const { return m_contentsBuffer.get(); }
    void setContentsSolidColor(const Color&);

    void setDebugIndicators(Color&& debugBorderColor, std::optional<float> debugBorderWidth, std::optional<unsigned> repaintCount);

    const TransformationMatrix& toSurfaceTransform() const { return m_transforms.combined; }
    FloatRect effectiveLayerRect() const { return FloatRect({ }, m_size); }

    // Applies the animations, computes the transforms, then walks the tree. When a frame damage is passed,
    // it is collected first in a walk that draws nothing, before the walk that draws. The draw is limited
    // to the region the target must redraw - the target's prior owed damage combined with this frame's - and
    // no prior damage repaints the whole target. Returns whether any animation is still running.
    bool paint(SkCanvas&, std::optional<Damage>& frameDamage, const std::optional<Damage>& priorTargetDamage = std::nullopt, std::optional<SkColor> clearColor = std::nullopt);

private:
    using ScopedFlush = SkiaCompositingLayerImageSetBatch::ScopedFlush;

    SkiaCompositingLayer() = default;

    void removeFromParent();
    bool isVisible() const;
    bool isLeafOf3DRenderingContext() const { return !m_preserves3D && (m_parent && m_parent->m_preserves3D); }
    bool isReplica() const { return !!m_replicatedLayer; }
    // Contents are painted into m_contentsRect, which the layer bounds do not have to contain.
    bool paintsContentsRect() const { return m_contentsBuffer || m_imageBackingStore || (m_contentsSolidColor.isValid() && m_contentsSolidColor.isVisible()); }
    bool hasVisualContent() const { return m_backingStore || paintsContentsRect(); }
    bool hasVisiblePaintableContent() const { return !m_size.isEmpty() && m_visible && m_contentsVisible && hasVisualContent(); }

    // A backdrop filter paints the layer without any content of its own, so it contributes damage too.
    bool contributesToFrame() const { return hasVisiblePaintableContent() || (!m_size.isEmpty() && m_visible && m_contentsVisible && !!m_backdrop.filter); }

    // What the layer actually paints, unlike effectiveLayerRect(), which the contents rect and the
    // backdrop rect may both overhang, and which a filter such as a blur may paint outside of.
    FloatRect paintedLayerRect() const;
    Ref<SkiaCompositingLayer> backdropRoot();

    bool computeTransformsAndAnimations(const TransformationMatrix& parentTransform, const TransformationMatrix& futureParentTransform, MonotonicTime);

#if ENABLE(DAMAGE_TRACKING)
    // Where every layer that has painted last frame and where it is painting now, keyed by an identifier
    // given out on a layer's first paint. The root holds this rather than the layer itself, because the
    // place a layer that leaves the tree used to be still needs a repaint, and the layer may already be
    // destroyed by the time anyone notices. Every layer the collecting walk reaches records a visit, and
    // whatever is left unvisited when the walk ends no longer paints, so
    // damageStaleLayerRectsAndAdvanceEntries() repaints and drops it.
    class LayerRectTracker {
        WTF_MAKE_TZONE_ALLOCATED_INLINE(LayerRectTracker);
    public:
        void advanceToNextFrame() { ++m_currentFrameID; }
        uint64_t generateLayerRectID() { return ++m_lastLayerRectID; }

        void recordVisit(uint64_t layerRectID, const FloatRect& rectInFrame);

        // Adds what has to be repainted to the damage, then advances every surviving entry to the rect it
        // painted this frame and drops the entries of the layers that no longer paint.
        void damageStaleLayerRectsAndAdvanceEntries(Damage&);

    private:
        struct Entry {
            FloatRect previousRect;
            FloatRect currentRect; // United over every visit of the walk.
            uint64_t lastVisitedFrameID { 0 };
        };

        bool wasVisitedThisFrame(const Entry& entry) const { return entry.lastVisitedFrameID == m_currentFrameID; }

        HashMap<uint64_t, Entry> m_entries;
        uint64_t m_currentFrameID { 0 };
        uint64_t m_lastLayerRectID { 0 };
    };
#endif

    // The damage-collecting walk gathers damage and draws nothing. The painting walk is the reverse.
    struct PaintContext {
#if ENABLE(DAMAGE_TRACKING)
        // What the collecting walk gathers into. Only that walk engages it, which is how the two walks
        // tell themselves apart.
        struct CollectState {
            Damage& frameDamage;
            LayerRectTracker& layerRectTracker;
            Vector<FloatRect> backdropRectsInFrame; // Resolved once the walk has finished.
        };

        std::optional<CollectState> collectState;

        bool shouldDraw() const { return !collectState; }
#endif
        std::optional<SkiaDamageRegion> compositingDamageRegion;
        const SkiaDamageRegion* damageRegionOrNull() const { return compositingDamageRegion ? &*compositingDamageRegion : nullptr; }
        float opacity { 1 };
        std::optional<SkBlendMode> blendMode;
        IntSize offset;
        sk_sp<SkColorFilter> colorFilter;
        TransformationMatrix accumulatedReplicaTransform;
        RefPtr<SkiaCompositingLayer> paintingBackdropForLayer;
        bool skipAfterBackdrop { false };
        SkiaCompositingLayerImageSetBatch imageSetBatch;
    };

    struct Filter {
        sk_sp<SkImageFilter> filter;
        IntOutsets outsets;
    };
    using PaintFunction = Function<void(SkCanvas&, PaintContext&)>;

    void recursivePaint(SkCanvas&, PaintContext&);
    void paintWithOpacity(SkCanvas&, PaintContext&);
    void paintWithReplica(SkCanvas&, PaintContext&);
    void paintWithMaskAndBackdrop(SkCanvas&, PaintContext&);
    void paintWithBlendMode(SkCanvas&, PaintContext&);
    void paintWithFilterAndMask(SkCanvas&, PaintContext&);
    void paintSelf(SkCanvas&, PaintContext&);
    void paintContents(SkCanvas&, PaintContext&);
    void paintDebugBorder(SkCanvas&, PaintContext&);
    void paintRepaintCounter(SkCanvas&, PaintContext&);
#if ENABLE(DAMAGE_TRACKING)
    void collectFrameDamage(SkCanvas&, PaintContext&);
    void collectBackdropDamage(SkCanvas&, PaintContext&);
    void collectMaskDamage(SkCanvas&, PaintContext&);
    static void resolveBackdropDamage(const Vector<FloatRect>& backdropRectsInFrame, Damage&);
#endif
    void paintSelfAndChildren(SkCanvas&, PaintContext&);
    void paintWithIntermediateSurface(SkCanvas&, PaintContext&, const IntRect&, SkPaint*, PaintFunction&&);
    void paintWith3DRenderingContext(SkCanvas&, PaintContext&);
    void paintBackdrop(SkCanvas&, PaintContext&);
    Vector<IntRect, 1> computeConsolidatedOverlapRegionRects(const SkCanvas&, const PaintContext&, ComputeOverlapRegionMode);
    TransformationMatrix replicaTransform() const;
    TransformationMatrix combinedTransform(const PaintContext&) const;
    IntRect clipBounds(const SkCanvas&, const PaintContext&) const;
    sk_sp<SkImage> maskImage();
    void collect3DRenderingContextLayers(Vector<Ref<SkiaCompositingLayer>>&);
    void recursiveCleanUpAfterPaint();

    void clipRect(SkCanvas&, const FloatRoundedRect&, const TransformationMatrix& = { });

    enum class IncludesReplica : bool { No, Yes };
    void computeOverlapRegions(ComputeOverlapRegionData&, const TransformationMatrix& accumulatedReplicaTransform, IncludesReplica = IncludesReplica::Yes);

    void damageWholeLayer()
    {
#if ENABLE(DAMAGE_TRACKING)
        if (!damagePropagationEnabled() || m_size.isEmpty())
            return;

        if (!m_layerDamage)
            m_layerDamage = Damage(m_size, Damage::Mode::Full);
        else
            m_layerDamage->makeFull();
#endif
    }

#if ENABLE(DAMAGE_TRACKING)
    bool damagePropagationEnabled() const { return m_damagePropagationEnabled; }
    bool hasLayerDamage() const { return m_layerDamage && !m_layerDamage->isEmpty(); }
    void trackLayerRect(PaintContext&, const FloatRect& layerRectInFrame);
#endif

    struct AnimationsState {
        std::optional<TransformationMatrix> transform;
        std::optional<TransformationMatrix> futureTransform;
        std::optional<float> opacity;
        std::optional<Filter> filter;
        bool isRunning { false };
    };
    std::optional<AnimationsState> syncAnimations(MonotonicTime);

    const TransformationMatrix& localTransform() const;
    const TransformationMatrix& futureLocalTransform() const;
    float opacity() const;
    const std::optional<Filter> filter() const;

    struct DebugBorder {
        Color color;
        float width { 0 };

        friend bool operator==(const DebugBorder&, const DebugBorder&) = default;
    };

    Vector<Ref<SkiaCompositingLayer>> m_children;
    WeakPtr<SkiaCompositingLayer> m_parent;
    FloatSize m_size;
    FloatPoint m_position;
    FloatPoint3D m_anchorPoint { 0.5f, 0.5f, 0 };
    FloatPoint m_boundsOrigin;
    FloatRect m_contentsRect;
    struct {
        FloatSize size;
        FloatSize phase;
    } m_contentsTiling;
    TransformationMatrix m_transform;
    TransformationMatrix m_childrenTransform;
    bool m_preserves3D { false };
    bool m_backfaceVisibility { true };
    bool m_contentsVisible { true };
    bool m_contentsOpaque { false };
    bool m_visible { true };
    bool m_masksToBounds { false };
    bool m_contentsRectClipsDescendants { false };
    FloatRoundedRect m_contentsClippingRect;
    float m_opacity { 1 };
    std::optional<SkBlendMode> m_blendMode;
    std::optional<SkPath> m_clipPath;
    sk_sp<SkImage> m_maskImage;
    RefPtr<SkiaCompositingLayer> m_mask;
    RefPtr<SkiaCompositingLayer> m_replica;
    WeakPtr<SkiaCompositingLayer> m_replicatedLayer;
    std::unique_ptr<SkiaBackingStore> m_backingStore;
    RefPtr<CoordinatedAnimatedBackingStoreClient> m_animatedBackingStoreClient;
    RefPtr<CoordinatedImageBackingStore> m_imageBackingStore;
    std::unique_ptr<CoordinatedPlatformLayerBuffer> m_contentsBuffer;
    Color m_contentsSolidColor;
    std::optional<DebugBorder> m_debugBorder;
    std::optional<unsigned> m_repaintCount;

    // Cached repaint-counter overlay shaping, recomputed only when the count
    // changes rather than on every composite of this layer. count is the
    // value string/geometry were computed for.
    struct {
        std::optional<unsigned> count;
        CString string;
        float backgroundWidth { 0 };
        float backgroundHeight { 0 };
        float baselineOffset { 0 };
    } m_repaintCountOverlay;

    std::optional<Filter> m_filter;
    struct {
        sk_sp<SkImageFilter> filter;
        FloatRoundedRect clipRect;
    } m_backdrop;
    bool m_isBackdropRoot { false };
    bool m_shouldBlend { false };
    TextureMapperAnimations m_animations;
    std::optional<AnimationsState> m_animationsState;
    struct {
        TransformationMatrix combined;
        TransformationMatrix futureCombined;
    } m_transforms;
#if ENABLE(DAMAGE_TRACKING)
    bool m_damagePropagationEnabled { false };
    std::optional<Damage> m_layerDamage;
    std::unique_ptr<LayerRectTracker> m_layerRectTracker;
    uint64_t m_layerRectID { 0 };
    bool m_maskChanged { false };
#endif
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
