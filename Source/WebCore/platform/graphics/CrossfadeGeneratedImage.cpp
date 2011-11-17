/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#include "config.h"
#include "CrossfadeGeneratedImage.h"

#include "CSSCrossfadeValue.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"

using namespace std;

namespace WebCore {

CrossfadeGeneratedImage::CrossfadeGeneratedImage(CachedImage* fromImage, CachedImage* toImage, float percentage, ImageObserver* observer, IntSize crossfadeSize, const IntSize& size)
    : m_fromImage(fromImage)
    , m_toImage(toImage)
    , m_percentage(percentage)
    , m_crossfadeSize(crossfadeSize)
    , m_observer(observer)
    , m_crossfadeSubimageObserver(adoptPtr(new CrossfadeSubimageObserverProxy(this)))
{
    m_size = size;

    m_fromImage->addClient(m_crossfadeSubimageObserver.get());
    m_toImage->addClient(m_crossfadeSubimageObserver.get());

    m_crossfadeSubimageObserver->setReady(true);
}

CrossfadeGeneratedImage::~CrossfadeGeneratedImage()
{
    m_fromImage->removeClient(m_crossfadeSubimageObserver.get());
    m_toImage->removeClient(m_crossfadeSubimageObserver.get());
}

void CrossfadeGeneratedImage::drawCrossfade(GraphicsContext* context)
{
    float inversePercentage = 1 - m_percentage;

    Image* fromImage = m_fromImage->image();
    IntSize fromImageSize = fromImage->size();
    Image* toImage = m_toImage->image();
    IntSize toImageSize = toImage->size();

    // Draw nothing if either of the images hasn't loaded yet.
    if (fromImage == Image::nullImage() || toImage == Image::nullImage())
        return;

    GraphicsContextStateSaver stateSaver(*context);

    context->clip(IntRect(IntPoint(), m_crossfadeSize));
    context->beginTransparencyLayer(1);

    // Draw the image we're fading away from.
    context->save();
    if (m_crossfadeSize != fromImageSize)
        context->scale(FloatSize(static_cast<float>(m_crossfadeSize.width()) / fromImageSize.width(),
                                 static_cast<float>(m_crossfadeSize.height()) / fromImageSize.height()));
    context->setAlpha(inversePercentage);
    context->drawImage(fromImage, ColorSpaceDeviceRGB, IntPoint());
    context->restore();

    // Draw the image we're fading towards.
    context->save();
    if (m_crossfadeSize != toImageSize)
        context->scale(FloatSize(static_cast<float>(m_crossfadeSize.width()) / toImageSize.width(),
                                 static_cast<float>(m_crossfadeSize.height()) / toImageSize.height()));
    context->setAlpha(m_percentage);
    context->drawImage(toImage, ColorSpaceDeviceRGB, IntPoint(), CompositePlusLighter);
    context->restore();

    context->endTransparencyLayer();
}

void CrossfadeGeneratedImage::draw(GraphicsContext* context, const FloatRect& dstRect, const FloatRect& srcRect, ColorSpace, CompositeOperator compositeOp)
{
    UNUSED_PARAM(compositeOp);
    UNUSED_PARAM(srcRect);

    GraphicsContextStateSaver stateSaver(*context);
    context->clip(dstRect);
    context->translate(dstRect.x(), dstRect.y());

    drawCrossfade(context);
}

void CrossfadeGeneratedImage::drawPattern(GraphicsContext* context, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, ColorSpace styleColorSpace, CompositeOperator compositeOp, const FloatRect& destRect)
{
    OwnPtr<ImageBuffer> imageBuffer = ImageBuffer::create(m_size);
    if (!imageBuffer)
        return;

    // Fill with the cross-faded image.
    GraphicsContext* graphicsContext = imageBuffer->context();
    drawCrossfade(graphicsContext);

    // Tile the image buffer into the context.
    imageBuffer->drawPattern(context, srcRect, patternTransform, phase, styleColorSpace, compositeOp, destRect);
}
    
void CrossfadeSubimageObserverProxy::imageChanged(CachedImage* image, const IntRect* rect)
{
    if (m_ready)
        m_ownerValue->imageChanged(image, rect);
}

void CrossfadeGeneratedImage::imageChanged(CachedImage* image, const IntRect* rect)
{
    UNUSED_PARAM(image);
    m_observer->changedInRect(this, *rect);
}

}
