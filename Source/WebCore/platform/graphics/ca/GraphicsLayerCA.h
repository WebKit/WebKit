/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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

#include "GraphicsLayer.h"
#include "PlatformCAAnimation.h"
#include "PlatformCALayer.h"
#include "PlatformCALayerClient.h"
#include <wtf/HashMap.h>
#include <wtf/Optional.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/StringHash.h>

// Enable this to add a light red wash over the visible portion of Tiled Layers, as computed
// by flushCompositingState().
// #define VISIBLE_TILE_WASH

namespace WebCore {

namespace DisplayList {
class InMemoryDisplayList;
}

class FloatRoundedRect;
class Image;
class NativeImage;
class TransformState;

class GraphicsLayerCA : public GraphicsLayer, public PlatformCALayerClient {
public:
    WEBCORE_EXPORT explicit GraphicsLayerCA(Type, GraphicsLayerClient&);
    WEBCORE_EXPORT virtual ~GraphicsLayerCA();

    WEBCORE_EXPORT void initialize(Type) override;

    WEBCORE_EXPORT void setName(const String&) override;
    WEBCORE_EXPORT String debugName() const override;

    WEBCORE_EXPORT PlatformLayerID primaryLayerID() const override;

    WEBCORE_EXPORT PlatformLayer* platformLayer() const override;
    PlatformCALayer* platformCALayer() const { return primaryLayer(); }

    WEBCORE_EXPORT bool setChildren(Vector<Ref<GraphicsLayer>>&&) override;
    WEBCORE_EXPORT void addChild(Ref<GraphicsLayer>&&) override;
    WEBCORE_EXPORT void addChildAtIndex(Ref<GraphicsLayer>&&, int index) override;
    WEBCORE_EXPORT void addChildAbove(Ref<GraphicsLayer>&&, GraphicsLayer* sibling) override;
    WEBCORE_EXPORT void addChildBelow(Ref<GraphicsLayer>&&, GraphicsLayer* sibling) override;
    WEBCORE_EXPORT bool replaceChild(GraphicsLayer* oldChild, Ref<GraphicsLayer>&& newChild) override;

    WEBCORE_EXPORT void removeFromParent() override;

    WEBCORE_EXPORT void setMaskLayer(RefPtr<GraphicsLayer>&&) override;
    WEBCORE_EXPORT void setReplicatedLayer(GraphicsLayer*) override;

    WEBCORE_EXPORT void setPosition(const FloatPoint&) override;
    WEBCORE_EXPORT void syncPosition(const FloatPoint&) override;
    WEBCORE_EXPORT void setApproximatePosition(const FloatPoint&) override;
    WEBCORE_EXPORT void setAnchorPoint(const FloatPoint3D&) override;
    WEBCORE_EXPORT void setSize(const FloatSize&) override;
    WEBCORE_EXPORT void setBoundsOrigin(const FloatPoint&) override;
    WEBCORE_EXPORT void syncBoundsOrigin(const FloatPoint&) override;

    WEBCORE_EXPORT void setTransform(const TransformationMatrix&) override;

    WEBCORE_EXPORT void setChildrenTransform(const TransformationMatrix&) override;

    WEBCORE_EXPORT void setPreserves3D(bool) override;
    WEBCORE_EXPORT void setMasksToBounds(bool) override;
    WEBCORE_EXPORT void setDrawsContent(bool) override;
    WEBCORE_EXPORT void setContentsVisible(bool) override;
    WEBCORE_EXPORT void setAcceleratesDrawing(bool) override;
    WEBCORE_EXPORT void setUsesDisplayListDrawing(bool) override;
    WEBCORE_EXPORT void setUserInteractionEnabled(bool) override;
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    WEBCORE_EXPORT void setSeparated(bool) override;
#endif

    WEBCORE_EXPORT void setBackgroundColor(const Color&) override;

    WEBCORE_EXPORT void setContentsOpaque(bool) override;
    WEBCORE_EXPORT void setSupportsSubpixelAntialiasedText(bool) override;

    WEBCORE_EXPORT void setBackfaceVisibility(bool) override;

    // return true if we started an animation
    WEBCORE_EXPORT void setOpacity(float) override;

