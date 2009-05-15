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

#ifndef GraphicsLayer_h
#define GraphicsLayer_h

#if USE(ACCELERATED_COMPOSITING)

#include "Animation.h"
#include "Color.h"
#include "FloatPoint.h"
#include "FloatPoint3D.h"
#include "FloatSize.h"
#include "GraphicsLayerClient.h"
#include "TransformationMatrix.h"
#include "TransformOperations.h"
#include <wtf/OwnPtr.h>

#if PLATFORM(MAC)
#ifdef __OBJC__
@class WebLayer;
@class CALayer;
typedef WebLayer PlatformLayer;
typedef CALayer* NativeLayer;
#else
typedef void* PlatformLayer;
typedef void* NativeLayer;
#endif
#else
typedef void* PlatformLayer;
typedef void* NativeLayer;
#endif

namespace WebCore {

class FloatPoint3D;
class GraphicsContext;
class Image;
class TextStream;
class TimingFunction;

// GraphicsLayer is an abstraction for a rendering surface with backing store,
// which may have associated transformation and animations.

class GraphicsLayer {
public:
    // Used to store one float value of a keyframe animation.
    class FloatValue {
    public:
        FloatValue(float key, float value, const TimingFunction* timingFunction = 0)
            : m_key(key), m_value(value), m_timingFunction(0)
        {
            if (timingFunction)
                m_timingFunction.set(new TimingFunction(*timingFunction));
        }

        FloatValue(const FloatValue& other)
            : m_key(other.key()), m_value(other.value()), m_timingFunction(0)
        {
            if (other.timingFunction())
                m_timingFunction.set(new TimingFunction(*other.timingFunction()));
        }

        const FloatValue& operator=(const FloatValue& other)
        {
            if (&other != this)
                set(other.key(), other.value(), other.timingFunction());
            return *this;
        }

        void set(float key, float value, const TimingFunction*);
        
        float key() const { return m_key; }
        float value() const { return m_value; }
        const TimingFunction* timingFunction() const { return m_timingFunction.get(); }

    private:
        float m_key;
        float m_value;
        OwnPtr<TimingFunction> m_timingFunction;
    };
    
    
    class FloatValueList {
    public:
        void insert(float key, float value, const TimingFunction* timingFunction);

        size_t size() const { return m_values.size(); }
        const FloatValue& at(size_t i) const { return m_values.at(i); }
        const Vector<FloatValue>& values() const { return m_values; }

    private:
        Vector<FloatValue> m_values;
    };

    // Used to store one transform in a keyframe list.
    class TransformValue {
    public:
        TransformValue(float key = NAN, const TransformOperations* value = 0, const TimingFunction* timingFunction = 0)
            : m_key(key)
        {
            if (value)
                m_value.set(new TransformOperations(*value));
            if (timingFunction)
                m_timingFunction.set(new TimingFunction(*timingFunction));
        }

        TransformValue(const TransformValue& other)
            : m_key(other.key())
        {
            if (other.value())
                m_value.set(new TransformOperations(*other.value()));
            if (other.timingFunction())
                m_timingFunction.set(new TimingFunction(*other.timingFunction()));
        }

        const TransformValue& operator=(const TransformValue& other)
        {
            if (&other != this)
                set(other.key(), other.value(), other.timingFunction());
            return *this;
        }

        void set(float key, const TransformOperations* value, const TimingFunction* timingFunction);

        float key() const { return m_key; }
        const TransformOperations* value() const { return m_value.get(); }
        const TimingFunction* timingFunction() const { return m_timingFunction.get(); }
        
    private:
        float m_key;
        OwnPtr<TransformOperations> m_value;
        OwnPtr<TimingFunction> m_timingFunction;
    };
    
    // Used to store a series of transforms in a keyframe list.
    class TransformValueList {
    public:
        typedef Vector<TransformOperation::OperationType> FunctionList;

        size_t size() const { return m_values.size(); }
        const TransformValue& at(size_t i) const { return m_values.at(i); }
        const Vector<TransformValue>& values() const { return m_values; }
        
        void insert(float key, const TransformOperations* value, const TimingFunction* timingFunction);
        
        // return a list of the required functions. List is empty if keyframes are not valid
        // If return value is true, functions contain rotations of >= 180 degrees
        void makeFunctionList(FunctionList& list, bool& isValid, bool& hasBigRotation) const;
    private:
        Vector<TransformValue> m_values;
    };

    static GraphicsLayer* createGraphicsLayer(GraphicsLayerClient*);
    
