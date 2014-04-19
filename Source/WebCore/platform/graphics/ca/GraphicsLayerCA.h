/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef GraphicsLayerCA_h
#define GraphicsLayerCA_h

#include "GraphicsLayer.h"
#include "PlatformCAAnimation.h"
#include "PlatformCALayer.h"
#include "PlatformCALayerClient.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(COCOA)
#include "TileController.h"
#endif

// Enable this to add a light red wash over the visible portion of Tiled Layers, as computed
// by flushCompositingState().
// #define VISIBLE_TILE_WASH

namespace WebCore {

class Image;
class TransformState;

class GraphicsLayerCA : public GraphicsLayer, public PlatformCALayerClient {
public:
    // The width and height of a single tile in a tiled layer. Should be large enough to
    // avoid lots of small tiles (and therefore lots of drawing callbacks), but small enough
    // to keep the overall tile cost low.
    static const int kTiledLayerTileSize = 512;

    GraphicsLayerCA(GraphicsLayerClient*);
    virtual ~GraphicsLayerCA();

    virtual void initialize() override;

    virtual void setName(const String&) override;

    virtual PlatformLayerID primaryLayerID() const override;

    virtual PlatformLayer* platformLayer() const override;
    PlatformCALayer* platformCALayer() const { return primaryLayer(); }

    virtual bool setChildren(const Vector<GraphicsLayer*>&) override;
    virtual void addChild(GraphicsLayer*) override;
    virtual void addChildAtIndex(GraphicsLayer*, int index) override;
    virtual void addChildAbove(GraphicsLayer*, GraphicsLayer* sibling) override;
    virtual void addChildBelow(GraphicsLayer*, GraphicsLayer* sibling) override;
    virtual bool replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild) override;

    virtual void removeFromParent() override;

    virtual void setMaskLayer(GraphicsLayer*) override;
    virtual void setReplicatedLayer(GraphicsLayer*) override;

    virtual void setPosition(const FloatPoint&) override;
    virtual void setAnchorPoint(const FloatPoint3D&) override;
    virtual void setSize(const FloatSize&) override;
    virtual void setBoundsOrigin(const FloatPoint&) override;

    virtual void setTransform(const TransformationMatrix&) override;

    virtual void setChildrenTransform(const TransformationMatrix&) override;

    virtual void setPreserves3D(bool) override;
    virtual void setMasksToBounds(bool) override;
    virtual void setDrawsContent(bool) override;
    virtual void setContentsVisible(bool) override;
    virtual void setAcceleratesDrawing(bool) override;

    virtual void setBackgroundColor(const Color&) override;

    virtual void setContentsOpaque(bool) override;
    virtual void setBackfaceVisibility(bool) override;

    // return true if we started an animation
    virtual void setOpacity(float) override;

#if ENABLE(CSS_FILTERS)
    virtual bool setFilters(const FilterOperations&) override;
    virtual bool filtersCanBeComposited(const FilterOperations&);
#endif

#if ENABLE(CSS_COMPOSITING)
    virtual void setBlendMode(BlendMode) override;
#endif

    virtual void setNeedsDisplay() override;
    virtual void setNeedsDisplayInRect(const FloatRect&, ShouldClipToLayer = ClipToLayer) override;
    virtual void setContentsNeedsDisplay() override;
    
    virtual void setContentsRect(const FloatRect&) override;
    virtual void setContentsClippingRect(const FloatRect&) override;
    
    virtual void suspendAnimations(double time) override;
    virtual void resumeAnimations() override;

    virtual bool addAnimation(const KeyframeValueList&, const FloatSize& boxSize, const Animation*, const String& animationName, double timeOffset) override;
    virtual void pauseAnimation(const String& animationName, double timeOffset) override;
    virtual void removeAnimation(const String& animationName) override;

    virtual void setContentsToImage(Image*) override;
    virtual void setContentsToMedia(PlatformLayer*) override;
#if PLATFORM(IOS)
    virtual PlatformLayer* contentsLayerForMedia() const override;
#endif
    virtual void setContentsToCanvas(PlatformLayer*) override;
    virtual void setContentsToSolidColor(const Color&) override;

