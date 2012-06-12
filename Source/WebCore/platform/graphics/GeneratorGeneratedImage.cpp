/*
 * Copyright (C) 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GeneratorGeneratedImage.h"

#include "FloatRect.h"
#include "GraphicsContext.h"
#include "Length.h"

namespace WebCore {

void GeneratorGeneratedImage::draw(GraphicsContext* destContext, const FloatRect& destRect, const FloatRect& srcRect, ColorSpace styleColorSpace, CompositeOperator compositeOp)
{
    unsigned generatorHash = m_generator->hash();
    if (!m_cachedImageBuffer || m_cachedGeneratorHash != generatorHash || m_cachedAdjustedSize != m_size || !destContext->isCompatibleWithBuffer(m_cachedImageBuffer.get())) {
        // Create a BitmapImage and call draw on it.
        m_cachedImageBuffer = destContext->createCompatibleBuffer(m_size);
        if (!m_cachedImageBuffer)
            return;

        // Fill with the generated image.
        m_cachedImageBuffer->context()->fillRect(FloatRect(FloatPoint(), m_size), *m_generator);

        m_cachedGeneratorHash = generatorHash;
        m_cachedAdjustedSize = m_size;
    }

    // Draw the image buffer to the destination
    m_cachedImageBuffer->draw(destContext, styleColorSpace, destRect, srcRect, compositeOp);
    m_cacheTimer.restart();
}

void GeneratorGeneratedImage::drawPattern(GraphicsContext* destContext, const FloatRect& srcRect, const AffineTransform& patternTransform,
                                 const FloatPoint& phase, ColorSpace styleColorSpace, CompositeOperator compositeOp, const FloatRect& destRect)
{
    // Allow the generator to provide visually-equivalent tiling parameters for better performance.
    IntSize adjustedSize = m_size;
    FloatRect adjustedSrcRect = srcRect;
    m_generator->adjustParametersForTiledDrawing(adjustedSize, adjustedSrcRect);

    // Factor in the destination context's scale to generate at the best resolution
    AffineTransform destContextCTM = destContext->getCTM();
    double xScale = fabs(destContextCTM.xScale());
    double yScale = fabs(destContextCTM.yScale());
    AffineTransform adjustedPatternCTM = patternTransform;
    adjustedPatternCTM.scale(1.0 / xScale, 1.0 / yScale);
    adjustedSrcRect.scale(xScale, yScale);

    unsigned generatorHash = m_generator->hash();

    if (!m_cachedImageBuffer || m_cachedGeneratorHash != generatorHash || m_cachedAdjustedSize != adjustedSize || !destContext->isCompatibleWithBuffer(m_cachedImageBuffer.get())) {
        m_cachedImageBuffer = destContext->createCompatibleBuffer(adjustedSize);
        if (!m_cachedImageBuffer)
            return;

        // Fill with the generated image.
        m_cachedImageBuffer->context()->fillRect(FloatRect(FloatPoint(), adjustedSize), *m_generator);

        m_cachedGeneratorHash = generatorHash;
        m_cachedAdjustedSize = adjustedSize;
    }

    // Tile the image buffer into the context.
    m_cachedImageBuffer->drawPattern(destContext, adjustedSrcRect, adjustedPatternCTM, phase, styleColorSpace, compositeOp, destRect);
    m_cacheTimer.restart();
}

void GeneratedImage::computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
    Image::computeIntrinsicDimensions(intrinsicWidth, intrinsicHeight, intrinsicRatio);
    intrinsicRatio = FloatSize();
}

void GeneratorGeneratedImage::invalidateCacheTimerFired(DeferrableOneShotTimer<GeneratorGeneratedImage>*)
{
    m_cachedImageBuffer.clear();
    m_cachedAdjustedSize = IntSize();
}

}