    virtual ~GraphicsLayer();

    GraphicsLayerClient* client() const { return m_client; }

    // Layer name. Only used to identify layers in debug output
    const String& name() const { return m_name; }
    virtual void setName(const String& name) { m_name = name; }

    // For hosting this GraphicsLayer in a native layer hierarchy.
    virtual NativeLayer nativeLayer() const { return 0; }

    GraphicsLayer* parent() const { return m_parent; };
    void setParent(GraphicsLayer* layer) { m_parent = layer; } // Internal use only.
    
    const Vector<GraphicsLayer*>& children() const { return m_children; }

    // Add child layers. If the child is already parented, it will be removed from its old parent.
    virtual void addChild(GraphicsLayer*);
    virtual void addChildAtIndex(GraphicsLayer*, int index);
    virtual void addChildAbove(GraphicsLayer* layer, GraphicsLayer* sibling);
    virtual void addChildBelow(GraphicsLayer* layer, GraphicsLayer* sibling);
    virtual bool replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild);

    void removeAllChildren();
    virtual void removeFromParent();

    // Offset is origin of the renderer minus origin of the graphics layer (so either zero or negative).
    IntSize offsetFromRenderer() const { return m_offsetFromRenderer; }
    void setOffsetFromRenderer(const IntSize& offset) { m_offsetFromRenderer = offset; }

    // The position of the layer (the location of its top-left corner in its parent)
    const FloatPoint& position() const { return m_position; }
    virtual void setPosition(const FloatPoint& p) { m_position = p; }
    
    // Anchor point: (0, 0) is top left, (1, 1) is bottom right. The anchor point
    // affects the origin of the transforms.
    const FloatPoint3D& anchorPoint() const { return m_anchorPoint; }
    virtual void setAnchorPoint(const FloatPoint3D& p) { m_anchorPoint = p; }

    // The bounds of the layer
    const FloatSize& size() const { return m_size; }
    virtual void setSize(const FloatSize& size) { m_size = size; }

    const TransformationMatrix& transform() const { return m_transform; }
    virtual void setTransform(const TransformationMatrix& t) { m_transform = t; }

    const TransformationMatrix& childrenTransform() const { return m_childrenTransform; }
    virtual void setChildrenTransform(const TransformationMatrix& t) { m_childrenTransform = t; }

    bool preserves3D() const { return m_preserves3D; }
    virtual void setPreserves3D(bool b) { m_preserves3D = b; }
    
    bool masksToBounds() const { return m_masksToBounds; }
    virtual void setMasksToBounds(bool b) { m_masksToBounds = b; }
    
    bool drawsContent() const { return m_drawsContent; }
    virtual void setDrawsContent(bool b) { m_drawsContent = b; }

    // The color used to paint the layer backgrounds
    const Color& backgroundColor() const { return m_backgroundColor; }
    virtual void setBackgroundColor(const Color&, const Animation* = 0, double beginTime = 0);
    virtual void clearBackgroundColor();
    bool backgroundColorSet() const { return m_backgroundColorSet; }

    // opaque means that we know the layer contents have no alpha
    bool contentsOpaque() const { return m_contentsOpaque; }
    virtual void setContentsOpaque(bool b) { m_contentsOpaque = b; }

    bool backfaceVisibility() const { return m_backfaceVisibility; }
    virtual void setBackfaceVisibility(bool b) { m_backfaceVisibility = b; }

    float opacity() const { return m_opacity; }
    // return true if we started an animation
    virtual bool setOpacity(float o, const Animation* = 0, double beginTime = 0);

    // Some GraphicsLayers paint only the foreground or the background content
    GraphicsLayerPaintingPhase drawingPhase() const { return m_paintingPhase; }
    void setDrawingPhase(GraphicsLayerPaintingPhase phase) { m_paintingPhase = phase; }

    virtual void setNeedsDisplay() = 0;
    // mark the given rect (in layer coords) as needing dispay. Never goes deep.
    virtual void setNeedsDisplayInRect(const FloatRect&) = 0;

    virtual bool animateTransform(const TransformValueList&, const IntSize&, const Animation*, double beginTime, bool isTransition) = 0;
    virtual bool animateFloat(AnimatedPropertyID, const FloatValueList&, const Animation*, double beginTime) = 0;
    
    void removeFinishedAnimations(const String& name, int index, bool reset);
    void removeFinishedTransitions(AnimatedPropertyID);
    void removeAllAnimations();

    virtual void suspendAnimations();
    virtual void resumeAnimations();
    
