/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "LinkHighlightLayerDelegate.h"

#include "GraphicsLayerChromium.h"
#include "cc/CCKeyframedAnimationCurve.h"
#include <wtf/CurrentTime.h>
#include <wtf/OwnPtr.h>

#if USE(ACCELERATED_COMPOSITING)

namespace WebCore {

PassRefPtr<LinkHighlightLayerDelegate> LinkHighlightLayerDelegate::create(GraphicsLayerChromium* parent, const Path& path, int animationId, int groupId)
{
    return adoptRef(new LinkHighlightLayerDelegate(parent, path, animationId, groupId));
}

LinkHighlightLayerDelegate::LinkHighlightLayerDelegate(GraphicsLayerChromium* parent, const Path& path, int animationId, int groupId)
    : m_contentLayer(ContentLayerChromium::create(this))
    , m_parent(parent)
    , m_path(path)
{
    m_contentLayer->setIsDrawable(true);

    IntRect rect = enclosingIntRect(path.boundingRect());

    m_contentLayer->setBounds(rect.size());

    TransformationMatrix transform;
    transform.translate(rect.x() + rect.width() / 2, rect.y() + rect.height() / 2);
    m_contentLayer->setTransform(transform);

    m_path.translate(FloatSize(-rect.x(), -rect.y()));

    m_contentLayer->setLayerAnimationDelegate(this);

    // FIXME: Should these be configurable?
    const float startOpacity = 0.25;
    const float duration = 2;

    m_contentLayer->setOpacity(startOpacity);

    OwnPtr<CCKeyframedFloatAnimationCurve> curve(CCKeyframedFloatAnimationCurve::create());
    curve->addKeyframe(CCFloatKeyframe::create(0, startOpacity, nullptr));
    curve->addKeyframe(CCFloatKeyframe::create(duration / 2, startOpacity, nullptr));
    curve->addKeyframe(CCFloatKeyframe::create(duration, 0, nullptr));

    // animationId = 1 is reserved for us.
    OwnPtr<CCActiveAnimation> animation(CCActiveAnimation::create(curve.release(), animationId, groupId, CCActiveAnimation::Opacity));
    animation->setNeedsSynchronizedStartTime(true);
    m_contentLayer->layerAnimationController()->add(animation.release());
}

LinkHighlightLayerDelegate::~LinkHighlightLayerDelegate()
{
    m_contentLayer->removeFromParent();
    m_contentLayer->clearDelegate();
}

ContentLayerChromium* LinkHighlightLayerDelegate::getContentLayer()
{
    return m_contentLayer.get();
}

void LinkHighlightLayerDelegate::paintContents(GraphicsContext& gc, const IntRect& clipRect)
{
    // FIXME: make colour configurable?
    gc.setStrokeColor(Color(0, 0, 255, 255), ColorSpaceDeviceRGB);
    gc.setStrokeThickness(2);
    gc.strokePath(m_path);
    gc.setFillColor(Color(255, 0, 255, 255), ColorSpaceDeviceRGB);
    gc.fillPath(m_path);
}

void LinkHighlightLayerDelegate::didScroll(const IntSize&)
{
}

void LinkHighlightLayerDelegate::notifyAnimationStarted(double time)
{
}

void LinkHighlightLayerDelegate::notifyAnimationFinished(double time)
{
    // Allow null parent pointer to facilitate testing.
    if (m_parent)
        m_parent->didFinishLinkHighlightLayer();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
