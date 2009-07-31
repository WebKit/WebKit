/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef GraphicsLayerCA_h
#define GraphicsLayerCA_h

#if USE(ACCELERATED_COMPOSITING)

#include "GraphicsLayer.h"
#include "StringHash.h"
#include <wtf/HashSet.h>
#include <wtf/RetainPtr.h>

@class CABasicAnimation;
@class CAKeyframeAnimation;
@class CALayer;
@class CAMediaTimingFunction;
@class CAPropertyAnimation;
@class WebAnimationDelegate;
@class WebLayer;

namespace WebCore {

class GraphicsLayerCA : public GraphicsLayer {
public:

    GraphicsLayerCA(GraphicsLayerClient*);
    virtual ~GraphicsLayerCA();

    virtual void setName(const String&);

    // for hosting this GraphicsLayer in a native layer hierarchy
    virtual NativeLayer nativeLayer() const;

    virtual void addChild(GraphicsLayer*);
    virtual void addChildAtIndex(GraphicsLayer*, int index);
    virtual void addChildAbove(GraphicsLayer* layer, GraphicsLayer* sibling);
    virtual void addChildBelow(GraphicsLayer* layer, GraphicsLayer* sibling);
    virtual bool replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild);

    virtual void removeFromParent();

    virtual void setPosition(const FloatPoint&);
    virtual void setAnchorPoint(const FloatPoint3D&);
    virtual void setSize(const FloatSize&);

    virtual void setTransform(const TransformationMatrix&);

    virtual void setChildrenTransform(const TransformationMatrix&);

    virtual void setPreserves3D(bool);
    virtual void setMasksToBounds(bool);
    virtual void setDrawsContent(bool);

    virtual void setBackgroundColor(const Color&);
    virtual void clearBackgroundColor();

    virtual void setContentsOpaque(bool);
    virtual void setBackfaceVisibility(bool);

    // return true if we started an animation
    virtual void setOpacity(float);

    virtual void setNeedsDisplay();
    virtual void setNeedsDisplayInRect(const FloatRect&);

    virtual void setContentsRect(const IntRect&);
    
    virtual void suspendAnimations();
    virtual void resumeAnimations();

    virtual bool addAnimation(const KeyframeValueList&, const IntSize& boxSize, const Animation*, const String& keyframesName, double beginTime);
    virtual void removeAnimationsForProperty(AnimatedPropertyID);
    virtual void removeAnimationsForKeyframes(const String& keyframesName);
    virtual void pauseAnimation(const String& keyframesName);
    
    virtual void setContentsToImage(Image*);
    virtual void setContentsToVideo(PlatformLayer*);
    
    virtual PlatformLayer* platformLayer() const;

#ifndef NDEBUG
    virtual void setDebugBackgroundColor(const Color&);
    virtual void setDebugBorder(const Color&, float borderWidth);
#endif

    virtual void setGeometryOrientation(CompositingCoordinatesOrientation);

    void recursiveCommitChanges();
    void commitLayerChanges();

    virtual void syncCompositingState();

protected:
    virtual void setOpacityInternal(float);

private:
    void updateOpacityOnLayer();

    WebLayer* primaryLayer() const { return m_transformLayer.get() ? m_transformLayer.get() : m_layer.get(); }
    WebLayer* hostLayerForSublayers() const;
    WebLayer* layerForSuperlayer() const;
    CALayer* animatedLayer(AnimatedPropertyID property) const;

    bool createAnimationFromKeyframes(const KeyframeValueList&, const Animation*, const String& keyframesName, double beginTime);
    bool createTransformAnimationsFromKeyframes(const KeyframeValueList&, const Animation*, const String& keyframesName, double beginTime, const IntSize& boxSize);

    // Return autoreleased animation (use RetainPtr?)
    CABasicAnimation* createBasicAnimation(const Animation*, AnimatedPropertyID, bool additive);
    CAKeyframeAnimation* createKeyframeAnimation(const Animation*, AnimatedPropertyID, bool additive);
    void setupAnimation(CAPropertyAnimation*, const Animation*, bool additive);
    
    CAMediaTimingFunction* timingFunctionForAnimationValue(const AnimationValue*, const Animation*);
    
    bool setAnimationEndpoints(const KeyframeValueList&, const Animation*, CABasicAnimation*);
    bool setAnimationKeyframes(const KeyframeValueList&, const Animation*, CAKeyframeAnimation*);

    bool setTransformAnimationEndpoints(const KeyframeValueList&, const Animation*, CABasicAnimation*, int functionIndex, TransformOperation::OperationType, bool isMatrixAnimation, const IntSize& boxSize);
    bool setTransformAnimationKeyframes(const KeyframeValueList&, const Animation*, CAKeyframeAnimation*, int functionIndex, TransformOperation::OperationType, bool isMatrixAnimation, const IntSize& boxSize);
    