    WEBCORE_EXPORT bool setFilters(const FilterOperations&) override;
    virtual bool filtersCanBeComposited(const FilterOperations&);

    WEBCORE_EXPORT bool setBackdropFilters(const FilterOperations&) override;
    WEBCORE_EXPORT void setBackdropFiltersRect(const FloatRoundedRect&) override;

#if ENABLE(CSS_COMPOSITING)
    WEBCORE_EXPORT void setBlendMode(BlendMode) override;
#endif

    WEBCORE_EXPORT void setNeedsDisplay() override;
    WEBCORE_EXPORT void setNeedsDisplayInRect(const FloatRect&, ShouldClipToLayer = ClipToLayer) override;
    WEBCORE_EXPORT void setContentsNeedsDisplay() override;
    
    WEBCORE_EXPORT void setContentsRect(const FloatRect&) override;
    WEBCORE_EXPORT void setContentsClippingRect(const FloatRoundedRect&) override;
    WEBCORE_EXPORT void setContentsRectClipsDescendants(bool) override;
    WEBCORE_EXPORT void setMasksToBoundsRect(const FloatRoundedRect&) override;

    WEBCORE_EXPORT void setShapeLayerPath(const Path&) override;
    WEBCORE_EXPORT void setShapeLayerWindRule(WindRule) override;

    WEBCORE_EXPORT void setEventRegion(EventRegion&&) override;
#if ENABLE(SCROLLING_THREAD)
    WEBCORE_EXPORT void setScrollingNodeID(ScrollingNodeID) override;
#endif

    WEBCORE_EXPORT void suspendAnimations(MonotonicTime) override;
    WEBCORE_EXPORT void resumeAnimations() override;

    WEBCORE_EXPORT bool addAnimation(const KeyframeValueList&, const FloatSize& boxSize, const Animation*, const String& animationName, double timeOffset) override;
    WEBCORE_EXPORT void pauseAnimation(const String& animationName, double timeOffset) override;
    WEBCORE_EXPORT void removeAnimation(const String& animationName) override;
    WEBCORE_EXPORT void transformRelatedPropertyDidChange() override;
    WEBCORE_EXPORT void setContentsToImage(Image*) override;
#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT PlatformLayer* contentsLayerForMedia() const override;
#endif
    WEBCORE_EXPORT void setContentsToPlatformLayer(PlatformLayer*, ContentsLayerPurpose) override;
    WEBCORE_EXPORT void setContentsToSolidColor(const Color&) override;
#if ENABLE(MODEL_ELEMENT)
    WEBCORE_EXPORT void setContentsToModel(RefPtr<Model>&&) override;
#endif

    bool usesContentsLayer() const override { return m_contentsLayerPurpose != ContentsLayerPurpose::None; }
    
    WEBCORE_EXPORT void setShowDebugBorder(bool) override;
    WEBCORE_EXPORT void setShowRepaintCounter(bool) override;

    WEBCORE_EXPORT void setDebugBackgroundColor(const Color&) override;
    WEBCORE_EXPORT void setDebugBorder(const Color&, float borderWidth) override;

    WEBCORE_EXPORT void setCustomAppearance(CustomAppearance) override;

    WEBCORE_EXPORT void deviceOrPageScaleFactorChanged() override;

    FloatSize pixelAlignmentOffset() const override { return m_pixelAlignmentOffset; }

    struct CommitState {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        unsigned treeDepth { 0 };
        unsigned totalBackdropFilterArea { 0 };
        bool ancestorHadChanges { false };
        bool ancestorHasTransformAnimation { false };
        bool ancestorStartedOrEndedTransformAnimation { false };
        bool ancestorWithTransformAnimationIntersectsCoverageRect { false };
    };
    bool needsCommit(const CommitState&);
    void recursiveCommitChanges(CommitState&, const TransformState&, float pageScaleFactor = 1, const FloatPoint& positionRelativeToBase = FloatPoint(), bool affectedByPageScale = false);

    WEBCORE_EXPORT void flushCompositingState(const FloatRect&) override;
    WEBCORE_EXPORT void flushCompositingStateForThisLayerOnly() override;

    WEBCORE_EXPORT bool visibleRectChangeRequiresFlush(const FloatRect& visibleRect) const override;

    WEBCORE_EXPORT TiledBacking* tiledBacking() const override;

