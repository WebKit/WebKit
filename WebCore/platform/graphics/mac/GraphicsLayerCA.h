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
#include <wtf/RetainPtr.h>

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

    virtual void setBackgroundColor(const Color&, const Animation* anim = 0, double beginTime = 0);
    virtual void clearBackgroundColor();

    virtual void setContentsOpaque(bool);
    virtual void setBackfaceVisibility(bool);

    // return true if we started an animation
    virtual bool setOpacity(float, const Animation* anim = 0, double beginTime = 0);

    virtual void setNeedsDisplay();
    virtual void setNeedsDisplayInRect(const FloatRect&);

    virtual void suspendAnimations();
    virtual void resumeAnimations();

    virtual bool animateTransform(const TransformValueList&, const IntSize&, const Animation*, double beginTime, bool isTransition);
    virtual bool animateFloat(AnimatedPropertyID, const FloatValueList&, const Animation*, double beginTime);

    virtual void setContentsToImage(Image*);
    virtual void setContentsToVideo(PlatformLayer*);
    virtual void clearContents();
    
    virtual void updateContentsRect();

    virtual PlatformLayer* platformLayer() const;

#ifndef NDEBUG
    virtual void setDebugBackgroundColor(const Color&);
    virtual void setDebugBorder(const Color&, float borderWidth);
    virtual void setZPosition(float);
#endif

private:
    WebLayer* primaryLayer() const  { return m_transformLayer.get() ? m_transformLayer.get() : m_layer.get(); }
    WebLayer* hostLayerForSublayers() const;
    WebLayer* layerForSuperlayer() const;

    WebLayer* animatedLayer(AnimatedPropertyID property) const
    {
        return (property == AnimatedPropertyBackgroundColor) ? m_contentsLayer.get() : primaryLayer();
    }

    void setBasicAnimation(AnimatedPropertyID, TransformOperation::OperationType, short index, void* fromVal, void* toVal, bool isTransition, const Animation*, double time);
    void setKeyframeAnimation(AnimatedPropertyID, TransformOperation::OperationType, short index, void* keys, void* values, void* timingFunctions, bool isTransition, const Animation*, double time);

    virtual void removeAnimation(int index, bool reset);

    bool requiresTiledLayer(const FloatSize&) const;
    void swapFromOrToTiledLayer(bool useTiledLayer);

    void setContentsLayer(WebLayer*);
    WebLayer* contentsLayer() const { return m_contentsLayer.get(); }
    
    RetainPtr<WebLayer> m_layer;
    RetainPtr<WebLayer> m_transformLayer;
    RetainPtr<WebLayer> m_contentsLayer;

    RetainPtr<WebAnimationDelegate> m_animationDelegate;

    bool m_contentLayerForImageOrVideo;
};

} // namespace WebCore


#endif // USE(ACCELERATED_COMPOSITING)

#endif // GraphicsLayerCA_h
