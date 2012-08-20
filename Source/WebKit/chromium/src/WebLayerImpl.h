/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebLayerImpl_h
#define WebLayerImpl_h

#include <public/WebLayer.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class LayerChromium;
}

namespace WebKit {

class WebLayerImpl : public WebLayer {
public:
    explicit WebLayerImpl(PassRefPtr<WebCore::LayerChromium>);
    virtual ~WebLayerImpl();

    // WebLayer implementation.
    virtual void invalidateRect(const WebFloatRect&) OVERRIDE;
    virtual void invalidate() OVERRIDE;
    virtual void addChild(WebLayer*) OVERRIDE;
    virtual void insertChild(WebLayer*, size_t index) OVERRIDE;
    virtual void replaceChild(WebLayer* reference, WebLayer* newLayer) OVERRIDE;
    virtual void setChildren(const WebVector<WebLayer*>&) OVERRIDE;
    virtual void removeFromParent() OVERRIDE;
    virtual void removeAllChildren() OVERRIDE;
    virtual void setAnchorPoint(const WebFloatPoint&) OVERRIDE;
    virtual WebFloatPoint anchorPoint() const OVERRIDE;
    virtual void setAnchorPointZ(float) OVERRIDE;
    virtual float anchorPointZ() const OVERRIDE;
    virtual void setBounds(const WebSize&) OVERRIDE;
    virtual WebSize bounds() const OVERRIDE;
    virtual void setMasksToBounds(bool) OVERRIDE;
    virtual bool masksToBounds() const OVERRIDE;
    virtual void setMaskLayer(WebLayer*) OVERRIDE;
    virtual void setReplicaLayer(WebLayer*) OVERRIDE;
    virtual void setOpacity(float) OVERRIDE;
    virtual float opacity() const OVERRIDE;
    virtual void setOpaque(bool) OVERRIDE;
    virtual bool opaque() const OVERRIDE;
    virtual void setPosition(const WebFloatPoint&) OVERRIDE;
    virtual WebFloatPoint position() const OVERRIDE;
    virtual void setSublayerTransform(const SkMatrix44&) OVERRIDE;
    virtual void setSublayerTransform(const WebTransformationMatrix&) OVERRIDE;
    virtual SkMatrix44 sublayerTransform() const OVERRIDE;
    virtual void setTransform(const SkMatrix44&) OVERRIDE;
    virtual void setTransform(const WebTransformationMatrix&) OVERRIDE;
    virtual SkMatrix44 transform() const OVERRIDE;
    virtual void setDrawsContent(bool) OVERRIDE;
    virtual bool drawsContent() const OVERRIDE;
    virtual void setPreserves3D(bool) OVERRIDE;
    virtual void setUseParentBackfaceVisibility(bool) OVERRIDE;
    virtual void setBackgroundColor(WebColor) OVERRIDE;
    virtual void setFilters(const WebFilterOperations&) OVERRIDE;
    virtual void setBackgroundFilters(const WebFilterOperations&) OVERRIDE;
    virtual void setDebugBorderColor(const WebColor&) OVERRIDE;
    virtual void setDebugBorderWidth(float) OVERRIDE;
    virtual void setDebugName(WebString) OVERRIDE;
    virtual void setAnimationDelegate(WebAnimationDelegate*) OVERRIDE;
    virtual bool addAnimation(WebAnimation*) OVERRIDE;
    virtual void removeAnimation(int animationId) OVERRIDE;
    virtual void removeAnimation(int animationId, WebAnimation::TargetProperty) OVERRIDE;
    virtual void pauseAnimation(int animationId, double timeOffset) OVERRIDE;
    virtual void suspendAnimations(double monotonicTime) OVERRIDE;
    virtual void resumeAnimations(double monotonicTime) OVERRIDE;
    virtual bool hasActiveAnimation() OVERRIDE;
    virtual void transferAnimationsTo(WebLayer*) OVERRIDE;
    virtual void setForceRenderSurface(bool) OVERRIDE;
    virtual void setScrollPosition(WebPoint) OVERRIDE;
    virtual void setScrollable(bool) OVERRIDE;
    virtual void setHaveWheelEventHandlers(bool) OVERRIDE;
    virtual void setShouldScrollOnMainThread(bool) OVERRIDE;
    virtual void setNonFastScrollableRegion(const WebVector<WebRect>&) OVERRIDE;
    virtual void setIsContainerForFixedPositionLayers(bool) OVERRIDE;
    virtual void setFixedToContainerLayer(bool) OVERRIDE;

    WebCore::LayerChromium* layer() const;

protected:
    RefPtr<WebCore::LayerChromium> m_layer;
};

} // namespace WebKit

#endif // WebLayerImpl_h
