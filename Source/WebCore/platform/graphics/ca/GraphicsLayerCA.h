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

// Enable this to add a light red wash over the visible portion of Tiled Layers, as computed
// by flushCompositingState().
// #define VISIBLE_TILE_WASH

namespace WebCore {

namespace DisplayList {
class DisplayList;
}

class FloatRoundedRect;
class Image;
class TransformState;

class GraphicsLayerCA : public GraphicsLayer, public PlatformCALayerClient {
public:

    WEBCORE_EXPORT explicit GraphicsLayerCA(Type, GraphicsLayerClient&);
    WEBCORE_EXPORT virtual ~GraphicsLayerCA();

    WEBCORE_EXPORT void initialize(Type) override;

    WEBCORE_EXPORT void setName(const String&) override;

    WEBCORE_EXPORT PlatformLayerID primaryLayerID() const override;

    WEBCORE_EXPORT PlatformLayer* platformLayer() const override;
    PlatformCALayer* platformCALayer() const { return primaryLayer(); }

    WEBCORE_EXPORT bool setChildren(const Vector<GraphicsLayer*>&) override;
    WEBCORE_EXPORT void addChild(GraphicsLayer*) override;
    WEBCORE_EXPORT void addChildAtIndex(GraphicsLayer*, int index) override;
    WEBCORE_EXPORT void addChildAbove(GraphicsLayer*, GraphicsLayer* sibling) override;
    WEBCORE_EXPORT void addChildBelow(GraphicsLayer*, GraphicsLayer* sibling) override;
    WEBCORE_EXPORT bool replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild) override;

    WEBCORE_EXPORT void removeFromParent() override;

    WEBCORE_EXPORT void setMaskLayer(GraphicsLayer*) override;
    WEBCORE_EXPORT void setReplicatedLayer(GraphicsLayer*) override;

    WEBCORE_EXPORT void setPosition(const FloatPoint&) override;
    WEBCORE_EXPORT void setAnchorPoint(const FloatPoint3D&) override;
    WEBCORE_EXPORT void setSize(const FloatSize&) override;
    WEBCORE_EXPORT void setBoundsOrigin(const FloatPoint&) override;

    WEBCORE_EXPORT void setTransform(const TransformationMatrix&) override;

    WEBCORE_EXPORT void setChildrenTransform(const TransformationMatrix&) override;

    WEBCORE_EXPORT void setPreserves3D(bool) override;
    WEBCORE_EXPORT void setMasksToBounds(bool) override;
    WEBCORE_EXPORT void setDrawsContent(bool) override;
    WEBCORE_EXPORT void setContentsVisible(bool) override;
    WEBCORE_EXPORT void setAcceleratesDrawing(bool) override;
    WEBCORE_EXPORT void setUsesDisplayListDrawing(bool) override;

    WEBCORE_EXPORT void setBackgroundColor(const Color&) override;

    WEBCORE_EXPORT void setContentsOpaque(bool) override;
    WEBCORE_EXPORT void setSupportsSmoothedFonts(bool) override;

    WEBCORE_EXPORT void setBackfaceVisibility(bool) override;

    // return true if we started an animation
    WEBCORE_EXPORT void setOpacity(float) override;

    WEBCORE_EXPORT bool setFilters(const FilterOperations&) override;
    virtual bool filtersCanBeComposited(const FilterOperations&);

    WEBCORE_EXPORT bool setBackdropFilters(const FilterOperations&) override;
    WEBCORE_EXPORT void setBackdropFiltersRect(const FloatRect&) override;

#if ENABLE(CSS_COMPOSITING)
    WEBCORE_EXPORT void setBlendMode(BlendMode) override;
#endif

    WEBCORE_EXPORT void setNeedsDisplay() override;
    WEBCORE_EXPORT void setNeedsDisplayInRect(const FloatRect&, ShouldClipToLayer = ClipToLayer) override;
    WEBCORE_EXPORT void setContentsNeedsDisplay() override;
    
    WEBCORE_EXPORT void setContentsRect(const FloatRect&) override;
    WEBCORE_EXPORT void setContentsClippingRect(const FloatRoundedRect&) override;
    WEBCORE_EXPORT bool setMasksToBoundsRect(const FloatRoundedRect&) override;