    bool animationIsRunning(const String& keyframesName) const
    {
        return m_runningKeyframeAnimations.find(keyframesName) != m_runningKeyframeAnimations.end();
    }

    bool requiresTiledLayer(const FloatSize&) const;
    void swapFromOrToTiledLayer(bool useTiledLayer);

    CompositingCoordinatesOrientation defaultContentsOrientation() const;
    void updateContentsTransform();
    
    void setupContentsLayer(CALayer*);
    CALayer* contentsLayer() const { return m_contentsLayer.get(); }
    
    // All these "update" methods will be called inside a BEGIN_BLOCK_OBJC_EXCEPTIONS/END_BLOCK_OBJC_EXCEPTIONS block.
    void updateSublayerList();
    void updateLayerPosition();
    void updateLayerSize();
    void updateAnchorPoint();
    void updateTransform();
    void updateChildrenTransform();
    void updateMasksToBounds();
    void updateContentsOpaque();
    void updateBackfaceVisibility();
    void updateLayerPreserves3D();
    void updateLayerDrawsContent();
    void updateLayerBackgroundColor();

    void updateContentsImage();
    void updateContentsVideo();
    void updateContentsRect();
    void updateGeometryOrientation();

    void updateLayerAnimations();

    void setAnimationOnLayer(CAPropertyAnimation*, AnimatedPropertyID, int index, double beginTime);
    bool removeAnimationFromLayer(AnimatedPropertyID, int index);
    void pauseAnimationOnLayer(AnimatedPropertyID, int index);

    enum LayerChange {
        NoChange = 0,
        NameChanged = 1 << 1,
        ChildrenChanged = 1 << 2,   // also used for content layer, and preserves-3d, and size if tiling changes?
        PositionChanged = 1 << 3,
        AnchorPointChanged = 1 << 4,
        SizeChanged = 1 << 5,
        TransformChanged = 1 << 6,
        ChildrenTransformChanged = 1 << 7,
        Preserves3DChanged = 1 << 8,
        MasksToBoundsChanged = 1 << 9,
        DrawsContentChanged = 1 << 10,  // need this?
        BackgroundColorChanged = 1 << 11,
        ContentsOpaqueChanged = 1 << 12,
        BackfaceVisibilityChanged = 1 << 13,
        OpacityChanged = 1 << 14,
        AnimationChanged = 1 << 15,
        DirtyRectsChanged = 1 << 16,
        ContentsImageChanged = 1 << 17,
        ContentsVideoChanged = 1 << 18,
        ContentsRectChanged = 1 << 19,
        GeometryOrientationChanged = 1 << 20
    };
    typedef unsigned LayerChangeFlags;
    void noteLayerPropertyChanged(LayerChangeFlags flags);

    void repaintLayerDirtyRects();

    RetainPtr<WebLayer> m_layer;
    RetainPtr<WebLayer> m_transformLayer;
    RetainPtr<CALayer> m_contentsLayer;
    
    enum ContentsLayerPurpose {
        NoContentsLayer = 0,
        ContentsLayerForImage,
        ContentsLayerForVideo
    };
    
    ContentsLayerPurpose m_contentsLayerPurpose;
    bool m_contentsLayerHasBackgroundColor : 1;

    RetainPtr<WebAnimationDelegate> m_animationDelegate;

    RetainPtr<CGImageRef> m_pendingContentsImage;
    
    struct LayerAnimation {
        LayerAnimation(CAPropertyAnimation* caAnim, const String& keyframesName, AnimatedPropertyID property, int index, double beginTime)
        : m_animation(caAnim)
        , m_keyframesName(keyframesName)
        , m_property(property)
        , m_index(index)
        , m_beginTime(beginTime)
        { }

        RetainPtr<CAPropertyAnimation*> m_animation;
        String m_keyframesName;
        AnimatedPropertyID m_property;
        int m_index;
        double m_beginTime;
    };
    
    Vector<LayerAnimation> m_uncomittedAnimations;
    
    // Animations on the layer are identified by property + index.
    typedef int AnimatedProperty;   // std containers choke on the AnimatedPropertyID enum
    typedef pair<AnimatedProperty, int> AnimationPair;

    HashSet<AnimatedProperty> m_transitionPropertiesToRemove;
    
    enum { Remove, Pause };
    typedef int AnimationProcessingAction;
    typedef HashMap<String, AnimationProcessingAction> AnimationsToProcessMap;
    AnimationsToProcessMap m_keyframeAnimationsToProcess;

    // Map of keyframe names to their associated lists of animations for running animations, so we can remove/pause them.
    typedef HashMap<String, Vector<AnimationPair> > KeyframeAnimationsMap;
    KeyframeAnimationsMap m_runningKeyframeAnimations;
    
    Vector<FloatRect> m_dirtyRects;
    
    LayerChangeFlags m_uncommittedChanges;
};

} // namespace WebCore


#endif // USE(ACCELERATED_COMPOSITING)

#endif // GraphicsLayerCA_h