    WEBCORE_EXPORT Vector<std::pair<String, double>> acceleratedAnimationsForTesting() const final;

private:
    bool isGraphicsLayerCA() const override { return true; }

    // PlatformCALayerClient overrides
    void platformCALayerLayoutSublayersOfLayer(PlatformCALayer*) override { }
    bool platformCALayerRespondsToLayoutChanges() const override { return false; }
    WEBCORE_EXPORT void platformCALayerCustomSublayersChanged(PlatformCALayer*) override;

    WEBCORE_EXPORT void platformCALayerAnimationStarted(const String& animationKey, MonotonicTime beginTime) override;
    WEBCORE_EXPORT void platformCALayerAnimationEnded(const String& animationKey) override;
    CompositingCoordinatesOrientation platformCALayerContentsOrientation() const override { return contentsOrientation(); }
    WEBCORE_EXPORT void platformCALayerPaintContents(PlatformCALayer*, GraphicsContext&, const FloatRect& clip, GraphicsLayerPaintBehavior) override;
    bool platformCALayerShowDebugBorders() const override { return isShowingDebugBorder(); }
    WEBCORE_EXPORT bool platformCALayerShowRepaintCounter(PlatformCALayer*) const override;
    int platformCALayerRepaintCount(PlatformCALayer*) const override { return repaintCount(); }
    int platformCALayerIncrementRepaintCount(PlatformCALayer*) override { return incrementRepaintCount(); }

    bool platformCALayerContentsOpaque() const override { return contentsOpaque(); }
    bool platformCALayerDrawsContent() const override { return drawsContent(); }
    void platformCALayerLayerDidDisplay(PlatformCALayer* layer) override { return layerDidDisplay(layer); }
    WEBCORE_EXPORT void platformCALayerSetNeedsToRevalidateTiles() override;
    WEBCORE_EXPORT float platformCALayerDeviceScaleFactor() const override;
    WEBCORE_EXPORT float platformCALayerContentsScaleMultiplierForNewTiles(PlatformCALayer*) const override;
    WEBCORE_EXPORT bool platformCALayerShouldAggressivelyRetainTiles(PlatformCALayer*) const override;
    WEBCORE_EXPORT bool platformCALayerShouldTemporarilyRetainTileCohorts(PlatformCALayer*) const override;
    WEBCORE_EXPORT bool platformCALayerUseGiantTiles() const override;
    WEBCORE_EXPORT void platformCALayerLogFilledVisibleFreshTile(unsigned) override;

    bool isCommittingChanges() const override { return m_isCommittingChanges; }
    bool isUsingDisplayListDrawing(PlatformCALayer*) const override { return m_usesDisplayListDrawing; }

    WEBCORE_EXPORT void setAllowsBackingStoreDetaching(bool) override;
    bool allowsBackingStoreDetaching() const override { return m_allowsBackingStoreDetaching; }

    WEBCORE_EXPORT String displayListAsText(DisplayList::AsTextFlags) const override;

    WEBCORE_EXPORT String platformLayerTreeAsText(OptionSet<PlatformLayerTreeAsTextFlags>) const override;

    WEBCORE_EXPORT void setIsTrackingDisplayListReplay(bool) override;
    WEBCORE_EXPORT String replayDisplayListAsText(DisplayList::AsTextFlags) const override;

    WEBCORE_EXPORT double backingStoreMemoryEstimate() const override;

    WEBCORE_EXPORT bool shouldRepaintOnSizeChange() const override;

    WEBCORE_EXPORT void layerDidDisplay(PlatformCALayer*);

    virtual Ref<PlatformCALayer> createPlatformCALayer(PlatformCALayer::LayerType, PlatformCALayerClient* owner);
    virtual Ref<PlatformCALayer> createPlatformCALayer(PlatformLayer*, PlatformCALayerClient* owner);
#if ENABLE(MODEL_ELEMENT)
    virtual Ref<PlatformCALayer> createPlatformCALayer(Ref<WebCore::Model>, PlatformCALayerClient* owner);
#endif
    virtual Ref<PlatformCAAnimation> createPlatformCAAnimation(PlatformCAAnimation::AnimationType, const String& keyPath);

