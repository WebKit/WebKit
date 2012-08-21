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

#include "config.h"
#include "WebLayerImpl.h"

#include "CCActiveAnimation.h"
#include "LayerChromium.h"
#include "SkMatrix44.h"
#include "WebAnimationImpl.h"
#include <public/WebFloatPoint.h>
#include <public/WebFloatRect.h>
#include <public/WebSize.h>
#include <public/WebTransformationMatrix.h>

using WebCore::CCActiveAnimation;
using WebCore::LayerChromium;

namespace WebKit {

namespace {

WebTransformationMatrix transformationMatrixFromSkMatrix44(const SkMatrix44& matrix)
{
    double data[16];
    matrix.asColMajord(data);
    return WebTransformationMatrix(data[0], data[1], data[2], data[3],
                                   data[4], data[5], data[6], data[7],
                                   data[8], data[9], data[10], data[11],
                                   data[12], data[13], data[14], data[15]);
}

SkMatrix44 skMatrix44FromTransformationMatrix(const WebTransformationMatrix& matrix)
{
    SkMatrix44 skMatrix;
    skMatrix.set(0, 0, SkDoubleToMScalar(matrix.m11()));
    skMatrix.set(1, 0, SkDoubleToMScalar(matrix.m12()));
    skMatrix.set(2, 0, SkDoubleToMScalar(matrix.m13()));
    skMatrix.set(3, 0, SkDoubleToMScalar(matrix.m14()));
    skMatrix.set(0, 1, SkDoubleToMScalar(matrix.m21()));
    skMatrix.set(1, 1, SkDoubleToMScalar(matrix.m22()));
    skMatrix.set(2, 1, SkDoubleToMScalar(matrix.m23()));
    skMatrix.set(3, 1, SkDoubleToMScalar(matrix.m24()));
    skMatrix.set(0, 2, SkDoubleToMScalar(matrix.m31()));
    skMatrix.set(1, 2, SkDoubleToMScalar(matrix.m32()));
    skMatrix.set(2, 2, SkDoubleToMScalar(matrix.m33()));
    skMatrix.set(3, 2, SkDoubleToMScalar(matrix.m34()));
    skMatrix.set(0, 3, SkDoubleToMScalar(matrix.m41()));
    skMatrix.set(1, 3, SkDoubleToMScalar(matrix.m42()));
    skMatrix.set(2, 3, SkDoubleToMScalar(matrix.m43()));
    skMatrix.set(3, 3, SkDoubleToMScalar(matrix.m44()));
    return skMatrix;
}

} // anonymous namespace


WebLayer* WebLayer::create()
{
    return new WebLayerImpl(LayerChromium::create());
}

WebLayerImpl::WebLayerImpl(PassRefPtr<LayerChromium> layer)
    : m_layer(layer)
{
}

WebLayerImpl::~WebLayerImpl()
{
    m_layer->clearRenderSurface();
    m_layer->setLayerAnimationDelegate(0);
}

int WebLayerImpl::id() const
{
    return m_layer->id();
}

void WebLayerImpl::invalidateRect(const WebFloatRect& rect)
{
    m_layer->setNeedsDisplayRect(rect);
}

void WebLayerImpl::invalidate()
{
    m_layer->setNeedsDisplay();
}

void WebLayerImpl::addChild(WebLayer* child)
{
    m_layer->addChild(static_cast<WebLayerImpl*>(child)->layer());
}

void WebLayerImpl::insertChild(WebLayer* child, size_t index)
{
    m_layer->insertChild(static_cast<WebLayerImpl*>(child)->layer(), index);
}

void WebLayerImpl::replaceChild(WebLayer* reference, WebLayer* newLayer)
{
    m_layer->replaceChild(static_cast<WebLayerImpl*>(reference)->layer(), static_cast<WebLayerImpl*>(newLayer)->layer());
}

void WebLayerImpl::setChildren(const WebVector<WebLayer*>& webChildren)
{
    Vector<RefPtr<LayerChromium> > children(webChildren.size());
    for (size_t i = 0; i < webChildren.size(); ++i)
        children[i] = static_cast<WebLayerImpl*>(webChildren[i])->layer();
    m_layer->setChildren(children);
}

void WebLayerImpl::removeFromParent()
{
    m_layer->removeFromParent();
}

void WebLayerImpl::removeAllChildren()
{
    m_layer->removeAllChildren();
}

void WebLayerImpl::setAnchorPoint(const WebFloatPoint& anchorPoint)
{
    m_layer->setAnchorPoint(anchorPoint);
}

WebFloatPoint WebLayerImpl::anchorPoint() const
{
    return WebFloatPoint(m_layer->anchorPoint());
}

void WebLayerImpl::setAnchorPointZ(float anchorPointZ)
{
    m_layer->setAnchorPointZ(anchorPointZ);
}

float WebLayerImpl::anchorPointZ() const
{
    return m_layer->anchorPointZ();
}

void WebLayerImpl::setBounds(const WebSize& size)
{
    m_layer->setBounds(size);
}

WebSize WebLayerImpl::bounds() const
{
    return WebSize(m_layer->bounds());
}

void WebLayerImpl::setMasksToBounds(bool masksToBounds)
{
    m_layer->setMasksToBounds(masksToBounds);
}

bool WebLayerImpl::masksToBounds() const
{
    return m_layer->masksToBounds();
}

void WebLayerImpl::setMaskLayer(WebLayer* maskLayer)
{
    m_layer->setMaskLayer(maskLayer ? static_cast<WebLayerImpl*>(maskLayer)->layer() : 0);
}

void WebLayerImpl::setReplicaLayer(WebLayer* replicaLayer)
{
    m_layer->setReplicaLayer(replicaLayer ? static_cast<WebLayerImpl*>(replicaLayer)->layer() : 0);
}

void WebLayerImpl::setOpacity(float opacity)
{
    m_layer->setOpacity(opacity);
}

float WebLayerImpl::opacity() const
{
    return m_layer->opacity();
}

void WebLayerImpl::setOpaque(bool opaque)
{
    m_layer->setOpaque(opaque);
}

bool WebLayerImpl::opaque() const
{
    return m_layer->opaque();
}

void WebLayerImpl::setPosition(const WebFloatPoint& position)
{
    m_layer->setPosition(position);
}

WebFloatPoint WebLayerImpl::position() const
{
    return WebFloatPoint(m_layer->position());
}

void WebLayerImpl::setSublayerTransform(const SkMatrix44& matrix)
{
    m_layer->setSublayerTransform(transformationMatrixFromSkMatrix44(matrix));
}

void WebLayerImpl::setSublayerTransform(const WebTransformationMatrix& matrix)
{
    m_layer->setSublayerTransform(matrix);
}

SkMatrix44 WebLayerImpl::sublayerTransform() const
{
    return skMatrix44FromTransformationMatrix(m_layer->sublayerTransform());
}

void WebLayerImpl::setTransform(const SkMatrix44& matrix)
{
    m_layer->setTransform(transformationMatrixFromSkMatrix44(matrix));
}

void WebLayerImpl::setTransform(const WebTransformationMatrix& matrix)
{
    m_layer->setTransform(matrix);
}

SkMatrix44 WebLayerImpl::transform() const
{
    return skMatrix44FromTransformationMatrix(m_layer->transform());
}

void WebLayerImpl::setDrawsContent(bool drawsContent)
{
    m_layer->setIsDrawable(drawsContent);
}

bool WebLayerImpl::drawsContent() const
{
    return m_layer->drawsContent();
}

void WebLayerImpl::setPreserves3D(bool preserve3D)
{
    m_layer->setPreserves3D(preserve3D);
}

void WebLayerImpl::setUseParentBackfaceVisibility(bool useParentBackfaceVisibility)
{
    m_layer->setUseParentBackfaceVisibility(useParentBackfaceVisibility);
}

void WebLayerImpl::setBackgroundColor(WebColor color)
{
    m_layer->setBackgroundColor(color);
}

void WebLayerImpl::setFilters(const WebFilterOperations& filters)
{
    m_layer->setFilters(filters);
}

void WebLayerImpl::setBackgroundFilters(const WebFilterOperations& filters)
{
    m_layer->setBackgroundFilters(filters);
}

void WebLayerImpl::setDebugBorderColor(const WebColor& color)
{
    m_layer->setDebugBorderColor(color);
}

void WebLayerImpl::setDebugBorderWidth(float width)
{
    m_layer->setDebugBorderWidth(width);
}

void WebLayerImpl::setDebugName(WebString name)
{
    m_layer->setDebugName(name);
}

void WebLayerImpl::setAnimationDelegate(WebAnimationDelegate* delegate)
{
    m_layer->setLayerAnimationDelegate(delegate);
}

bool WebLayerImpl::addAnimation(WebAnimation* animation)
{
    return m_layer->addAnimation(static_cast<WebAnimationImpl*>(animation)->cloneToCCAnimation());
}

void WebLayerImpl::removeAnimation(int animationId)
{
    m_layer->removeAnimation(animationId);
}

void WebLayerImpl::removeAnimation(int animationId, WebAnimation::TargetProperty targetProperty)
{
    m_layer->layerAnimationController()->removeAnimation(animationId, static_cast<CCActiveAnimation::TargetProperty>(targetProperty));
}

void WebLayerImpl::pauseAnimation(int animationId, double timeOffset)
{
    m_layer->pauseAnimation(animationId, timeOffset);
}

void WebLayerImpl::suspendAnimations(double monotonicTime)
{
    m_layer->suspendAnimations(monotonicTime);
}

void WebLayerImpl::resumeAnimations(double monotonicTime)
{
    m_layer->resumeAnimations(monotonicTime);
}

bool WebLayerImpl::hasActiveAnimation()
{
    return m_layer->hasActiveAnimation();
}

void WebLayerImpl::transferAnimationsTo(WebLayer* other)
{
    ASSERT(other);
    static_cast<WebLayerImpl*>(other)->m_layer->setLayerAnimationController(m_layer->releaseLayerAnimationController());
}

void WebLayerImpl::setForceRenderSurface(bool forceRenderSurface)
{
    m_layer->setForceRenderSurface(forceRenderSurface);
}

void WebLayerImpl::setScrollPosition(WebPoint position)
{
    m_layer->setScrollPosition(position);
}

void WebLayerImpl::setScrollable(bool scrollable)
{
    m_layer->setScrollable(scrollable);
}

void WebLayerImpl::setHaveWheelEventHandlers(bool haveWheelEventHandlers)
{
    m_layer->setHaveWheelEventHandlers(haveWheelEventHandlers);
}

void WebLayerImpl::setShouldScrollOnMainThread(bool shouldScrollOnMainThread)
{
    m_layer->setShouldScrollOnMainThread(shouldScrollOnMainThread);
}

void WebLayerImpl::setNonFastScrollableRegion(const WebVector<WebRect>& rects)
{
    WebCore::Region region;
    for (size_t i = 0; i < rects.size(); ++i) {
        WebCore::IntRect rect = rects[i];
        region.unite(rect);
    }
    m_layer->setNonFastScrollableRegion(region);

}

void WebLayerImpl::setIsContainerForFixedPositionLayers(bool enable)
{
    m_layer->setIsContainerForFixedPositionLayers(enable);
}

void WebLayerImpl::setFixedToContainerLayer(bool enable)
{
    m_layer->setFixedToContainerLayer(enable);
}

LayerChromium* WebLayerImpl::layer() const
{
    return m_layer.get();
}

} // namespace WebKit