    virtual bool usesContentsLayer() const override { return m_contentsLayerPurpose != NoContentsLayer; }
    
    virtual void setShowDebugBorder(bool) override;
    virtual void setShowRepaintCounter(bool) override;

    virtual void setDebugBackgroundColor(const Color&) override;
    virtual void setDebugBorder(const Color&, float borderWidth) override;

    virtual void setCustomAppearance(CustomAppearance) override;
    virtual void setCustomBehavior(CustomBehavior) override;

    virtual void layerDidDisplay(PlatformLayer*) override;

    virtual void setMaintainsPixelAlignment(bool) override;
#if PLATFORM(IOS)
    virtual FloatSize pixelAlignmentOffset() const override { return m_pixelAlignmentOffset; }
#endif
    virtual void deviceOrPageScaleFactorChanged() override;

    struct CommitState {
        bool ancestorHasTransformAnimation;
        int treeDepth;
        CommitState()
            : ancestorHasTransformAnimation(false)
            , treeDepth(0)
        { }
    };
    void recursiveCommitChanges(const CommitState&, const TransformState&, const TransformationMatrix& rootRelativeTransformForScaling, float pageScaleFactor = 1, const FloatPoint& positionRelativeToBase = FloatPoint(), bool affectedByPageScale = false);

    virtual void flushCompositingState(const FloatRect&) override;
    virtual void flushCompositingStateForThisLayerOnly() override;

    virtual bool visibleRectChangeRequiresFlush(const FloatRect& visibleRect) const override;

    virtual TiledBacking* tiledBacking() const override;

protected:
    virtual void setOpacityInternal(float) override;
    
    bool animationCanBeAccelerated(const KeyframeValueList&, const Animation*) const;

private:
    virtual bool isGraphicsLayerCA() const override { return true; }

    virtual void willBeDestroyed() override;

    // PlatformCALayerClient overrides
    virtual void platformCALayerLayoutSublayersOfLayer(PlatformCALayer*) override { }
    virtual bool platformCALayerRespondsToLayoutChanges() const override { return false; }

    virtual void platformCALayerAnimationStarted(CFTimeInterval beginTime) override;
    virtual CompositingCoordinatesOrientation platformCALayerContentsOrientation() const override { return contentsOrientation(); }
    virtual void platformCALayerPaintContents(PlatformCALayer*, GraphicsContext&, const FloatRect& clip) override;
    virtual bool platformCALayerShowDebugBorders() const override { return isShowingDebugBorder(); }
    virtual bool platformCALayerShowRepaintCounter(PlatformCALayer*) const override;
    virtual int platformCALayerIncrementRepaintCount(PlatformCALayer*) override { return incrementRepaintCount(); }

    virtual bool platformCALayerContentsOpaque() const override { return contentsOpaque(); }
    virtual bool platformCALayerDrawsContent() const override { return drawsContent(); }
    virtual void platformCALayerLayerDidDisplay(PlatformLayer* layer) override { return layerDidDisplay(layer); }
    virtual void platformCALayerSetNeedsToRevalidateTiles() override;
    virtual float platformCALayerDeviceScaleFactor() const override;
    virtual float platformCALayerContentsScaleMultiplierForNewTiles(PlatformCALayer*) const override;
    virtual bool platformCALayerShouldAggressivelyRetainTiles(PlatformCALayer*) const override;
    virtual bool platformCALayerShouldTemporarilyRetainTileCohorts(PlatformCALayer*) const override;

    virtual bool isCommittingChanges() const override { return m_isCommittingChanges; }

    virtual double backingStoreMemoryEstimate() const override;

    virtual bool shouldRepaintOnSizeChange() const override;

    void updateOpacityOnLayer();
    
#if ENABLE(CSS_FILTERS)
    void updateFilters();
#endif

#if ENABLE(CSS_COMPOSITING)
    void updateBlendMode();
#endif