    PlatformCALayer* primaryLayer() const { return m_structuralLayer.get() ? m_structuralLayer.get() : m_layer.get(); }
    PlatformCALayer* hostLayerForSublayers() const;
    PlatformCALayer* layerForSuperlayer() const;
    PlatformCALayer* animatedLayer(AnimatedPropertyID) const;

    typedef String CloneID; // Identifier for a given clone, based on original/replica branching down the tree.
    static bool isReplicatedRootClone(const CloneID& cloneID) { return cloneID[0U] & 1; }

    typedef HashMap<CloneID, RefPtr<PlatformCALayer>> LayerMap;
    LayerMap* primaryLayerClones() const;
    LayerMap* animatedLayerClones(AnimatedPropertyID) const;
    static void clearClones(LayerMap&);

    bool createAnimationFromKeyframes(const KeyframeValueList&, const Animation*, const String& animationName, Seconds timeOffset, bool keyframesShouldUseAnimationWideTimingFunction);
    bool createTransformAnimationsFromKeyframes(const KeyframeValueList&, const Animation*, const String& animationName, Seconds timeOffset, const FloatSize& boxSize, bool keyframesShouldUseAnimationWideTimingFunction);
    bool createFilterAnimationsFromKeyframes(const KeyframeValueList&, const Animation*, const String& animationName, Seconds timeOffset, bool keyframesShouldUseAnimationWideTimingFunction);

    // Return autoreleased animation (use RetainPtr?)
    Ref<PlatformCAAnimation> createBasicAnimation(const Animation*, const String& keyPath, bool additive, bool keyframesShouldUseAnimationWideTimingFunction);
    Ref<PlatformCAAnimation> createKeyframeAnimation(const Animation*, const String&, bool additive, bool keyframesShouldUseAnimationWideTimingFunction);
    Ref<PlatformCAAnimation> createSpringAnimation(const Animation*, const String&, bool additive, bool keyframesShouldUseAnimationWideTimingFunction);
    void setupAnimation(PlatformCAAnimation*, const Animation*, bool additive, bool keyframesShouldUseAnimationWideTimingFunction);
    
    const TimingFunction& timingFunctionForAnimationValue(const AnimationValue&, const Animation&, bool keyframesShouldUseAnimationWideTimingFunction);
    
    bool setAnimationEndpoints(const KeyframeValueList&, const Animation*, PlatformCAAnimation*);
    bool setAnimationKeyframes(const KeyframeValueList&, const Animation*, PlatformCAAnimation*, bool keyframesShouldUseAnimationWideTimingFunction);

    bool setTransformAnimationEndpoints(const KeyframeValueList&, const Animation*, PlatformCAAnimation*, int functionIndex, TransformOperation::OperationType, bool isMatrixAnimation, const FloatSize& boxSize);
    bool setTransformAnimationKeyframes(const KeyframeValueList&, const Animation*, PlatformCAAnimation*, int functionIndex, TransformOperation::OperationType, bool isMatrixAnimation, const FloatSize& boxSize, bool keyframesShouldUseAnimationWideTimingFunction);
    
    bool setFilterAnimationEndpoints(const KeyframeValueList&, const Animation*, PlatformCAAnimation*, int functionIndex, int internalFilterPropertyIndex);
    bool setFilterAnimationKeyframes(const KeyframeValueList&, const Animation*, PlatformCAAnimation*, int functionIndex, int internalFilterPropertyIndex, FilterOperation::OperationType, bool keyframesShouldUseAnimationWideTimingFunction);

    bool isRunningTransformAnimation() const;

    WEBCORE_EXPORT bool backingStoreAttached() const override;
    WEBCORE_EXPORT bool backingStoreAttachedForTesting() const override;

    bool hasAnimations() const { return !m_animations.isEmpty(); }
    bool animationIsRunning(const String& animationName) const;

    void commitLayerChangesBeforeSublayers(CommitState&, float pageScaleFactor, const FloatPoint& positionRelativeToBase, bool& layerTypeChanged);
    void commitLayerChangesAfterSublayers(CommitState&);

    FloatPoint computePositionRelativeToBase(float& pageScale) const;

    bool requiresTiledLayer(float pageScaleFactor) const;
    void changeLayerTypeTo(PlatformCALayer::LayerType);