    WEBCORE_EXPORT void setShapeLayerPath(const Path&) override;
    WEBCORE_EXPORT void setShapeLayerWindRule(WindRule) override;

    WEBCORE_EXPORT void suspendAnimations(double time) override;
    WEBCORE_EXPORT void resumeAnimations() override;

    WEBCORE_EXPORT bool addAnimation(const KeyframeValueList&, const FloatSize& boxSize, const Animation*, const String& animationName, double timeOffset) override;
    WEBCORE_EXPORT void pauseAnimation(const String& animationName, double timeOffset) override;
    WEBCORE_EXPORT void removeAnimation(const String& animationName) override;

    WEBCORE_EXPORT void setContentsToImage(Image*) override;
#if PLATFORM(IOS)
    WEBCORE_EXPORT PlatformLayer* contentsLayerForMedia() const override;
#endif
    WEBCORE_EXPORT void setContentsToPlatformLayer(PlatformLayer*, ContentsLayerPurpose) override;
    WEBCORE_EXPORT void setContentsToSolidColor(const Color&) override;

    bool usesContentsLayer() const override { return m_contentsLayerPurpose != NoContentsLayer; }
    
    WEBCORE_EXPORT void setShowDebugBorder(bool) override;
    WEBCORE_EXPORT void setShowRepaintCounter(bool) override;

    WEBCORE_EXPORT void setDebugBackgroundColor(const Color&) override;
    WEBCORE_EXPORT void setDebugBorder(const Color&, float borderWidth) override;

    WEBCORE_EXPORT void setCustomAppearance(CustomAppearance) override;

    WEBCORE_EXPORT void deviceOrPageScaleFactorChanged() override;

    FloatSize pixelAlignmentOffset() const override { return m_pixelAlignmentOffset; }

    struct CommitState {
        int treeDepth { 0 };
        bool ancestorHasTransformAnimation { false };
        bool ancestorIsViewportConstrained { false };
        bool viewportIsStable { true };
        
        CommitState(bool stableViewport)
            : viewportIsStable(stableViewport)
        {
        }
    };
    void recursiveCommitChanges(const CommitState&, const TransformState&, float pageScaleFactor = 1, const FloatPoint& positionRelativeToBase = FloatPoint(), bool affectedByPageScale = false);

    WEBCORE_EXPORT void flushCompositingState(const FloatRect&, bool viewportIsStable) override;
    WEBCORE_EXPORT void flushCompositingStateForThisLayerOnly(bool viewportIsStable) override;

    WEBCORE_EXPORT bool visibleRectChangeRequiresFlush(const FloatRect& visibleRect) const override;

    WEBCORE_EXPORT TiledBacking* tiledBacking() const override;

protected:
    WEBCORE_EXPORT void setOpacityInternal(float) override;
    
    WEBCORE_EXPORT bool animationCanBeAccelerated(const KeyframeValueList&, const Animation*) const;

private:
    bool isGraphicsLayerCA() const override { return true; }

    WEBCORE_EXPORT void willBeDestroyed() override;

    // PlatformCALayerClient overrides
    void platformCALayerLayoutSublayersOfLayer(PlatformCALayer*) override { }
    bool platformCALayerRespondsToLayoutChanges() const override { return false; }
    WEBCORE_EXPORT void platformCALayerCustomSublayersChanged(PlatformCALayer*) override;

    WEBCORE_EXPORT void platformCALayerAnimationStarted(const String& animationKey, CFTimeInterval beginTime) override;
    WEBCORE_EXPORT void platformCALayerAnimationEnded(const String& animationKey) override;
    CompositingCoordinatesOrientation platformCALayerContentsOrientation() const override { return contentsOrientation(); }
    WEBCORE_EXPORT void platformCALayerPaintContents(PlatformCALayer*, GraphicsContext&, const FloatRect& clip) override;
    bool platformCALayerShowDebugBorders() const override { return isShowingDebugBorder(); }
    WEBCORE_EXPORT bool platformCALayerShowRepaintCounter(PlatformCALayer*) const override;
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

    bool isCommittingChanges() const override { return m_isCommittingChanges; }
    bool isUsingDisplayListDrawing(PlatformCALayer*) const override { return m_usesDisplayListDrawing; }

    WEBCORE_EXPORT void setIsViewportConstrained(bool) override;
    bool isViewportConstrained() const override { return m_isViewportConstrained; }

