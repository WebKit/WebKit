/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "GraphicsContextImpl.h"

namespace WebCore {

GraphicsContextImpl::GraphicsContextImpl(GraphicsContext& context, const FloatRect&, const AffineTransform&)
    : m_graphicsContext(context)
{
}

GraphicsContextImpl::~GraphicsContextImpl()
{
}

ImageDrawResult GraphicsContextImpl::drawImageImpl(GraphicsContext& context, Image& image, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& options)
{
    InterpolationQualityMaintainer interpolationQualityForThisScope(context, options.interpolationQuality());
    return image.draw(context, destination, source, options);
}

ImageDrawResult GraphicsContextImpl::drawTiledImageImpl(GraphicsContext& context, Image& image, const FloatRect& destination, const FloatPoint& source, const FloatSize& tileSize, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    InterpolationQualityMaintainer interpolationQualityForThisScope(context, options.interpolationQuality());
    return image.drawTiled(context, destination, source, tileSize, spacing, options);
}

ImageDrawResult GraphicsContextImpl::drawTiledImageImpl(GraphicsContext& context, Image& image, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor, Image::TileRule hRule, Image::TileRule vRule, const ImagePaintingOptions& options)
{
    if (hRule == Image::StretchTile && vRule == Image::StretchTile) {
        // Just do a scale.
        return drawImageImpl(context, image, destination, source, options);
    }

    InterpolationQualityMaintainer interpolationQualityForThisScope(context, options.interpolationQuality());
    return image.drawTiled(context, destination, source, tileScaleFactor, hRule, vRule, options.compositeOperator());
}

} // namespace WebCore