    CompositingCoordinatesOrientation defaultContentsOrientation() const;

    void setupContentsLayer(PlatformCALayer*);
    PlatformCALayer* contentsLayer() const { return m_contentsLayer.get(); }

    void updateClippingStrategy(PlatformCALayer&, RefPtr<PlatformCALayer>& shapeMaskLayer, const FloatRoundedRect&);

    WEBCORE_EXPORT void setReplicatedByLayer(RefPtr<GraphicsLayer>&&) override;

    WEBCORE_EXPORT void getDebugBorderInfo(Color&, float& width) const override;
    WEBCORE_EXPORT void dumpAdditionalProperties(WTF::TextStream&, LayerTreeAsTextBehavior) const override;
    void dumpInnerLayer(WTF::TextStream&, PlatformCALayer*, OptionSet<PlatformLayerTreeAsTextFlags>) const;
    const char *purposeNameForInnerLayer(PlatformCALayer&) const;

    void computePixelAlignment(float contentsScale, const FloatPoint& positionRelativeToBase,
        FloatPoint& position, FloatPoint3D& anchorPoint, FloatSize& alignmentOffset) const;

    TransformationMatrix layerTransform(const FloatPoint& position, const TransformationMatrix* customTransform = 0) const;
    TransformationMatrix transformByApplyingAnchorPoint(const TransformationMatrix&) const;

    enum ComputeVisibleRectFlag { RespectAnimatingTransforms = 1 << 0 };
    typedef unsigned ComputeVisibleRectFlags;
    
    struct VisibleAndCoverageRects {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        FloatRect visibleRect;
        FloatRect coverageRect;
        TransformationMatrix animatingTransform;
    };
    
    VisibleAndCoverageRects computeVisibleAndCoverageRect(TransformState&, bool accumulateTransform, ComputeVisibleRectFlags = RespectAnimatingTransforms) const;
    bool adjustCoverageRect(VisibleAndCoverageRects&, const FloatRect& oldVisibleRect) const;

    const FloatRect& visibleRect() const { return m_visibleRect; }
    const FloatRect& coverageRect() const { return m_coverageRect; }

    void setVisibleAndCoverageRects(const VisibleAndCoverageRects&);
    
    bool recursiveVisibleRectChangeRequiresFlush(const CommitState&, const TransformState&) const;
    
    bool isPageTiledBackingLayer() const { return type() == Type::PageTiledBacking; }

    // Used to track the path down the tree for replica layers.
    struct ReplicaState {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        static const size_t maxReplicaDepth = 16;
        enum ReplicaBranchType { ChildBranch = 0, ReplicaBranch = 1 };
        ReplicaState(ReplicaBranchType firstBranch)
            : m_replicaDepth(0)
        {
            push(firstBranch);
        }
        
        // Called as we walk down the tree to build replicas.
        void push(ReplicaBranchType branchType)
        {
            m_replicaBranches.append(branchType);
            if (branchType == ReplicaBranch)
                ++m_replicaDepth;
        }
        
        void setBranchType(ReplicaBranchType branchType)
        {
            ASSERT(!m_replicaBranches.isEmpty());

            if (m_replicaBranches.last() != branchType) {
                if (branchType == ReplicaBranch)
                    ++m_replicaDepth;
                else
                    --m_replicaDepth;
            }

            m_replicaBranches.last() = branchType;
        }

        void pop()
        {
            if (m_replicaBranches.last() == ReplicaBranch)
                --m_replicaDepth;
            m_replicaBranches.removeLast();
        }
        
        size_t depth() const { return m_replicaBranches.size(); }
        size_t replicaDepth() const { return m_replicaDepth; }

        CloneID cloneID() const;        

    private:
        Vector<ReplicaBranchType> m_replicaBranches;
        size_t m_replicaDepth;
    };
    RefPtr<PlatformCALayer>replicatedLayerRoot(ReplicaState&);

    enum CloneLevel { RootCloneLevel, IntermediateCloneLevel };
    RefPtr<PlatformCALayer> fetchCloneLayers(GraphicsLayer* replicaRoot, ReplicaState&, CloneLevel);
    
    Ref<PlatformCALayer> cloneLayer(PlatformCALayer *, CloneLevel);
    RefPtr<PlatformCALayer> findOrMakeClone(CloneID, PlatformCALayer *, LayerMap&, CloneLevel);