    WEBCORE_EXPORT String displayListAsText(DisplayList::AsTextFlags) const override;

    WEBCORE_EXPORT void setIsTrackingDisplayListReplay(bool) override;
    WEBCORE_EXPORT String replayDisplayListAsText(DisplayList::AsTextFlags) const override;

    WEBCORE_EXPORT double backingStoreMemoryEstimate() const override;

    WEBCORE_EXPORT bool shouldRepaintOnSizeChange() const override;

    WEBCORE_EXPORT void layerDidDisplay(PlatformCALayer*);

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
    bool createFilterAnimationsFromKeyframes(const KeyframeValueList&, const Animation*, const String& animationName, double timeOffset);

    // Return autoreleased animation (use RetainPtr?)
    PassRefPtr<PlatformCAAnimation> createBasicAnimation(const Animation*, const String& keyPath, bool additive);
    PassRefPtr<PlatformCAAnimation> createKeyframeAnimation(const Animation*, const String&, bool additive);
    void setupAnimation(PlatformCAAnimation*, const Animation*, bool additive);
    
    const TimingFunction* timingFunctionForAnimationValue(const AnimationValue&, const Animation&);
    
    bool setAnimationEndpoints(const KeyframeValueList&, const Animation*, PlatformCAAnimation*);
    bool setAnimationKeyframes(const KeyframeValueList&, const Animation*, PlatformCAAnimation*);

    bool setTransformAnimationEndpoints(const KeyframeValueList&, const Animation*, PlatformCAAnimation*, int functionIndex, TransformOperation::OperationType, bool isMatrixAnimation, const FloatSize& boxSize);
    bool setTransformAnimationKeyframes(const KeyframeValueList&, const Animation*, PlatformCAAnimation*, int functionIndex, TransformOperation::OperationType, bool isMatrixAnimation, const FloatSize& boxSize);
    
    bool setFilterAnimationEndpoints(const KeyframeValueList&, const Animation*, PlatformCAAnimation*, int functionIndex, int internalFilterPropertyIndex);
    bool setFilterAnimationKeyframes(const KeyframeValueList&, const Animation*, PlatformCAAnimation*, int functionIndex, int internalFilterPropertyIndex, FilterOperation::OperationType);

    bool isRunningTransformAnimation() const;

    bool animationIsRunning(const String& animationName) const
    {
        return m_runningAnimations.find(animationName) != m_runningAnimations.end();
    }

    void commitLayerChangesBeforeSublayers(CommitState&, float pageScaleFactor, const FloatPoint& positionRelativeToBase);
    void commitLayerChangesAfterSublayers(CommitState&);

    FloatPoint computePositionRelativeToBase(float& pageScale) const;

    bool requiresTiledLayer(float pageScaleFactor) const;
    void changeLayerTypeTo(PlatformCALayer::LayerType);

    CompositingCoordinatesOrientation defaultContentsOrientation() const;

    void setupContentsLayer(PlatformCALayer*);
    PlatformCALayer* contentsLayer() const { return m_contentsLayer.get(); }

    void updateClippingStrategy(PlatformCALayer&, RefPtr<PlatformCALayer>& shapeMaskLayer, const FloatRoundedRect&);

    WEBCORE_EXPORT void setReplicatedByLayer(GraphicsLayer*) override;

    WEBCORE_EXPORT bool canThrottleLayerFlush() const override;

    WEBCORE_EXPORT void getDebugBorderInfo(Color&, float& width) const override;
    WEBCORE_EXPORT void dumpAdditionalProperties(TextStream&, int indent, LayerTreeAsTextBehavior) const override;

    void computePixelAlignment(float contentsScale, const FloatPoint& positionRelativeToBase,
        FloatPoint& position, FloatPoint3D& anchorPoint, FloatSize& alignmentOffset) const;

    TransformationMatrix layerTransform(const FloatPoint& position, const TransformationMatrix* customTransform = 0) const;

    enum ComputeVisibleRectFlag { RespectAnimatingTransforms = 1 << 0 };
    typedef unsigned ComputeVisibleRectFlags;
    
    struct VisibleAndCoverageRects {
        FloatRect visibleRect;
        FloatRect coverageRect;
        
        VisibleAndCoverageRects(const FloatRect& visRect, const FloatRect& covRect)
            : visibleRect(visRect)
            , coverageRect(covRect)
        {
        }
    };
    
