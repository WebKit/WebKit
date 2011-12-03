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
#include "platform/WebLayer.h"

#include "platform/WebFloatPoint.h"
#include "Color.h"
#include "LayerChromium.h"
#include "SkMatrix44.h"
#include "TransformationMatrix.h"
#include "WebLayerImpl.h"
#include "WebSize.h"

using namespace WebCore;

namespace {

TransformationMatrix transformationMatrixFromSkMatrix44(const SkMatrix44& matrix)
{
    double data[16];
    matrix.asColMajord(data);
    return TransformationMatrix(data[0], data[1], data[2], data[3],
                                data[4], data[5], data[6], data[7],
                                data[8], data[9], data[10], data[11],
                                data[12], data[13], data[14], data[15]);
}

SkMatrix44 skMatrix44FromTransformationMatrix(const TransformationMatrix& matrix)
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

namespace WebKit {

WebLayer WebLayer::create(WebLayerClient* client)
{
    return WebLayer(WebLayerImpl::create(client));
}

void WebLayer::reset()
{
    m_private.reset();
}

void WebLayer::assign(const WebLayer& other)
{
    m_private = other.m_private;
}

bool WebLayer::equals(const WebLayer& n) const
{
    return (m_private.get() == n.m_private.get());
}

WebLayer WebLayer::rootLayer() const
{
    return WebLayer(const_cast<LayerChromium*>(m_private->rootLayer()));
}

WebLayer WebLayer::parent() const
{
    return WebLayer(const_cast<LayerChromium*>(m_private->parent()));
}

void WebLayer::addChild(const WebLayer& child)
{
    m_private->addChild(child);
}

void WebLayer::insertChild(const WebLayer& child, size_t index)
{
    m_private->insertChild(child, index);
}

void WebLayer::replaceChild(const WebLayer& reference, const WebLayer& newLayer)
{
    WebLayer ref = reference;
    m_private->replaceChild(ref.unwrap<LayerChromium>(), newLayer);
}

void WebLayer::removeFromParent()
{
    m_private->removeFromParent();
}

void WebLayer::removeAllChildren()
{
    m_private->removeAllChildren();
}

void WebLayer::setAnchorPoint(const WebFloatPoint& anchorPoint)
{
    m_private->setAnchorPoint(anchorPoint);
}

WebFloatPoint WebLayer::anchorPoint() const
{
    return WebFloatPoint(m_private->anchorPoint());
}

void WebLayer::setAnchorPointZ(float anchorPointZ)
{
    m_private->setAnchorPointZ(anchorPointZ);
}

float WebLayer::anchorPointZ() const
{
    return m_private->anchorPointZ();
}

void WebLayer::setBounds(const WebSize& size)
{
    m_private->setBounds(size);
}

WebSize WebLayer::bounds() const
{
    return WebSize(m_private->bounds());
}

void WebLayer::setMasksToBounds(bool masksToBounds)
{
    m_private->setMasksToBounds(masksToBounds);
}

bool WebLayer::masksToBounds() const
{
    return m_private->masksToBounds();
}

void WebLayer::setMaskLayer(const WebLayer& maskLayer)
{
    WebLayer ref = maskLayer;
    return m_private->setMaskLayer(ref.unwrap<LayerChromium>());
}

WebLayer WebLayer::maskLayer() const
{
    return WebLayer(m_private->maskLayer());
}

void WebLayer::setOpacity(float opacity)
{
    m_private->setOpacity(opacity);
}

float WebLayer::opacity() const
{
    return m_private->opacity();
}

void WebLayer::setOpaque(bool opaque)
{
    m_private->setOpaque(opaque);
}

bool WebLayer::opaque() const
{
    return m_private->opaque();
}

void WebLayer::setPosition(const WebFloatPoint& position)
{
    m_private->setPosition(position);
}

WebFloatPoint WebLayer::position() const
{
    return WebFloatPoint(m_private->position());
}

void WebLayer::setSublayerTransform(const SkMatrix44& matrix)
{
    m_private->setSublayerTransform(transformationMatrixFromSkMatrix44(matrix));
}

SkMatrix44 WebLayer::sublayerTransform() const
{
    return skMatrix44FromTransformationMatrix(m_private->sublayerTransform());
}

void WebLayer::setTransform(const SkMatrix44& matrix)
{
    m_private->setTransform(transformationMatrixFromSkMatrix44(matrix));
}

SkMatrix44 WebLayer::transform() const
{
    return skMatrix44FromTransformationMatrix(m_private->transform());
}

void WebLayer::setDebugBorderColor(const WebColor& color)
{
    m_private->setDebugBorderColor(color);
}

void WebLayer::setDebugBorderWidth(float width)
{
    m_private->setDebugBorderWidth(width);
}

WebLayer::WebLayer(const PassRefPtr<LayerChromium>& node)
    : m_private(node)
{
}

WebLayer& WebLayer::operator=(const PassRefPtr<LayerChromium>& node)
{
    m_private = node;
    return *this;
}

WebLayer::operator PassRefPtr<LayerChromium>() const
{
    return m_private.get();
}

} // namespace WebKit
