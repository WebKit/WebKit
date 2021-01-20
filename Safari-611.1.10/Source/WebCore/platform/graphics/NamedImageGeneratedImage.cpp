/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "NamedImageGeneratedImage.h"

#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "Theme.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

NamedImageGeneratedImage::NamedImageGeneratedImage(String name, const FloatSize& size)
    : m_name(name)
{
    setContainerSize(size);
}

ImageDrawResult NamedImageGeneratedImage::draw(GraphicsContext& context, const FloatRect& dstRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
#if USE(NEW_THEME) || PLATFORM(IOS_FAMILY)
    GraphicsContextStateSaver stateSaver(context);
    context.setCompositeOperation(options.compositeOperator(), options.blendMode());
    context.clip(dstRect);
    context.translate(dstRect.location());
    if (dstRect.size() != srcRect.size())
        context.scale(FloatSize(dstRect.width() / srcRect.width(), dstRect.height() / srcRect.height()));
    context.translate(-srcRect.location());

    Theme::singleton().drawNamedImage(m_name, context, dstRect);
    return ImageDrawResult::DidDraw;
#else
    UNUSED_PARAM(context);
    UNUSED_PARAM(dstRect);
    UNUSED_PARAM(srcRect);
    UNUSED_PARAM(options);
    return ImageDrawResult::DidNothing;
#endif
}

void NamedImageGeneratedImage::drawPattern(GraphicsContext& context, const FloatRect& dstRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
#if USE(NEW_THEME)
    auto imageBuffer = ImageBuffer::createCompatibleBuffer(size(), context);
    if (!imageBuffer)
        return;

    GraphicsContext& graphicsContext = imageBuffer->context();
    Theme::singleton().drawNamedImage(m_name, graphicsContext, FloatRect(0, 0, size().width(), size().height()));

    // Tile the image buffer into the context.
    imageBuffer->drawPattern(context, dstRect, srcRect, patternTransform, phase, spacing, options);
#else
    UNUSED_PARAM(context);
    UNUSED_PARAM(dstRect);
    UNUSED_PARAM(srcRect);
    UNUSED_PARAM(patternTransform);
    UNUSED_PARAM(phase);
    UNUSED_PARAM(spacing);
    UNUSED_PARAM(options);
#endif
}

void NamedImageGeneratedImage::dump(TextStream& ts) const
{
    GeneratedImage::dump(ts);
    ts.dumpProperty("name", m_name);
}

}