    VisibleAndCoverageRects computeVisibleAndCoverageRect(TransformState&, bool accumulateTransform, ComputeVisibleRectFlags = RespectAnimatingTransforms) const;
    bool adjustCoverageRect(VisibleAndCoverageRects&, const FloatRect& oldVisibleRect) const;

    const FloatRect& visibleRect() const { return m_visibleRect; }
    const FloatRect& coverageRect() const { return m_coverageRect; }

    void setVisibleAndCoverageRects(const VisibleAndCoverageRects&, bool isViewportConstrained, bool viewportIsStable);
    
    static FloatRect adjustTiledLayerVisibleRect(TiledBacking*, const FloatRect& oldVisibleRect, const FloatRect& newVisibleRect, const FloatSize& oldSize, const FloatSize& newSize);

    bool recursiveVisibleRectChangeRequiresFlush(const TransformState&) const;
    
    bool isPageTiledBackingLayer() const { return type() == Type::PageTiledBacking; }

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
        RefPtr<PlatformCALayer>& contentsLayer, RefPtr<PlatformCALayer>& contentsClippingLayer, RefPtr<PlatformCALayer>& contentsShapeMaskLayer, RefPtr<PlatformCALayer>& shapeMaskLayer, CloneLevel);

    static void clearClones(std::unique_ptr<LayerMap>&);

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
    void updateContentsFormat();
    void updateBackfaceVisibility();
    void updateStructuralLayer();
    void updateDrawsContent();
    void updateCoverage();
    void updateBackgroundColor();

    void updateContentsImage();
    void updateContentsPlatformLayer();
    void updateContentsColorLayer();
    void updateContentsRects();
    void updateMasksToBoundsRect();
    void updateMaskLayer();
    void updateReplicatedLayers();

    void updateAnimations();
    void updateContentsNeedsDisplay();
    void updateAcceleratesDrawing();
    void updateDebugBorder();
    void updateTiles();
    void updateContentsScale(float pageScaleFactor);
    void updateCustomAppearance();

    void updateOpacityOnLayer();
    void updateFilters();
    void updateBackdropFilters();
    void updateBackdropFiltersRect();

#if ENABLE(CSS_COMPOSITING)
    void updateBlendMode();
