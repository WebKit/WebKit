/*
 * Copyright (C) 2021 Apple Inc.  All rights reserved.
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
#include "Filter.h"

#include "FilterEffect.h"
#include "FilterImage.h"
#include "ImageBuffer.h"

namespace WebCore {

Filter::Filter(Filter::Type filterType, RenderingMode renderingMode, const FloatSize& filterScale, ClipOperation clipOperation)
    : FilterFunction(filterType)
    , m_renderingMode(renderingMode)
    , m_filterScale(filterScale)
    , m_clipOperation(clipOperation)
{
}

Filter::Filter(Filter::Type filterType, RenderingMode renderingMode, const FloatSize& filterScale, const FloatRect& sourceImageRect, const FloatRect& filterRegion, ClipOperation clipOperation)
    : FilterFunction(filterType)
    , m_renderingMode(renderingMode)
    , m_filterScale(filterScale)
    , m_sourceImageRect(sourceImageRect)
    , m_filterRegion(filterRegion)
    , m_clipOperation(clipOperation)
{
}

FloatPoint Filter::scaledByFilterScale(const FloatPoint& point) const
{
    return point.scaled(m_filterScale.width(), m_filterScale.height());
}

FloatSize Filter::scaledByFilterScale(const FloatSize& size) const
{
    return size * m_filterScale;
}

FloatRect Filter::scaledByFilterScale(const FloatRect& rect) const
{
    auto scaledRect = rect;
    scaledRect.scale(m_filterScale);
    return scaledRect;
}

FloatRect Filter::maxEffectRect(const FloatRect& primitiveSubregion) const
{
    return intersection(primitiveSubregion, m_filterRegion);
}

FloatRect Filter::clipToMaxEffectRect(const FloatRect& imageRect, const FloatRect& primitiveSubregion) const
{
    auto maxEffectRect = this->maxEffectRect(primitiveSubregion);
    return m_clipOperation == ClipOperation::Intersect ? intersection(imageRect, maxEffectRect) : unionRect(imageRect, maxEffectRect);
}

bool Filter::clampFilterRegionIfNeeded()
{
    auto lastEffect = this->lastEffect();
    lastEffect->determineFilterPrimitiveSubregion(*this);
    
    auto maxEffectRect = this->maxEffectRect(lastEffect->filterPrimitiveSubregion());
    auto scaledMaxEffectRect = scaledByFilterScale(maxEffectRect);

    FloatSize clampingScale(1, 1);
    if (!ImageBuffer::sizeNeedsClamping(scaledMaxEffectRect.size(), clampingScale))
        return false;

    m_filterScale = m_filterScale * clampingScale;

    // At least one FilterEffect has a too big image size,
    // recalculate the effect sizes with new scale factors.
    lastEffect->determineFilterPrimitiveSubregion(*this);
    return true;
}

RefPtr<FilterImage> Filter::apply(ImageBuffer* sourceImage, const FloatRect& sourceImageRect)
{
    setSourceImage(sourceImage);
    setSourceImageRect(sourceImageRect);

    auto result = apply();
    if (!result)
        return { };

    result->transformToColorSpace(DestinationColorSpace::SRGB());
    return result;
}

} // namespace WebCore