    virtual PassRefPtr<PlatformCALayer> createPlatformCALayer(PlatformCALayer::LayerType, PlatformCALayerClient* owner);
    virtual PassRefPtr<PlatformCALayer> createPlatformCALayer(PlatformLayer*, PlatformCALayerClient* owner);
    virtual PassRefPtr<PlatformCAAnimation> createPlatformCAAnimation(PlatformCAAnimation::AnimationType, const String& keyPath);

    PlatformCALayer* primaryLayer() const { return m_structuralLayer.get() ? m_structuralLayer.get() : m_layer.get(); }
    PlatformCALayer* hostLayerForSublayers() const;
    PlatformCALayer* layerForSuperlayer() const;
    PlatformCALayer* animatedLayer(AnimatedPropertyID) const;

    typedef String CloneID; // Identifier for a given clone, based on original/replica branching down the tree.
    static bool isReplicatedRootClone(const CloneID& cloneID) { return cloneID[0U] & 1; }

    typedef HashMap<CloneID, RefPtr<PlatformCALayer>> LayerMap;
    LayerMap* primaryLayerClones() const { return m_structuralLayer.get() ? m_structuralLayerClones.get() : m_layerClones.get(); }
    LayerMap* animatedLayerClones(AnimatedPropertyID) const;

    bool createAnimationFromKeyframes(const KeyframeValueList&, const Animation*, const String& animationName, double timeOffset);
    bool createTransformAnimationsFromKeyframes(const KeyframeValueList&, const Animation*, const String& animationName, double timeOffset, const FloatSize& boxSize);
#if ENABLE(CSS_FILTERS)
    bool createFilterAnimationsFromKeyframes(const KeyframeValueList&, const Animation*, const String& animationName, double timeOffset);
#endif

    // Return autoreleased animation (use RetainPtr?)
    PassRefPtr<PlatformCAAnimation> createBasicAnimation(const Animation*, const String& keyPath, bool additive);
    PassRefPtr<PlatformCAAnimation> createKeyframeAnimation(const Animation*, const String&, bool additive);
    void setupAnimation(PlatformCAAnimation*, const Animation*, bool additive);
    
    const TimingFunction* timingFunctionForAnimationValue(const AnimationValue&, const Animation&);
    
    bool setAnimationEndpoints(const KeyframeValueList&, const Animation*, PlatformCAAnimation*);
    bool setAnimationKeyframes(const KeyframeValueList&, const Animation*, PlatformCAAnimation*);

    bool setTransformAnimationEndpoints(const KeyframeValueList&, const Animation*, PlatformCAAnimation*, int functionIndex, TransformOperation::OperationType, bool isMatrixAnimation, const FloatSize& boxSize, Vector<TransformationMatrix>& matrixes);
    bool setTransformAnimationKeyframes(const KeyframeValueList&, const Animation*, PlatformCAAnimation*, int functionIndex, TransformOperation::OperationType, bool isMatrixAnimation, const FloatSize& boxSize, Vector<TransformationMatrix>& matrixes);
    
#if ENABLE(CSS_FILTERS)
    bool setFilterAnimationEndpoints(const KeyframeValueList&, const Animation*, PlatformCAAnimation*, int functionIndex, int internalFilterPropertyIndex);
    bool setFilterAnimationKeyframes(const KeyframeValueList&, const Animation*, PlatformCAAnimation*, int functionIndex, int internalFilterPropertyIndex, FilterOperation::OperationType);
#endif

    bool isRunningTransformAnimation() const;

    bool animationIsRunning(const String& animationName) const
    {
        return m_runningAnimations.find(animationName) != m_runningAnimations.end();
    }

    void commitLayerChangesBeforeSublayers(CommitState&, float pageScaleFactor, const FloatPoint& positionRelativeToBase, const FloatRect& oldVisibleRect, TransformationMatrix* transformFromRoot = 0);
    void commitLayerChangesAfterSublayers(CommitState&);

    FloatPoint computePositionRelativeToBase(float& pageScale) const;

    bool requiresTiledLayer(float pageScaleFactor) const;
    void swapFromOrToTiledLayer(bool useTiledLayer);

