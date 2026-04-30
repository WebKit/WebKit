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
#include "SkiaCompositingLayerOverlapRegions.h"
#include "TextureMapperAnimation.h"
#include "TransformationMatrix.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColorFilter.h>
#include <skia/core/SkM44.h>
#include <skia/core/SkPath.h>
#include <skia/effects/SkImageFilters.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/MonotonicTime.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>

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
    void setSharedFrameDamage(std::shared_ptr<Damage> frameDamage) { m_sharedFrameDamage = WTF::move(frameDamage); }
    void addDamage(Damage&&);
#endif

    void setUseBackingStore(bool, CoordinatedAnimatedBackingStoreClient* = nullptr);
    void updateBackingStore(CoordinatedBackingStoreProxy::Update&&, float);
    void setImageBackingStore(CoordinatedImageBackingStore*);
    void setContentsBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&&);
    CoordinatedPlatformLayerBuffer* contentsBuffer() const { return m_contentsBuffer.get(); }
    void setContentsSolidColor(const Color&);

    void setDebugIndicators(Color&& debugBorderColor, std::optional<float> debugBorderWidth, std::optional<unsigned> repaintCount);

    const TransformationMatrix& toSurfaceTransform() const { return m_transforms.combined; }
    FloatRect effectiveLayerRect() const { return FloatRect({ }, m_size); }

    bool paint(SkCanvas&, std::optional<Damage>&);

private:
    SkiaCompositingLayer() = default;

    void removeFromParent();
    bool isVisible() const;
    bool isLeafOf3DRenderingContext() const { return !m_preserves3D && (m_parent && m_parent->m_preserves3D); }
    bool isReplica() const { return !!m_replicatedLayer; }
    bool hasVisualContent() const;
    Ref<SkiaCompositingLayer> backdropRoot();

    bool computeTransformsAndAnimations(const TransformationMatrix& parentTransform, const TransformationMatrix& futureParentTransform, MonotonicTime);

    struct PaintContext {
        explicit PaintContext(std::optional<Damage>& damage)
            : frameDamage(damage)
        {
        }

        float opacity { 1 };
        std::optional<SkBlendMode> blendMode;
        IntSize offset;
        sk_sp<SkColorFilter> colorFilter;
        TransformationMatrix accumulatedReplicaTransform;
        RefPtr<SkiaCompositingLayer> paintingBackdropForLayer;
        std::optional<Damage>& frameDamage;
    };

    struct Filter {
        sk_sp<SkImageFilter> filter;
        IntOutsets outsets;
    };
    using PaintFunction = Function<void(SkCanvas&, PaintContext&)>;

    void recursivePaint(SkCanvas&, PaintContext&);
    void paintSelf(SkCanvas&, PaintContext&);
    void paintSelfAndChildren(SkCanvas&, PaintContext&);
    void paintSelfAndChildrenWithReplicaFilterAndMask(SkCanvas&, PaintContext&);
    void paintSelfAndChildrenWithFilterAndMask(SkCanvas&, PaintContext&);
    void paintWithIntermediateSurface(SkCanvas&, PaintContext&, const IntRect&, SkPaint*, PaintFunction&&);
    void paintUsingOverlapRegions(SkCanvas&, PaintContext&);
    void paintUsing3DRenderingContext(SkCanvas&, PaintContext&);
    Vector<IntRect, 1> computeConsolidatedOverlapRegionRects(const SkCanvas&, const PaintContext&, ComputeOverlapRegionMode);
    TransformationMatrix replicaTransform() const;
    IntRect clipBounds(const SkCanvas&, const PaintContext&) const;
    sk_sp<SkImage> maskImage();
    void collect3DRenderingContextLayers(Vector<Ref<SkiaCompositingLayer>>&);

    enum class IncludesReplica : bool { No, Yes };
    void computeOverlapRegions(ComputeOverlapRegionData&, const TransformationMatrix& accumulatedReplicaTransform, IncludesReplica = IncludesReplica::Yes);

#if ENABLE(DAMAGE_TRACKING)
    bool frameDamagePropagationEnabled() const { return !!m_sharedFrameDamage; }
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
    void addPreviousRectToSharedFrameDamage();
    void recursiveAddPreviousRectToSharedFrameDamage(Ref<SkiaCompositingLayer>);
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
    std::shared_ptr<Damage> m_sharedFrameDamage;
    std::optional<Damage> m_layerDamage;
    std::optional<FloatRect> m_previousLayerRectInFrameCoordinates;
    FloatRect m_accumulatedOverlapRegionFrameDamage;
#endif
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
