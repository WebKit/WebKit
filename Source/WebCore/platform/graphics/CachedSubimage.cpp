/*
 * Copyright (C) 2023 Apple Inc.  All rights reserved.
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
#include "CachedSubimage.h"

#include "GeometryUtilities.h"
#include "GraphicsContext.h"

namespace WebCore {

static FloatRect calculateCachedSubimageSourceRect(GraphicsContext& context, const FloatRect& destinationRect, const FloatRect& sourceRect, const FloatRect& imageRect)
{
    auto scaleFactor = destinationRect.size() / sourceRect.size();
    auto effectiveScaleFactor = scaleFactor * context.scaleFactor();

    auto cachedSubimageSourceSize = CachedSubimage::maxSide / effectiveScaleFactor;
    auto cachedSubimageSourceRect = FloatRect { sourceRect.center() - cachedSubimageSourceSize / 2, cachedSubimageSourceSize };

    auto shift = [](const FloatSize& delta) -> FloatSize {
        return FloatSize { std::max(0.0f, -delta.width()), std::max(0.0f, -delta.height()) };
    };

    // Move cachedSubimageSourceRect inside imageRect if needed.
    cachedSubimageSourceRect.move(shift(cachedSubimageSourceRect.location() - imageRect.location()));
    cachedSubimageSourceRect.move(-shift(imageRect.size() - cachedSubimageSourceRect.size()));
    cachedSubimageSourceRect.intersect(imageRect);

    return cachedSubimageSourceRect;
}

std::unique_ptr<CachedSubimage> CachedSubimage::create(GraphicsContext& context, const FloatSize& imageSize, const FloatRect& destinationRect, const FloatRect& sourceRect)
{
    auto cachedSubimageSourceRect = calculateCachedSubimageSourceRect(context, destinationRect, sourceRect, FloatRect { { }, imageSize });
    if (!(roundedIntRect(cachedSubimageSourceRect) == roundedIntRect(sourceRect) || cachedSubimageSourceRect.contains(sourceRect)))
        return nullptr;

    auto cachedSubimageDestinationRect = mapRect(cachedSubimageSourceRect, sourceRect, destinationRect);

    auto imageBuffer = context.createScaledImageBuffer(cachedSubimageDestinationRect, context.scaleFactor(), DestinationColorSpace::SRGB(), RenderingMode::Unaccelerated, RenderingMethod::Local);
    if (!imageBuffer)
        return nullptr;

    return makeUnique<CachedSubimage>(imageBuffer.releaseNonNull(), context.scaleFactor(), cachedSubimageDestinationRect, cachedSubimageSourceRect);
}

std::unique_ptr<CachedSubimage> CachedSubimage::createPixelated(GraphicsContext& context, const FloatRect& destinationRect, const FloatRect& sourceRect)
{
    auto imageBuffer = context.createScaledImageBuffer(destinationRect, context.scaleFactor(), DestinationColorSpace::SRGB(), RenderingMode::Unaccelerated, RenderingMethod::Local);
    if (!imageBuffer)
        return nullptr;

    return makeUnique<CachedSubimage>(imageBuffer.releaseNonNull(), context.scaleFactor(), destinationRect, sourceRect);
}

CachedSubimage::CachedSubimage(Ref<ImageBuffer>&& imageBuffer, const FloatSize& scaleFactor, const FloatRect& destinationRect, const FloatRect& sourceRect)
    : m_imageBuffer(WTFMove(imageBuffer))
    , m_scaleFactor(scaleFactor)
    , m_destinationRect(destinationRect)
    , m_sourceRect(sourceRect)
{
}

bool CachedSubimage::canBeUsed(GraphicsContext& context, const FloatRect& destinationRect, const FloatRect& sourceRect) const
{
    if (context.scaleFactor() != m_scaleFactor)
        return false;

    if (!areEssentiallyEqual(destinationRect.size() / sourceRect.size(), m_destinationRect.size() / m_sourceRect.size()))
        return false;

    return m_sourceRect.contains(sourceRect);
}

void CachedSubimage::draw(GraphicsContext& context, const FloatRect& destinationRect, const FloatRect& sourceRect)
{
    ASSERT(canBeUsed(context, destinationRect, sourceRect));

    auto sourceRectScaled = sourceRect;
    sourceRectScaled.move(toFloatSize(-m_sourceRect.location()));

    auto scaleFactor = destinationRect.size() / sourceRect.size();
    sourceRectScaled.scale(scaleFactor * context.scaleFactor());

    context.drawImageBuffer(m_imageBuffer.get(), destinationRect, sourceRectScaled, { });
}

} // namespace WebCore
