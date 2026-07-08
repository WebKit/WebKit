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
#include "SkiaDamageRestriction.h"
#include "TextureMapperAnimation.h"
#include "TransformationMatrix.h"
#include <functional>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColorFilter.h>
#include <skia/core/SkM44.h>
#include <skia/core/SkPath.h>
#include <skia/effects/SkImageFilters.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/Function.h>
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

#if ENABLE(DAMAGE_TRACKING)
    // Walks the tree without drawing to gather the frame damage that paint() limits itself to. Being
    // the frame's first walk, it also applies animations and computes transforms.
    void collectDamage(const IntSize& surfaceSize, Damage& frameDamage);
#endif

    // No region paints everything. An empty region paints nothing, since the buffer is already current.
    // Otherwise draw only the region's rects. Prepares transforms first unless collectDamage() did.
    void paint(SkCanvas&, std::optional<DamageRegion>&& = std::nullopt);

    // Whether the scene had running animations this frame. Valid after collectDamage() or paint().
    bool hasRunningAnimations() const { return m_hadRunningAnimations; }

private:
    using ScopedFlush = SkiaCompositingLayerImageSetBatch::ScopedFlush;

    SkiaCompositingLayer() = default;

    void removeFromParent();
    bool isVisible() const;
    bool isLeafOf3DRenderingContext() const { return !m_preserves3D && (m_parent && m_parent->m_preserves3D); }
    bool isReplica() const { return !!m_replicatedLayer; }
    bool hasVisualContent() const;
    bool hasVisiblePaintableContent() const { return !m_size.isEmpty() && m_visible && m_contentsVisible && hasVisualContent(); }
    Ref<SkiaCompositingLayer> backdropRoot();

    bool computeTransformsAndAnimations(const TransformationMatrix& parentTransform, const TransformationMatrix& futureParentTransform, MonotonicTime);

    // Applies animations and computes transforms. The frame's first walk does the work, later ones
    // do nothing. paint() clears the flag when it ends.
    void ensurePrepared();

    enum class PaintMode : uint8_t {
        CollectDamage,
        Paint
    };

    struct PaintContext {
        bool shouldDraw() const { return mode == PaintMode::Paint; }
        bool collectsDamage() const { return mode == PaintMode::CollectDamage; }

        PaintMode mode { PaintMode::Paint };
        std::optional<DamageRegion> compositingDamageRegion;
        float opacity { 1 };
        std::optional<SkBlendMode> blendMode;
        IntSize offset;
        sk_sp<SkColorFilter> colorFilter;
        TransformationMatrix accumulatedReplicaTransform;
        RefPtr<SkiaCompositingLayer> paintingBackdropForLayer;
        bool skipAfterBackdrop { false };
        std::optional<std::reference_wrapper<Damage>> frameDamage; // Set for the CollectDamage walk only.
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
    TransformationMatrix combinedTransform(const PaintContext&) const;
#if ENABLE(DAMAGE_TRACKING)
    void collectFrameDamage(SkCanvas&, PaintContext&);
#endif
    void paintSelfAndChildren(SkCanvas&, PaintContext&);
    void paintWithIntermediateSurface(SkCanvas&, PaintContext&, const IntRect&, SkPaint*, PaintFunction&&);
    void paintWith3DRenderingContext(SkCanvas&, PaintContext&);
    void paintBackdrop(SkCanvas&, PaintContext&);
    Vector<IntRect, 1> computeConsolidatedOverlapRegionRects(const SkCanvas&, const PaintContext&, ComputeOverlapRegionMode);
    TransformationMatrix replicaTransform() const;
    IntRect clipBounds(const SkCanvas&, const PaintContext&) const;
    sk_sp<SkImage> maskImage();
    void collect3DRenderingContextLayers(Vector<Ref<SkiaCompositingLayer>>&);
    void recursiveCleanUpAfterPaint();

    void clipRect(SkCanvas&, const FloatRoundedRect&, const TransformationMatrix& = { });

    enum class IncludesReplica : bool { No, Yes };
    void computeOverlapRegions(ComputeOverlapRegionData&, const TransformationMatrix& accumulatedReplicaTransform, IncludesReplica = IncludesReplica::Yes);

#if ENABLE(DAMAGE_TRACKING)
    bool frameDamagePropagationEnabled() const { return m_damagePropagationEnabled; }
    void damageWholeLayer()
    {
        m_accumulatedOverlapRegionFrameDamage = { };
        if (m_size.isEmpty())
            return;

        if (!m_layerDamage)
            m_layerDamage = Damage(m_size, Damage::Mode::Full);
        else
            m_layerDamage->makeFull();
    }
    // Repaint the layer's last painted position once it moves, hides, or leaves the tree.
    void consumePreviousLayerRect(Damage& frameDamage) { frameDamage.add(std::exchange(m_previousLayerRectInFrameCoordinates, { })); }
    void recursiveConsumePreviousLayerRects(Damage&);
    void recursiveMoveOrphanedPreviousLayerRects(Vector<FloatRect, 1>&);
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
    bool m_preparedThisFrame { false };
    bool m_hadRunningAnimations { false };
    struct {
        TransformationMatrix combined;
        TransformationMatrix futureCombined;
    } m_transforms;
#if ENABLE(DAMAGE_TRACKING)
    bool m_damagePropagationEnabled { false };
    // Last positions of layers removed since the last collect. They still need a repaint even though
    // the walk no longer visits them.
    Vector<FloatRect, 1> m_orphanedPreviousLayerRects;
    std::optional<Damage> m_layerDamage;
    FloatRect m_previousLayerRectInFrameCoordinates;
    FloatRect m_accumulatedOverlapRegionFrameDamage;
#endif
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