#endif

    void updateShape();
    void updateWindRule();

    enum StructuralLayerPurpose {
        NoStructuralLayer = 0,
        StructuralLayerForPreserves3D,
        StructuralLayerForReplicaFlattening,
        StructuralLayerForBackdrop
    };
    void ensureStructuralLayer(StructuralLayerPurpose);
    StructuralLayerPurpose structuralLayerPurpose() const;

    void setAnimationOnLayer(PlatformCAAnimation&, AnimatedPropertyID, const String& animationName, int index, int subIndex, double timeOffset);
    bool removeCAAnimationFromLayer(AnimatedPropertyID, const String& animationName, int index, int subINdex);
    void pauseCAAnimationOnLayer(AnimatedPropertyID, const String& animationName, int index, int subIndex, double timeOffset);

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

    bool appendToUncommittedAnimations(const KeyframeValueList&, const TransformOperations*, const Animation*, const String& animationName, const FloatSize& boxSize, int animationIndex, double timeOffset, bool isMatrixAnimation);
    bool appendToUncommittedAnimations(const KeyframeValueList&, const FilterOperation*, const Animation*, const String& animationName, int animationIndex, double timeOffset);

    enum LayerChange : uint64_t {
        NoChange                        = 0,
        NameChanged                     = 1LLU << 1,
        ChildrenChanged                 = 1LLU << 2, // also used for content layer, and preserves-3d, and size if tiling changes?
        GeometryChanged                 = 1LLU << 3,
        TransformChanged                = 1LLU << 4,
        ChildrenTransformChanged        = 1LLU << 5,
        Preserves3DChanged              = 1LLU << 6,
        MasksToBoundsChanged            = 1LLU << 7,
        DrawsContentChanged             = 1LLU << 8,
        BackgroundColorChanged          = 1LLU << 9,
        ContentsOpaqueChanged           = 1LLU << 10,
        ContentsFormatChanged           = 1LLU << 11,
        BackfaceVisibilityChanged       = 1LLU << 12,
        OpacityChanged                  = 1LLU << 13,
        AnimationChanged                = 1LLU << 14,
        DirtyRectsChanged               = 1LLU << 15,
        ContentsImageChanged            = 1LLU << 16,
        ContentsPlatformLayerChanged    = 1LLU << 17,
        ContentsColorLayerChanged       = 1LLU << 18,
        ContentsRectsChanged            = 1LLU << 19,
        MasksToBoundsRectChanged        = 1LLU << 20,
        MaskLayerChanged                = 1LLU << 21,
        ReplicatedLayerChanged          = 1LLU << 22,
        ContentsNeedsDisplay            = 1LLU << 23,
        AcceleratesDrawingChanged       = 1LLU << 24,
        ContentsScaleChanged            = 1LLU << 25,
        ContentsVisibilityChanged       = 1LLU << 26,
        CoverageRectChanged             = 1LLU << 27,
        FiltersChanged                  = 1LLU << 28,
        BackdropFiltersChanged          = 1LLU << 29,
        BackdropFiltersRectChanged      = 1LLU << 30,
        TilingAreaChanged               = 1LLU << 31,
        TilesAdded                      = 1LLU << 32,
        DebugIndicatorsChanged          = 1LLU << 33,
        CustomAppearanceChanged         = 1LLU << 34,
        BlendModeChanged                = 1LLU << 35,
        ShapeChanged                    = 1LLU << 36,
        WindRuleChanged                 = 1LLU << 37,
    };
    typedef uint64_t LayerChangeFlags;
    enum ScheduleFlushOrNot { ScheduleFlush, DontScheduleFlush };
    void noteLayerPropertyChanged(LayerChangeFlags, ScheduleFlushOrNot = ScheduleFlush);
    void noteSublayersChanged(ScheduleFlushOrNot = ScheduleFlush);
    void noteChangesForScaleSensitiveProperties();

    void propagateLayerChangeToReplicas(ScheduleFlushOrNot = ScheduleFlush);

    void repaintLayerDirtyRects();

    RefPtr<PlatformCALayer> m_layer; // The main layer
    RefPtr<PlatformCALayer> m_structuralLayer; // A layer used for structural reasons, like preserves-3d or replica-flattening. Is the parent of m_layer.
    RefPtr<PlatformCALayer> m_contentsClippingLayer; // A layer used to clip inner content
    RefPtr<PlatformCALayer> m_shapeMaskLayer; // Used to clip with non-trivial corner radii.
    RefPtr<PlatformCALayer> m_contentsLayer; // A layer used for inner content, like image and video
    RefPtr<PlatformCALayer> m_contentsShapeMaskLayer; // Used to clip the content layer with non-trivial corner radii.
    RefPtr<PlatformCALayer> m_backdropLayer; // The layer used for backdrop rendering, if necessary.

    // References to clones of our layers, for replicated layers.
    std::unique_ptr<LayerMap> m_layerClones;
    std::unique_ptr<LayerMap> m_structuralLayerClones;
    std::unique_ptr<LayerMap> m_contentsLayerClones;
    std::unique_ptr<LayerMap> m_contentsClippingLayerClones;
    std::unique_ptr<LayerMap> m_contentsShapeMaskLayerClones;
    std::unique_ptr<LayerMap> m_shapeMaskLayerClones;

#ifdef VISIBLE_TILE_WASH
    RefPtr<PlatformCALayer> m_visibleTileWashLayer;
#endif
    FloatRect m_visibleRect;
    FloatSize m_sizeAtLastCoverageRectUpdate;

    FloatRect m_coverageRect; // Area for which we should maintain backing store, in the coordinate space of this layer.
    
    ContentsLayerPurpose m_contentsLayerPurpose { NoContentsLayer };
    bool m_needsFullRepaint : 1;
    bool m_usingBackdropLayerType : 1;
    bool m_isViewportConstrained : 1;
    bool m_intersectsCoverageRect : 1;
    bool m_hasEverPainted : 1;

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

    Vector<FloatRect> m_dirtyRects;
    
    std::unique_ptr<DisplayList::DisplayList> m_displayList;

    FloatSize m_pixelAlignmentOffset;

    LayerChangeFlags m_uncommittedChanges { 0 };
    bool m_isCommittingChanges { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_GRAPHICSLAYER(WebCore::GraphicsLayerCA, isGraphicsLayerCA())

#endif // GraphicsLayerCA_h