    void ensureCloneLayers(CloneID, RefPtr<PlatformCALayer>& primaryLayer, RefPtr<PlatformCALayer>& structuralLayer,
        RefPtr<PlatformCALayer>& contentsLayer, RefPtr<PlatformCALayer>& contentsClippingLayer, RefPtr<PlatformCALayer>& contentsShapeMaskLayer,
        RefPtr<PlatformCALayer>& shapeMaskLayer, RefPtr<PlatformCALayer>& backdropLayer, RefPtr<PlatformCALayer>& backdropClippingLayer,
        CloneLevel);

    bool hasCloneLayers() const { return !!m_layerClones; }
    void removeCloneLayers();
    FloatPoint positionForCloneRootLayer() const;

    // All these "update" methods will be called inside a BEGIN_BLOCK_OBJC_EXCEPTIONS/END_BLOCK_OBJC_EXCEPTIONS block.
    void updateNames();
    void updateSublayerList(bool maxLayerDepthReached = false);
    void updateGeometry(float pixelAlignmentScale, const FloatPoint& positionRelativeToBase);
    void updateTransform();
    void updateChildrenTransform();
    void updateMasksToBounds();
    void updateContentsVisibility();
    void updateContentsOpaque(float pageScaleFactor);
    void updateBackfaceVisibility();
    bool updateStructuralLayer();
    void updateDrawsContent();
    void updateCoverage(const CommitState&);
    void updateBackgroundColor();
    void updateUserInteractionEnabled();

    void updateContentsImage();
    void updateContentsPlatformLayer();
    void updateContentsColorLayer();
    void updateContentsRects();
    void updateMasksToBoundsRect();
    void updateEventRegion();
#if ENABLE(SCROLLING_THREAD)
    void updateScrollingNode();
#endif
    void updateMaskLayer();
    void updateReplicatedLayers();

    void updateAnimations();
    void updateContentsNeedsDisplay();
    void updateAcceleratesDrawing();
    void updateSupportsSubpixelAntialiasedText();
    void updateDebugIndicators();
    void updateTiles();
    void updateContentsScale(float pageScaleFactor);
    void updateCustomAppearance();

    void updateOpacityOnLayer();
    void updateFilters();
    void updateBackdropFilters(CommitState&);
    void updateBackdropFiltersRect();

#if ENABLE(CSS_COMPOSITING)
    void updateBlendMode();
#endif

    void updateShape();
    void updateWindRule();

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    void updateSeparated();
#endif

    enum StructuralLayerPurpose {
        NoStructuralLayer = 0,
        StructuralLayerForPreserves3D,
        StructuralLayerForReplicaFlattening,
        StructuralLayerForBackdrop
    };
    bool ensureStructuralLayer(StructuralLayerPurpose);
    StructuralLayerPurpose structuralLayerPurpose() const;

    // This represents the animation of a single property. There may be multiple transform animations for
    // a single transition or keyframe animation, so index is used to distinguish these.
    enum class PlayState { Playing, PlayPending, Paused, PausePending };
    struct LayerPropertyAnimation {
        LayerPropertyAnimation(Ref<PlatformCAAnimation>&& caAnimation, const String& animationName, AnimatedPropertyID property, int index, int subIndex, Seconds timeOffset)
            : m_animation(WTFMove(caAnimation))
            , m_name(animationName)
            , m_property(property)
            , m_index(index)
            , m_subIndex(subIndex)
            , m_timeOffset(timeOffset)
        { }

        String animationIdentifier() const { return makeString(m_name, '_', static_cast<unsigned>(m_property), '_', m_index, '_', m_subIndex); }
        Optional<Seconds> computedBeginTime() const
        {
            if (m_beginTime)
                return *m_beginTime - m_timeOffset;
            return WTF::nullopt;
        }

        RefPtr<PlatformCAAnimation> m_animation;
        String m_name;
        AnimatedPropertyID m_property;
        int m_index;
        int m_subIndex;
        Seconds m_timeOffset { 0_s };
        Optional<Seconds> m_beginTime;
        PlayState m_playState { PlayState::PlayPending };
        bool m_pendingRemoval { false };
    };

