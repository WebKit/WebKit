/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "FilterResults.h"
#include "FilterStyle.h"
#include "ImageBuffer.h"

namespace WebCore {

Filter::Filter(Filter::Type filterType, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier)
    : FilterFunction(filterType, renderingResourceIdentifier)
{
}

Filter::Filter(Filter::Type filterType, const FilterGeometry& geometry, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier)
    : FilterFunction(filterType, renderingResourceIdentifier)
    , m_geometry(geometry)
#if USE(CORE_IMAGE)
    , m_enclosingFilterRegion(geometry.filterRegion)
#endif
{
}

FloatPoint Filter::scaledByFilterScale(const FloatPoint& point) const
{
    return point.scaled(m_geometry.scale.width(), m_geometry.scale.height());
}

FloatSize Filter::scaledByFilterScale(const FloatSize& size) const
{
    return size * m_geometry.scale;
}

FloatRect Filter::scaledByFilterScale(const FloatRect& rect) const
{
    auto scaledRect = rect;
    scaledRect.scale(m_geometry.scale);
    return scaledRect;
}

FloatRect Filter::maxEffectRect(const FloatRect& primitiveSubregion) const
{
    return intersection(primitiveSubregion, m_geometry.filterRegion);
}

FloatRect Filter::clipToMaxEffectRect(const FloatRect& imageRect, const FloatRect& primitiveSubregion) const
{
    auto maxEffectRect = this->maxEffectRect(primitiveSubregion);
    return intersection(imageRect, maxEffectRect);
}

#if USE(CORE_IMAGE)
FloatRect Filter::absoluteEnclosingFilterRegion() const
{
    return scaledByFilterScale(m_enclosingFilterRegion);
}

FloatRect Filter::flippedRectRelativeToAbsoluteEnclosingFilterRegion(const FloatRect& absoluteRect) const
{
    auto absoluteFilterRegion = absoluteEnclosingFilterRegion();
    return FloatRect(absoluteRect.x() - absoluteFilterRegion.x(), absoluteFilterRegion.maxY() - absoluteRect.maxY(), absoluteRect.width(), absoluteRect.height());
}
#endif

bool Filter::clampFilterRegionIfNeeded()
{
    auto scaledFilterRegion = scaledByFilterScale(m_geometry.filterRegion);

    FloatSize clampingScale(1, 1);
    if (!ImageBuffer::sizeNeedsClamping(scaledFilterRegion.size(), clampingScale))
        return false;

    m_geometry.scale = m_geometry.scale * clampingScale;
    return true;
}

RenderingMode Filter::renderingMode() const
{
    if (m_filterRenderingModes.contains(FilterRenderingMode::Accelerated))
        return RenderingMode::Accelerated;
    
    ASSERT(m_filterRenderingModes.contains(FilterRenderingMode::Software));
    return RenderingMode::Unaccelerated;
}

void Filter::setFilterRenderingModes(OptionSet<FilterRenderingMode> preferredFilterRenderingModes)
{
    m_filterRenderingModes = supportedFilterRenderingModes(preferredFilterRenderingModes);
    ASSERT(m_filterRenderingModes.contains(FilterRenderingMode::Software));
}

RefPtr<FilterImage> Filter::apply(ImageBuffer* sourceImage, const FloatRect& sourceImageRect, FilterResults& results)
{
    RefPtr<FilterImage> input;

    if (sourceImage) {
        auto absoluteSourceImageRect = enclosingIntRect(scaledByFilterScale(sourceImageRect));
        input = FilterImage::create(m_geometry.filterRegion, sourceImageRect, absoluteSourceImageRect, Ref { *sourceImage }, results.allocator());
        if (!input)
            return nullptr;
    }

    auto result = apply(input.get(), results);
    if (!result)
        return nullptr;

    result->correctPremultipliedPixelBuffer();
    result->transformToColorSpace(DestinationColorSpace::SRGB());
    return result;
}

FilterStyleVector Filter::createFilterStyles(GraphicsContext& context, const FloatRect& sourceImageRect) const
{
    auto input = FilterStyle { std::nullopt, m_geometry.filterRegion, sourceImageRect };
    auto result = createFilterStyles(context, input);
    if (result.isEmpty())
        return { };

    result.reverse();
    result.shrinkToFit();
    return result;
}

ImageBuffer* Filter::filterResultBuffer(FilterImage& filterImage) const
{
#if USE(CORE_IMAGE)
    return filterImage.filterResultImageBuffer(*this);
#endif

    return filterImage.imageBuffer();
}

} // namespace WebCore
