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
#include "Damage.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerTransform.h"
#include "TextureMapperAnimation.h"
#include <wtf/EnumSet.h>
#include <wtf/OptionSet.h>

namespace WebCore {
class CoordinatedPlatformLayer;
class CoordinatedPlatformLayerBufferProxy;
class GraphicsLayerKeyframeValueList;
class NativeImage;

class GraphicsLayerCoordinated final : public GraphicsLayer {
public:
    WEBCORE_EXPORT GraphicsLayerCoordinated(Type, GraphicsLayerClient&, Ref<CoordinatedPlatformLayer>&&);
    virtual ~GraphicsLayerCoordinated();

    CoordinatedPlatformLayer& coordinatedPlatformLayer() const { return m_platformLayer.get(); }

    static void clampToSizeIfRectIsInfinite(FloatRect&, const FloatSize&);

private:
    bool isGraphicsLayerCoordinated() const override { return true; }

    std::optional<PlatformLayerIdentifier> primaryLayerID() const override;

    void setPosition(const FloatPoint&) override;
    void syncPosition(const FloatPoint&) override;
    void setBoundsOrigin(const FloatPoint&) override;
    void syncBoundsOrigin(const FloatPoint&) override;
    void setAnchorPoint(const FloatPoint3D&) override;
    void setSize(const FloatSize&) override;

#if ENABLE(SCROLLING_THREAD)
    void setScrollingNodeID(std::optional<ScrollingNodeID>) override;
#endif

    void setTransform(const TransformationMatrix&) override;
    void setChildrenTransform(const TransformationMatrix&) override;

    void setDrawsContent(bool) override;
    void setMasksToBounds(bool) override;
    void setPreserves3D(bool) override;
    void setBackfaceVisibility(bool) override;
    void setOpacity(float) override;
    void setBlendMode(BlendMode) override;
    void setContentsVisible(bool) override;
    void setContentsOpaque(bool) override;
    void setContentsRect(const FloatRect&) override;
    void setContentsRectClipsDescendants(bool) override;
    void setContentsTileSize(const FloatSize&) override;
    void setContentsTilePhase(const FloatSize&) override;
    void setContentsClippingRect(const FloatRoundedRect&) override;
    void setContentsNeedsDisplay() override;
    void setContentsNeedsDisplayInRect(const FloatRect&) override;
    void setContentsToPlatformLayer(PlatformLayer*, ContentsLayerPurpose) override;
    void setContentsDisplayDelegate(RefPtr<GraphicsLayerContentsDisplayDelegate>&&, ContentsLayerPurpose) override;
    RefPtr<GraphicsLayerAsyncContentsDisplayDelegate> createAsyncContentsDisplayDelegate(GraphicsLayerAsyncContentsDisplayDelegate*) override;
    void setContentsToImage(Image*) override;
    void setContentsToSolidColor(const Color&) override;
    bool usesContentsLayer() const override;

    bool setChildren(Vector<Ref<GraphicsLayer>>&&) override;
    void addChild(Ref<GraphicsLayer>&&) override;
    void addChildAtIndex(Ref<GraphicsLayer>&&, int index) override;
    void addChildAbove(Ref<GraphicsLayer>&&, GraphicsLayer*) override;
    void addChildBelow(Ref<GraphicsLayer>&&, GraphicsLayer*) override;
    bool replaceChild(GraphicsLayer* oldChild, Ref<GraphicsLayer>&& newChild) override;
    void willModifyChildren() override;

    void setEventRegion(EventRegion&&) override;

    void setShapeLayerPath(const Path&) override;
    void setShapeLayerWindRule(WindRule) override;

    void deviceOrPageScaleFactorChanged() override;

    float rootRelativeScaleFactor() const { return m_rootRelativeScaleFactor; }
    void setShouldUpdateRootRelativeScaleFactor(bool value) override { m_shouldUpdateRootRelativeScaleFactor = value; }
    void updateRootRelativeScale();

    bool setFilters(const FilterOperations&) override;
    void setMaskLayer(RefPtr<GraphicsLayer>&&) override;
    void setReplicatedByLayer(RefPtr<GraphicsLayer>&&) override;
    bool setBackdropFilters(const FilterOperations&) override;
    void setBackdropFiltersRect(const FloatRoundedRect&) override;
    void setIsBackdropRoot(bool) override;

    bool addAnimation(const GraphicsLayerKeyframeValueList&, const GraphicsLayerAnimation*, const String&, double) override;
    void removeAnimation(const String&, std::optional<AnimatedProperty>) override;
    void pauseAnimation(const String& animationName, double timeOffset) override;
    void suspendAnimations(MonotonicTime) override;
    void resumeAnimations() override;
    void transformRelatedPropertyDidChange() override;
    Vector<GraphicsLayer::AcceleratedAnimationForTesting> acceleratedAnimationsForTesting() const override;

