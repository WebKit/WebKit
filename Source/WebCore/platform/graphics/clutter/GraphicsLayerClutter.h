/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Collabora Ltd.
 * Copyright (C) 2012, 2013 Intel Corporation. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef GraphicsLayerClutter_h
#define GraphicsLayerClutter_h

#if USE(ACCELERATED_COMPOSITING)

#include "GraphicsLayer.h"
#include "GraphicsLayerClient.h"
#include "Image.h"
#include "ImageSource.h"
#include "PlatformClutterAnimation.h"
#include "PlatformClutterLayerClient.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

#include <clutter/clutter.h>
#include <wtf/gobject/GRefPtr.h>

typedef struct _GraphicsLayerActor GraphicsLayerActor;

namespace WebCore {

class TransformState;

typedef Vector<GRefPtr<GraphicsLayerActor> > GraphicsLayerActorList;

class GraphicsLayerClutter : public GraphicsLayer, public PlatformClutterLayerClient {
public:
    enum LayerType { LayerTypeLayer, LayerTypeWebLayer, LayerTypeVideoLayer, LayerTypeTransformLayer, LayerTypeRootLayer, LayerTypeCustom };

    GraphicsLayerClutter(GraphicsLayerClient*);
    virtual ~GraphicsLayerClutter();

    virtual ClutterActor* platformLayer() const;
    virtual void addChild(GraphicsLayer*);
    virtual void addChildAtIndex(GraphicsLayer*, int index);
    virtual void addChildAbove(GraphicsLayer*, GraphicsLayer* sibling);
    virtual void addChildBelow(GraphicsLayer*, GraphicsLayer* sibling);

    virtual void removeFromParent();

    virtual bool replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild);
    virtual bool setChildren(const Vector<GraphicsLayer*>&);
    virtual void setParent(GraphicsLayer*);

    virtual void setDrawsContent(bool);
    virtual void setAnchorPoint(const FloatPoint3D&);
    virtual void setOpacity(float);
    virtual void setPosition(const FloatPoint&);
    virtual void setSize(const FloatSize&);

    virtual void setTransform(const TransformationMatrix&);
    virtual void setName(const String&);
    virtual void setNeedsDisplay();
    virtual void setNeedsDisplayInRect(const FloatRect&);
    virtual void setContentsNeedsDisplay();

    virtual void setContentsToImage(Image*);
    virtual void setContentsRect(const IntRect&);

    virtual bool hasContentsLayer() const { return m_contentsLayer; }

    virtual void setPreserves3D(bool);
    virtual void setMasksToBounds(bool);

    virtual bool addAnimation(const KeyframeValueList&, const IntSize& boxSize, const Animation*, const String& animationName, double timeOffset);
    virtual void removeAnimation(const String& animationName);

    virtual void flushCompositingState(const FloatRect&);
    virtual void flushCompositingStateForThisLayerOnly();

    struct CommitState {
        bool ancestorHasTransformAnimation;
        CommitState()
            : ancestorHasTransformAnimation(false)
        { }
    };
    void recursiveCommitChanges(const CommitState&, const TransformState&, float pageScaleFactor = 1, const FloatPoint& positionRelativeToBase = FloatPoint(), bool affectedByPageScale = false);

private:
    FloatPoint computePositionRelativeToBase(float& pageScale) const;

    bool animationIsRunning(const String& animationName) const
    {
        return m_runningAnimations.find(animationName) != m_runningAnimations.end();
    }

    void commitLayerChangesBeforeSublayers(float pageScaleFactor, const FloatPoint& positionRelativeToBase);
    void commitLayerChangesAfterSublayers();

    void updateOpacityOnLayer();
    void setupContentsLayer(GraphicsLayerActor*);
    GraphicsLayerActor* contentsLayer() const { return m_contentsLayer.get(); }

    virtual void platformClutterLayerAnimationStarted(double beginTime);
    virtual void platformClutterLayerPaintContents(GraphicsContext&, const IntRect& clip);

    GraphicsLayerActor* primaryLayer() const { return m_structuralLayer.get() ? m_structuralLayer.get() : m_layer.get(); }
    GraphicsLayerActor* layerForSuperlayer() const;
    GraphicsLayerActor* animatedLayer(AnimatedPropertyID) const;

    PassRefPtr<PlatformClutterAnimation> createBasicAnimation(const Animation*, const String& keyPath, bool additive);
    PassRefPtr<PlatformClutterAnimation> createKeyframeAnimation(const Animation*, const String&, bool additive);
    void setupAnimation(PlatformClutterAnimation*, const Animation*, bool additive);

    const TimingFunction* timingFunctionForAnimationValue(const AnimationValue&, const Animation&);

    bool setAnimationEndpoints(const KeyframeValueList&, const Animation*, PlatformClutterAnimation*);
    bool setAnimationKeyframes(const KeyframeValueList&, const Animation*, PlatformClutterAnimation*);

    void setAnimationOnLayer(PlatformClutterAnimation*, AnimatedPropertyID, const String& animationName, int index, double timeOffset);
    bool removeClutterAnimationFromLayer(AnimatedPropertyID, const String& animationName, int index);
    void pauseClutterAnimationOnLayer(AnimatedPropertyID, const String& animationName, int index, double timeOffset);