    CompositingCoordinatesOrientation defaultContentsOrientation() const;
    
    void setupContentsLayer(PlatformCALayer*);
    PlatformCALayer* contentsLayer() const { return m_contentsLayer.get(); }

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    bool mediaLayerMustBeUpdatedOnMainThread() const;
#endif

    virtual void setReplicatedByLayer(GraphicsLayer*) override;

    virtual void getDebugBorderInfo(Color&, float& width) const override;
    virtual void dumpAdditionalProperties(TextStream&, int indent, LayerTreeAsTextBehavior) const override;

    void computePixelAlignment(float pixelAlignmentScale, const FloatPoint& positionRelativeToBase,
        FloatPoint& position, FloatSize&, FloatPoint3D& anchorPoint, FloatSize& alignmentOffset) const;

    TransformationMatrix layerTransform(const FloatPoint& position, const TransformationMatrix* customTransform = 0) const;
    void updateRootRelativeScale(TransformationMatrix* transformFromRoot);

    enum ComputeVisibleRectFlag { RespectAnimatingTransforms = 1 << 0 };
    typedef unsigned ComputeVisibleRectFlags;
    FloatRect computeVisibleRect(TransformState&, ComputeVisibleRectFlags = RespectAnimatingTransforms) const;
    const FloatRect& visibleRect() const { return m_visibleRect; }
    
    static FloatRect adjustTiledLayerVisibleRect(TiledBacking*, const FloatRect& oldVisibleRect, const FloatRect& newVisibleRect, const FloatSize& oldSize, const FloatSize& newSize);

    bool recursiveVisibleRectChangeRequiresFlush(const TransformState&) const;

    virtual bool canThrottleLayerFlush() const override;

    // Used to track the path down the tree for replica layers.
    struct ReplicaState {
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
    PassRefPtr<PlatformCALayer>replicatedLayerRoot(ReplicaState&);

    enum CloneLevel { RootCloneLevel, IntermediateCloneLevel };
    PassRefPtr<PlatformCALayer> fetchCloneLayers(GraphicsLayer* replicaRoot, ReplicaState&, CloneLevel);
    
    PassRefPtr<PlatformCALayer> cloneLayer(PlatformCALayer *, CloneLevel);
    PassRefPtr<PlatformCALayer> findOrMakeClone(CloneID, PlatformCALayer *, LayerMap*, CloneLevel);

    void ensureCloneLayers(CloneID, RefPtr<PlatformCALayer>& primaryLayer, RefPtr<PlatformCALayer>& structuralLayer,
        RefPtr<PlatformCALayer>& contentsLayer, RefPtr<PlatformCALayer>& contentsClippingLayer, CloneLevel);

    bool hasCloneLayers() const { return m_layerClones; }
    void removeCloneLayers();
    FloatPoint positionForCloneRootLayer() const;
    
    void propagateLayerChangeToReplicas();
    
    // All these "update" methods will be called inside a BEGIN_BLOCK_OBJC_EXCEPTIONS/END_BLOCK_OBJC_EXCEPTIONS block.
    void updateLayerNames();
    void updateSublayerList(bool maxLayerDepthReached = false);
    void updateGeometry(float pixelAlignmentScale, const FloatPoint& positionRelativeToBase);
    void updateTransform();
    void updateChildrenTransform();
    void updateMasksToBounds();
    void updateContentsVisibility();
    void updateContentsOpaque(float pageScaleFactor);
    void updateBackfaceVisibility();
    void updateStructuralLayer();
    void updateLayerDrawsContent();
    void updateBackgroundColor();

    void updateContentsImage();
    void updateContentsMediaLayer();
    void updateContentsCanvasLayer();
    void updateContentsColorLayer();
    void updateContentsRects();
    void updateMaskLayer();
    void updateReplicatedLayers();