    void setNeedsDisplay() override;
    void setNeedsDisplayInRect(const FloatRect&, ShouldClipToLayer = ShouldClipToLayer::Clip) override;

    FloatSize pixelAlignmentOffset() const override { return m_pixelAlignmentOffset; }

    void flushCompositingState(const FloatRect&) override;
    void flushCompositingStateForThisLayerOnly() override;

    void setShowDebugBorder(bool) override;
    void setShowRepaintCounter(bool) override;
    void dumpAdditionalProperties(TextStream&, OptionSet<LayerTreeAsTextOptions>) const override;

    enum class Change : uint8_t {
        Animations,
        Backdrop,
        BackdropRect,
        BackdropRoot,
        BackfaceVisibility,
        BlendMode,
        Children,
        ChildrenTransform,
        ContentsBuffer,
        ContentsBufferNeedsDisplay,
        ContentsClippingRect,
        ContentsColor,
        ContentsImage,
        ContentsOpaque,
        ContentsRect,
        ContentsRectClipsDescendants,
        ContentsScale,
        ContentsTiling,
        ContentsVisible,
        DebugIndicators,
        DirtyRegion,
        DrawsContent,
        EventRegion,
        Filters,
        Geometry,
        Mask,
        MasksToBounds,
        Opacity,
        Preserves3D,
        Replica,
#if ENABLE(SCROLLING_THREAD)
        ScrollingNode,
#endif
        Shape,
        TileCoverage,
        Transform,
    };

    enum class ScheduleFlush : bool { No, Yes };
    void noteLayerPropertyChanged(EnumSet<Change>, ScheduleFlush);
    void setNeedsUpdateLayerTransform();
    std::pair<FloatPoint, float> computePositionRelativeToBase() const;
    void computePixelAlignmentIfNeeded(float pageScaleFactor, const FloatPoint& positionRelativeToBase, FloatPoint& adjustedPosition, FloatPoint& adjustedBoundsOrigin, FloatPoint3D& adjustedAnchorPoint, FloatSize& adjustedSize);
    void computeLayerTransformIfNeeded(bool affectedByTransformAnimation);
    IntRect transformedRect(const FloatRect&) const;
    IntRect transformedRectIncludingFuture(const FloatRect&) const;
    void updateGeometry(float pageScaleFactor, const FloatPoint&);
    void updateBackdropFilters();
    void updateBackdropFiltersRect();
    void updateAnimations();
    void updateVisibleRect(const FloatRect&);
    void updateIndicators();
    bool isRunningTransformAnimation() const;
    bool filtersCanBeComposited(const FilterOperations&) const;

    struct CommitState {
        FloatRect visibleRect;
        bool ancestorHadChanges { false };
        bool ancestorHasTransformAnimation { false };
    };
    void commitLayerChanges(CommitState&, float pageScaleFactor, const FloatPoint&, bool affectedByTransformAnimation);
    bool needsCommit(CommitState&) const;
    void recursiveCommitChanges(CommitState&, float pageScaleFactor = 1, const FloatPoint& positionRelativeToBase = FloatPoint(), bool affectedByPageScale = false);

    bool updateBackingStoresIfNeeded();
    bool updateBackingStoreIfNeeded();

    const Ref<CoordinatedPlatformLayer> m_platformLayer;
    EnumSet<Change> m_pendingChanges;
    bool m_hasDescendantsWithPendingChanges { false };
    bool m_hasDescendantsWithPendingTilesCreation { false };
    bool m_hasDescendantsWithRunningTransformAnimations { false };
    FloatSize m_pixelAlignmentOffset;
    std::optional<Damage> m_dirtyRegion;
    std::optional<Damage> m_contentsDirtyRegion;
    FloatRect m_visibleRect;
    struct {
        GraphicsLayerTransform current;
        GraphicsLayerTransform future;
        TransformationMatrix cachedInverse;
        TransformationMatrix cachedFutureInverse;
        TransformationMatrix cachedCombined;
    } m_layerTransform;
    bool m_needsUpdateLayerTransform { false };
    bool m_shouldUpdateRootRelativeScaleFactor : 1 { false };
    float m_rootRelativeScaleFactor { 1.0f };
    RefPtr<CoordinatedPlatformLayerBufferProxy> m_contentsBufferProxy;
    RefPtr<GraphicsLayerContentsDisplayDelegate> m_contentsDisplayDelegate;
    RefPtr<NativeImage> m_contentsImage;
    Color m_contentsColor;
    RefPtr<CoordinatedPlatformLayer> m_backdropLayer;
    TextureMapperAnimations m_animations;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_GRAPHICSLAYER(WebCore::GraphicsLayerCoordinated, isGraphicsLayerCoordinated())

#endif // USE(COORDINATED_GRAPHICS)