    bool createAnimationFromKeyframes(const KeyframeValueList&, const Animation*, const String& animationName, double timeOffset);
    bool createTransformAnimationsFromKeyframes(const KeyframeValueList&, const Animation*, const String& animationName, double timeOffset, const IntSize& boxSize);

    bool setTransformAnimationEndpoints(const KeyframeValueList&, const Animation*, PlatformClutterAnimation*, int functionIndex, TransformOperation::OperationType, bool isMatrixAnimation, const IntSize& boxSize);
    bool setTransformAnimationKeyframes(const KeyframeValueList&, const Animation*, PlatformClutterAnimation*, int functionIndex, TransformOperation::OperationType, bool isMatrixAnimation, const IntSize& boxSize);

    enum MoveOrCopy { Move, Copy };
    static void moveOrCopyLayerAnimation(MoveOrCopy, const String& animationIdentifier, GraphicsLayerActor* fromLayer, GraphicsLayerActor* toLayer);
    void moveOrCopyAnimations(MoveOrCopy, GraphicsLayerActor* fromLayer, GraphicsLayerActor* toLayer);

    bool appendToUncommittedAnimations(const KeyframeValueList&, const TransformOperations*, const Animation*, const String& animationName, const IntSize& boxSize, int animationIndex, double timeOffset, bool isMatrixAnimation);

    enum LayerChange {
        NoChange = 0,
        NameChanged = 1 << 1,
        ChildrenChanged = 1 << 2, // also used for content layer, and preserves-3d, and size if tiling changes?
        GeometryChanged = 1 << 3,
        TransformChanged = 1 << 4,
        ChildrenTransformChanged = 1 << 5,
        Preserves3DChanged = 1 << 6,
        MasksToBoundsChanged = 1 << 7,
        DrawsContentChanged = 1 << 8, // need this?
        BackgroundColorChanged = 1 << 9,
        ContentsOpaqueChanged = 1 << 10,
        BackfaceVisibilityChanged = 1 << 11,
        OpacityChanged = 1 << 12,
        AnimationChanged = 1 << 13,
        DirtyRectsChanged = 1 << 14,
        ContentsImageChanged = 1 << 15,
        ContentsMediaLayerChanged = 1 << 16,
        ContentsCanvasLayerChanged = 1 << 17,
        ContentsColorLayerChanged = 1 << 18,
        ContentsRectChanged = 1 << 19,
        MaskLayerChanged = 1 << 20,
        ReplicatedLayerChanged = 1 << 21,
        ContentsNeedsDisplay = 1 << 22,
        AcceleratesDrawingChanged = 1 << 23,
        ContentsScaleChanged = 1 << 24,
        ContentsVisibilityChanged = 1 << 25,
        VisibleRectChanged = 1 << 26,
        FiltersChanged = 1 << 27,
        DebugIndicatorsChanged = 1 << 28
    };

    typedef unsigned LayerChangeFlags;
    void noteLayerPropertyChanged(LayerChangeFlags);
    void noteSublayersChanged();

    void updateBackfaceVisibility();
    void updateStructuralLayer();
    void updateLayerNames();
    void updateSublayerList();
    void updateGeometry(float pixelAlignmentScale, const FloatPoint& positionRelativeToBase);
    void updateTransform();
    void updateMasksToBounds();
    void updateLayerDrawsContent(float pixelAlignmentScale, const FloatPoint& positionRelativeToBase);
    void updateContentsImage();
    void updateContentsRect();
    void updateContentsNeedsDisplay();
    void updateAnimations();

    enum StructuralLayerPurpose {
        NoStructuralLayer = 0,
        StructuralLayerForPreserves3D,
        StructuralLayerForReplicaFlattening
    };
    void ensureStructuralLayer(StructuralLayerPurpose);
    StructuralLayerPurpose structuralLayerPurpose() const;

    void repaintLayerDirtyRects();

    GRefPtr<GraphicsLayerActor> m_layer;
    GRefPtr<GraphicsLayerActor> m_structuralLayer; // A layer used for structural reasons, like preserves-3d or replica-flattening. Is the parent of m_layer.
    GRefPtr<GraphicsLayerActor> m_contentsLayer; // A layer used for inner content, like image and video
    enum ContentsLayerPurpose {
        NoContentsLayer = 0,
        ContentsLayerForImage,
        ContentsLayerForMedia,
        ContentsLayerForCanvas,
        ContentsLayerForBackgroundColor
    };

    ContentsLayerPurpose m_contentsLayerPurpose;
    RefPtr<cairo_surface_t> m_pendingContentsImage;

    Vector<FloatRect> m_dirtyRects;
    LayerChangeFlags m_uncommittedChanges;

    // This represents the animation of a single property. There may be multiple transform animations for
    // a single transition or keyframe animation, so index is used to distinguish these.
    struct LayerPropertyAnimation {
        LayerPropertyAnimation(PassRefPtr<PlatformClutterAnimation> caAnimation, const String& animationName, AnimatedPropertyID property, int index, double timeOffset)
        : m_animation(caAnimation)
        , m_name(animationName)
        , m_property(property)
        , m_index(index)
        , m_timeOffset(timeOffset)
        { }

        RefPtr<PlatformClutterAnimation> m_animation;
        String m_name;
        AnimatedPropertyID m_property;
        int m_index;
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
    typedef HashMap<String, Vector<LayerPropertyAnimation> > AnimationsMap;
    AnimationsMap m_runningAnimations;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // GraphicsLayerClutter_h