    void updateAnimations();
    void updateContentsNeedsDisplay();
    void updateAcceleratesDrawing();
    void updateDebugBorder();
    void updateVisibleRect(const FloatRect& oldVisibleRect);
    void updateTiles();
    void updateContentsScale(float pageScaleFactor);
    void updateCustomAppearance();
    void updateCustomBehavior();

    enum StructuralLayerPurpose {
        NoStructuralLayer = 0,
        StructuralLayerForPreserves3D,
        StructuralLayerForReplicaFlattening
    };
    void ensureStructuralLayer(StructuralLayerPurpose);
    StructuralLayerPurpose structuralLayerPurpose() const;

    void setAnimationOnLayer(PlatformCAAnimation*, AnimatedPropertyID, const String& animationName, int index, int subIndex, double timeOffset);
    bool removeCAAnimationFromLayer(AnimatedPropertyID, const String& animationName, int index, int subINdex);
    void pauseCAAnimationOnLayer(AnimatedPropertyID, const String& animationName, int index, int subIndex, double timeOffset);

    enum MoveOrCopy { Move, Copy };
    static void moveOrCopyLayerAnimation(MoveOrCopy, const String& animationIdentifier, PlatformCALayer *fromLayer, PlatformCALayer *toLayer);
    void moveOrCopyAnimations(MoveOrCopy, PlatformCALayer * fromLayer, PlatformCALayer * toLayer);
    
    bool appendToUncommittedAnimations(const KeyframeValueList&, const TransformOperations*, const Animation*, const String& animationName, const FloatSize& boxSize, int animationIndex, double timeOffset, bool isMatrixAnimation);
#if ENABLE(CSS_FILTERS)
    bool appendToUncommittedAnimations(const KeyframeValueList&, const FilterOperation*, const Animation*, const String& animationName, int animationIndex, double timeOffset);
#endif

    // Returns true if any transform animations are running.
    bool getTransformFromAnimationsWithMaxScaleImpact(const TransformationMatrix& parentTransformFromRoot, TransformationMatrix&, float& maxScale) const;
    
    enum LayerChange {
        NoChange = 0,
        NameChanged = 1LLU << 1,
        ChildrenChanged = 1LLU << 2, // also used for content layer, and preserves-3d, and size if tiling changes?
        GeometryChanged = 1LLU << 3,
        TransformChanged = 1LLU << 4,
        ChildrenTransformChanged = 1LLU << 5,
        Preserves3DChanged = 1LLU << 6,
        MasksToBoundsChanged = 1LLU << 7,
        DrawsContentChanged = 1LLU << 8,
        BackgroundColorChanged = 1LLU << 9,
        ContentsOpaqueChanged = 1LLU << 10,
        BackfaceVisibilityChanged = 1LLU << 11,
        OpacityChanged = 1LLU << 12,
        AnimationChanged = 1LLU << 13,
        DirtyRectsChanged = 1LLU << 14,
        ContentsImageChanged = 1LLU << 15,
        ContentsMediaLayerChanged = 1LLU << 16,
        ContentsCanvasLayerChanged = 1LLU << 17,
        ContentsColorLayerChanged = 1LLU << 18,
        ContentsRectsChanged = 1LLU << 19,
        MaskLayerChanged = 1LLU << 20,
        ReplicatedLayerChanged = 1LLU << 21,
        ContentsNeedsDisplay = 1LLU << 22,
        AcceleratesDrawingChanged = 1LLU << 23,
        ContentsScaleChanged = 1LLU << 24,
        ContentsVisibilityChanged = 1LLU << 25,
        VisibleRectChanged = 1LLU << 26,
        FiltersChanged = 1LLU << 27,
        TilingAreaChanged = 1LLU << 28,
        TilesAdded = 1LLU < 29,
        DebugIndicatorsChanged = 1LLU << 30,
        CustomAppearanceChanged = 1LLU << 31,
        CustomBehaviorChanged = 1LLU << 32,
        BlendModeChanged = 1LLU << 33
    };
    typedef uint64_t LayerChangeFlags;
    enum ScheduleFlushOrNot { ScheduleFlush, DontScheduleFlush };
    void noteLayerPropertyChanged(LayerChangeFlags, ScheduleFlushOrNot = ScheduleFlush);
    void noteSublayersChanged(ScheduleFlushOrNot = ScheduleFlush);
    void noteChangesForScaleSensitiveProperties();