    void setAnimationOnLayer(LayerPropertyAnimation&);
    bool removeCAAnimationFromLayer(LayerPropertyAnimation&);
    void pauseCAAnimationOnLayer(LayerPropertyAnimation&);

    enum MoveOrCopy { Move, Copy };
    static void moveOrCopyLayerAnimation(MoveOrCopy, const String& animationIdentifier, PlatformCALayer *fromLayer, PlatformCALayer *toLayer);
    void moveOrCopyAnimations(MoveOrCopy, PlatformCALayer* fromLayer, PlatformCALayer* toLayer);

    void moveAnimations(PlatformCALayer* fromLayer, PlatformCALayer* toLayer)
    {
        moveOrCopyAnimations(Move, fromLayer, toLayer);
    }
    void copyAnimations(PlatformCALayer* fromLayer, PlatformCALayer* toLayer)
    {
        moveOrCopyAnimations(Copy, fromLayer, toLayer);
    }

    bool appendToUncommittedAnimations(const KeyframeValueList&, const TransformOperations*, const Animation*, const String& animationName, const FloatSize& boxSize, int animationIndex, Seconds timeOffset, bool isMatrixAnimation, bool keyframesShouldUseAnimationWideTimingFunction);
    bool appendToUncommittedAnimations(const KeyframeValueList&, const FilterOperation*, const Animation*, const String& animationName, int animationIndex, Seconds timeOffset, bool keyframesShouldUseAnimationWideTimingFunction);

    enum LayerChange : uint64_t {
        NoChange                                = 0,
        NameChanged                             = 1LLU << 1,
        ChildrenChanged                         = 1LLU << 2, // also used for content layer, and preserves-3d, and size if tiling changes?
        GeometryChanged                         = 1LLU << 3,
        TransformChanged                        = 1LLU << 4,
        ChildrenTransformChanged                = 1LLU << 5,
        Preserves3DChanged                      = 1LLU << 6,
        MasksToBoundsChanged                    = 1LLU << 7,
        DrawsContentChanged                     = 1LLU << 8,
        BackgroundColorChanged                  = 1LLU << 9,
        ContentsOpaqueChanged                   = 1LLU << 10,
        BackfaceVisibilityChanged               = 1LLU << 11,
        OpacityChanged                          = 1LLU << 12,
        AnimationChanged                        = 1LLU << 13,
        DirtyRectsChanged                       = 1LLU << 14,
        ContentsImageChanged                    = 1LLU << 15,
        ContentsPlatformLayerChanged            = 1LLU << 16,
        ContentsColorLayerChanged               = 1LLU << 17,
        ContentsRectsChanged                    = 1LLU << 18,
        MasksToBoundsRectChanged                = 1LLU << 19,
        MaskLayerChanged                        = 1LLU << 20,
        ReplicatedLayerChanged                  = 1LLU << 21,
        ContentsNeedsDisplay                    = 1LLU << 22,
        AcceleratesDrawingChanged               = 1LLU << 23,
        SupportsSubpixelAntialiasedTextChanged  = 1LLU << 24,
        ContentsScaleChanged                    = 1LLU << 25,
        ContentsVisibilityChanged               = 1LLU << 26,
        CoverageRectChanged                     = 1LLU << 27,
        FiltersChanged                          = 1LLU << 28,
        BackdropFiltersChanged                  = 1LLU << 29,
        BackdropFiltersRectChanged              = 1LLU << 30,
        TilingAreaChanged                       = 1LLU << 31,
        TilesAdded                              = 1LLU << 32,
        DebugIndicatorsChanged                  = 1LLU << 33,
        CustomAppearanceChanged                 = 1LLU << 34,
        BlendModeChanged                        = 1LLU << 35,
        ShapeChanged                            = 1LLU << 36,
        WindRuleChanged                         = 1LLU << 37,
        UserInteractionEnabledChanged           = 1LLU << 38,
        NeedsComputeVisibleAndCoverageRect      = 1LLU << 39,
        EventRegionChanged                      = 1LLU << 40,
#if ENABLE(SCROLLING_THREAD)
        ScrollingNodeChanged                    = 1LLU << 41,
#endif
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
        SeparatedChanged                        = 1LLU << 42,
#endif
    };
    typedef uint64_t LayerChangeFlags;
    void addUncommittedChanges(LayerChangeFlags);
    bool hasDescendantsWithUncommittedChanges() const { return m_hasDescendantsWithUncommittedChanges; }
    void setHasDescendantsWithUncommittedChanges(bool);
    enum ScheduleFlushOrNot { ScheduleFlush, DontScheduleFlush };
    void noteLayerPropertyChanged(LayerChangeFlags, ScheduleFlushOrNot = ScheduleFlush);
    void noteSublayersChanged(ScheduleFlushOrNot = ScheduleFlush);
    void noteChangesForScaleSensitiveProperties();