    // Layer contents
    virtual void setContentsToImage(Image*) { }
    virtual void setContentsToVideo(PlatformLayer*) { }
    virtual void setContentsBackgroundColor(const Color&) { }
    virtual void clearContents() { }
    
    virtual void updateContentsRect() { }

    // Callback from the underlying graphics system to draw layer contents.
    void paintGraphicsLayerContents(GraphicsContext&, const IntRect& clip);
    
    virtual PlatformLayer* platformLayer() const { return 0; }
    
    void dumpLayer(TextStream&, int indent = 0) const;

#ifndef NDEBUG
    int repaintCount() const { return m_repaintCount; }
    int incrementRepaintCount() { return ++m_repaintCount; }
#endif

    // Report whether the underlying compositing system uses a top-down
    // or a bottom-up coordinate system.
    enum CompositingCoordinatesOrientation { CompositingCoordinatesTopDown, CompositingCoordinatesBottomUp };
    static CompositingCoordinatesOrientation compositingCoordinatesOrientation();

#ifndef NDEBUG
    static bool showDebugBorders();
    static bool showRepaintCounter();
    
    void updateDebugIndicators();
    
    virtual void setDebugBackgroundColor(const Color&) { }
    virtual void setDebugBorder(const Color&, float /*borderWidth*/) { }
    // z-position is the z-equivalent of position(). It's only used for debugging purposes.
    virtual float zPosition() const { return m_zPosition; }
    virtual void setZPosition(float);
#endif

    static String propertyIdToString(AnimatedPropertyID);
    
protected:
    GraphicsLayer(GraphicsLayerClient*);

    void dumpProperties(TextStream&, int indent) const;

    // returns -1 if not found
    int findAnimationEntry(AnimatedPropertyID, short index) const;
    void addAnimationEntry(AnimatedPropertyID, short index, bool isTransition, const Animation*);

    virtual void removeAnimation(int /*index*/, bool /*reset*/) {}
    void removeAllAnimationsForProperty(AnimatedPropertyID);

    GraphicsLayerClient* m_client;
    String m_name;
    
    // Offset from the owning renderer
    IntSize m_offsetFromRenderer;
    
    // Position is relative to the parent GraphicsLayer
    FloatPoint m_position;
    FloatPoint3D m_anchorPoint;
    FloatSize m_size;
    TransformationMatrix m_transform;
    TransformationMatrix m_childrenTransform;

    Color m_backgroundColor;
    float m_opacity;
#ifndef NDEBUG
    float m_zPosition;
#endif

    bool m_backgroundColorSet : 1;
    bool m_contentsOpaque : 1;
    bool m_preserves3D: 1;
    bool m_backfaceVisibility : 1;
    bool m_usingTiledLayer : 1;
    bool m_masksToBounds : 1;
    bool m_drawsContent : 1;

    GraphicsLayerPaintingPhase m_paintingPhase;

    Vector<GraphicsLayer*> m_children;
    GraphicsLayer* m_parent;
    
    // AnimationEntry represents an animation of a property on this layer.
    // For transform only, there may be more than one, in which case 'index'
    // is an index into the list of transforms. 
    class AnimationEntry {
    public:
        AnimationEntry(const Animation* animation, AnimatedPropertyID property, short index, bool isTransition)
            : m_animation(const_cast<Animation*>(animation))
            , m_property(property)
            , m_index(index)
            , m_isCurrent(true)
            , m_isTransition(isTransition)
        {
        }
        
        const Animation* animation() const { return m_animation.get(); }
        AnimatedPropertyID property() const { return m_property; }
        int index() const { return m_index; }
        bool isCurrent() const { return m_isCurrent; }
        void setIsCurrent(bool b = true) { m_isCurrent = b; }
        bool isTransition() const { return m_isTransition; }

        bool matches(AnimatedPropertyID property, short index) const
        {
            return m_property == property && m_index == index;
        }
        
        void reset(const Animation* animation, bool isTransition)
        {
            m_animation = const_cast<Animation*>(animation);
            m_isTransition = isTransition;
            m_isCurrent = true;
        }
    
    private:
        RefPtr<Animation> m_animation;
        AnimatedPropertyID m_property : 14;
        short m_index : 16;
        bool m_isCurrent : 1;
        bool m_isTransition : 1;
    };
    
    Vector<AnimationEntry> m_animations;      // running animations/transitions
    
#ifndef NDEBUG
    int m_repaintCount;
#endif
};


} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // GraphicsLayer_h