    void repaintLayerDirtyRects();

    RefPtr<PlatformCALayer> m_layer; // The main layer
    RefPtr<PlatformCALayer> m_structuralLayer; // A layer used for structural reasons, like preserves-3d or replica-flattening. Is the parent of m_layer.
    RefPtr<PlatformCALayer> m_contentsClippingLayer; // A layer used to clip inner content
    RefPtr<PlatformCALayer> m_contentsLayer; // A layer used for inner content, like image and video

    // References to clones of our layers, for replicated layers.
    OwnPtr<LayerMap> m_layerClones;
    OwnPtr<LayerMap> m_structuralLayerClones;
    OwnPtr<LayerMap> m_contentsLayerClones;
    OwnPtr<LayerMap> m_contentsClippingLayerClones;

#ifdef VISIBLE_TILE_WASH
    RefPtr<PlatformCALayer> m_visibleTileWashLayer;
#endif
    FloatRect m_visibleRect;
    FloatSize m_sizeAtLastVisibleRectUpdate;
    
    enum ContentsLayerPurpose {
        NoContentsLayer = 0,
        ContentsLayerForImage,
        ContentsLayerForMedia,
        ContentsLayerForCanvas,
        ContentsLayerForBackgroundColor
    };
    
    ContentsLayerPurpose m_contentsLayerPurpose;
    bool m_isPageTiledBackingLayer : 1;
    
    float m_rootRelativeScaleFactor;
    
    Color m_contentsSolidColor;

    RetainPtr<CGImageRef> m_uncorrectedContentsImage;
    RetainPtr<CGImageRef> m_pendingContentsImage;
    
    // This represents the animation of a single property. There may be multiple transform animations for
    // a single transition or keyframe animation, so index is used to distinguish these.
    struct LayerPropertyAnimation {
        LayerPropertyAnimation(PassRefPtr<PlatformCAAnimation> caAnimation, const String& animationName, AnimatedPropertyID property, int index, int subIndex, double timeOffset)
            : m_animation(caAnimation)
            , m_name(animationName)
            , m_property(property)
            , m_index(index)
            , m_subIndex(subIndex)
            , m_timeOffset(timeOffset)
        { }

        RefPtr<PlatformCAAnimation> m_animation;
        String m_name;
        AnimatedPropertyID m_property;
        int m_index;
        int m_subIndex;
        double m_timeOffset;
    };
    
    // Uncommitted transitions and animations.
    Vector<LayerPropertyAnimation> m_uncomittedAnimations;
    
    enum Action { Remove, Pause };
    struct AnimationProcessingAction {
        AnimationProcessingAction(Action action = Remove, double timeOffset = 0)
            : action(action)
            , timeOffset(timeOffset)
        {
        }
        Action action;
        double timeOffset; // only used for pause
    };
    typedef HashMap<String, AnimationProcessingAction> AnimationsToProcessMap;
    AnimationsToProcessMap m_animationsToProcess;

    // Map of animation names to their associated lists of property animations, so we can remove/pause them.
    typedef HashMap<String, Vector<LayerPropertyAnimation>> AnimationsMap;
    AnimationsMap m_runningAnimations;

    // Map from animation key to TransformationMatrices for animations of transform. The vector contains a matrix for
    // the two endpoints, or each keyframe. Used for contentsScale adjustment.
    typedef HashMap<String, Vector<TransformationMatrix>> TransformsMap;
    TransformsMap m_animationTransforms;

    Vector<FloatRect> m_dirtyRects;
    FloatSize m_pixelAlignmentOffset;
    
    LayerChangeFlags m_uncommittedChanges;
    bool m_isCommittingChanges;
};

GRAPHICSLAYER_TYPE_CASTS(GraphicsLayerCA, isGraphicsLayerCA());

} // namespace WebCore

#endif // GraphicsLayerCA_h