    bool hasDescendantsWithRunningTransformAnimations() const { return m_hasDescendantsWithRunningTransformAnimations; }
    void setHasDescendantsWithRunningTransformAnimations(bool b) { m_hasDescendantsWithRunningTransformAnimations = b; }

    void propagateLayerChangeToReplicas(ScheduleFlushOrNot = ScheduleFlush);

    void repaintLayerDirtyRects();

    LayerChangeFlags m_uncommittedChanges { 0 };

    RefPtr<PlatformCALayer> m_layer; // The main layer
    RefPtr<PlatformCALayer> m_structuralLayer; // A layer used for structural reasons, like preserves-3d or replica-flattening. Is the parent of m_layer.
    RefPtr<PlatformCALayer> m_contentsClippingLayer; // A layer used to clip inner content
    RefPtr<PlatformCALayer> m_shapeMaskLayer; // Used to clip with non-trivial corner radii.
    RefPtr<PlatformCALayer> m_backdropClippingLayer; // Used to clip the backdrop layer with corner radii.
    RefPtr<PlatformCALayer> m_contentsLayer; // A layer used for inner content, like image and video
    RefPtr<PlatformCALayer> m_contentsShapeMaskLayer; // Used to clip the content layer with non-trivial corner radii.
    RefPtr<PlatformCALayer> m_backdropLayer; // The layer used for backdrop rendering, if necessary.

    // References to clones of our layers, for replicated layers.
    struct LayerClones {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        LayerMap primaryLayerClones;
        LayerMap structuralLayerClones;
        LayerMap contentsLayerClones;
        LayerMap contentsClippingLayerClones;
        LayerMap contentsShapeMaskLayerClones;
        LayerMap shapeMaskLayerClones;
        LayerMap backdropLayerClones;
        LayerMap backdropClippingLayerClones;
    };

    std::unique_ptr<LayerClones> m_layerClones;

#ifdef VISIBLE_TILE_WASH
    RefPtr<PlatformCALayer> m_visibleTileWashLayer;
#endif
    FloatRect m_visibleRect;
    FloatRect m_previousCommittedVisibleRect;
    FloatRect m_coverageRect; // Area for which we should maintain backing store, in the coordinate space of this layer.
    FloatSize m_sizeAtLastCoverageRectUpdate;
    FloatSize m_pixelAlignmentOffset;

    Color m_contentsSolidColor;

    RefPtr<NativeImage> m_uncorrectedContentsImage;
    RefPtr<NativeImage> m_pendingContentsImage;

#if ENABLE(MODEL_ELEMENT)
    RefPtr<Model> m_contentsModel;
#endif

    Vector<LayerPropertyAnimation> m_animations;
    Vector<LayerPropertyAnimation> m_baseValueTransformAnimations;
    Vector<LayerPropertyAnimation> m_animationGroups;

    Vector<FloatRect> m_dirtyRects;

    std::unique_ptr<DisplayList::InMemoryDisplayList> m_displayList;

    ContentsLayerPurpose m_contentsLayerPurpose { ContentsLayerPurpose::None };
    bool m_isCommittingChanges { false };

    bool m_needsFullRepaint : 1;
    bool m_usingBackdropLayerType : 1;
    bool m_allowsBackingStoreDetaching : 1;
    bool m_intersectsCoverageRect : 1;
    bool m_hasEverPainted : 1;
    bool m_hasDescendantsWithRunningTransformAnimations : 1;
    bool m_hasDescendantsWithUncommittedChanges : 1;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_GRAPHICSLAYER(WebCore::GraphicsLayerCA, isGraphicsLayerCA())
