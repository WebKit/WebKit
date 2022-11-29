/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2012 University of Szeged
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "FilterEffect.h"

#include "Filter.h"
#include "FilterEffectApplier.h"
#include "FilterEffectGeometry.h"
#include "FilterResults.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

FilterImageVector FilterEffect::takeImageInputs(FilterImageVector& stack) const
{
    unsigned inputsSize = numberOfImageInputs();
    ASSERT(stack.size() >= inputsSize);
    if (!inputsSize)
        return { };

    Vector<Ref<FilterImage>> inputs;
    inputs.reserveInitialCapacity(inputsSize);

    for (; inputsSize; --inputsSize)
        inputs.uncheckedAppend(stack.takeLast());

    return inputs;
}

static Vector<FloatRect> inputPrimitiveSubregions(const FilterImageVector& inputs)
{
    return inputs.map([](auto& input) {
        return input->primitiveSubregion();
    });
}

static Vector<FloatRect> inputImageRects(const FilterImageVector& inputs)
{
    return inputs.map([] (auto& input) {
        return input->imageRect();
    });
}

FloatRect FilterEffect::calculatePrimitiveSubregion(const Filter& filter, Span<const FloatRect> inputPrimitiveSubregions, const std::optional<FilterEffectGeometry>& geometry) const
{
    // This function implements https://www.w3.org/TR/filter-effects-1/#FilterPrimitiveSubRegion.
    FloatRect primitiveSubregion;

    // If there is no input effects, take the effect boundaries as unite rect. Don't use the input's subregion for FETile.
    if (!inputPrimitiveSubregions.empty() && filterType() != FilterEffect::Type::FETile) {
        for (auto& inputPrimitiveSubregion : inputPrimitiveSubregions)
            primitiveSubregion.unite(inputPrimitiveSubregion);
    } else
        primitiveSubregion = filter.filterRegion();

    // Clip the primitive subregion to the effect geometry.
    if (geometry) {
        if (auto x = geometry->x())
            primitiveSubregion.setX(*x);
        if (auto y = geometry->y())
            primitiveSubregion.setY(*y);
        if (auto width = geometry->width())
            primitiveSubregion.setWidth(*width);
        if (auto height = geometry->height())
            primitiveSubregion.setHeight(*height);
    }

    return primitiveSubregion;
}

FloatRect FilterEffect::calculateImageRect(const Filter& filter, Span<const FloatRect> inputImageRects, const FloatRect& primitiveSubregion) const
{
    FloatRect imageRect;
    for (auto& inputImageRect : inputImageRects)
        imageRect.unite(inputImageRect);
    return filter.clipToMaxEffectRect(imageRect, primitiveSubregion);
}

std::unique_ptr<FilterEffectApplier> FilterEffect::createApplier(const Filter& filter) const
{
    if (filter.filterRenderingMode() == FilterRenderingMode::Accelerated)
        return createAcceleratedApplier();

    if (filter.filterRenderingMode() == FilterRenderingMode::Software)
        return createSoftwareApplier();

    ASSERT_NOT_REACHED();
    return nullptr;
}

void FilterEffect::transformInputsColorSpace(const FilterImageVector& inputs) const
{
    for (auto& input : inputs)
        input->transformToColorSpace(operatingColorSpace());
}

void FilterEffect::correctPremultipliedInputs(const FilterImageVector& inputs) const
{
    // Correct any invalid pixels, if necessary, in the result of a filter operation.
    // This method is used to ensure valid pixel values on filter inputs and the final result.
    // Only the arithmetic composite filter ever needs to perform correction.
    for (auto& input : inputs)
        input->correctPremultipliedPixelBuffer();
}

RefPtr<FilterImage> FilterEffect::apply(const Filter& filter, FilterImage& input, FilterResults& results)
{
    return apply(filter, FilterImageVector { Ref { input } }, results);
}

RefPtr<FilterImage> FilterEffect::apply(const Filter& filter, const FilterImageVector& inputs, FilterResults& results, const std::optional<FilterEffectGeometry>& geometry)
{
    ASSERT(inputs.size() == numberOfImageInputs());

    if (auto result = results.effectResult(*this))
        return result;

    auto primitiveSubregion = calculatePrimitiveSubregion(filter, inputPrimitiveSubregions(inputs), geometry);
    auto imageRect = calculateImageRect(filter, inputImageRects(inputs), primitiveSubregion);
    auto absoluteImageRect = enclosingIntRect(filter.scaledByFilterScale(imageRect));

    if (absoluteImageRect.isEmpty() || ImageBuffer::sizeNeedsClamping(absoluteImageRect.size()))
        return nullptr;

    auto applier = createApplier(filter);
    if (!applier)
        return nullptr;

    auto isAlphaImage = resultIsAlphaImage(inputs);
    auto isValidPremultiplied = resultIsValidPremultiplied();
    auto imageColorSpace = resultColorSpace(inputs);

    auto result = FilterImage::create(primitiveSubregion, imageRect, absoluteImageRect, isAlphaImage, isValidPremultiplied, filter.renderingMode(), imageColorSpace, results.allocator());
    if (!result)
        return nullptr;

    LOG_WITH_STREAM(Filters, stream
        << "FilterEffect " << filterName() << " " << this << " apply(): " << *this
        << "\n  filterPrimitiveSubregion " << primitiveSubregion
        << "\n  absolutePaintRect " << absoluteImageRect
        << "\n  maxEffectRect " << filter.maxEffectRect(primitiveSubregion)
        << "\n  filter scale " << filter.filterScale());

    transformInputsColorSpace(inputs);
    if (isValidPremultiplied)
        correctPremultipliedInputs(inputs);

    if (!applier->apply(filter, inputs, *result))
        return nullptr;

    results.setEffectResult(*this, inputs, { *result });
    return result;
}

FilterStyleVector FilterEffect::createFilterStyles(const Filter& filter, const FilterStyle& input) const
{
    return { createFilterStyle(filter, input) };
}

FilterStyle FilterEffect::createFilterStyle(const Filter& filter, const FilterStyle& input, const std::optional<FilterEffectGeometry>& geometry) const
{
    ASSERT(supportedFilterRenderingModes().contains(FilterRenderingMode::GraphicsContext));

    auto primitiveSubregion = calculatePrimitiveSubregion(filter, { &input.primitiveSubregion, 1 }, geometry);
    auto imageRect = calculateImageRect(filter, { &input.imageRect, 1 }, primitiveSubregion);

    auto style = createGraphicsStyle(filter);
    return FilterStyle { style, primitiveSubregion, imageRect };
}

TextStream& FilterEffect::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    // FIXME: We should dump the subRegions of the filter primitives here later. This isn't
    // possible at the moment, because we need more detailed informations from the target object.
    
    if (representation == FilterRepresentation::Debugging) {
        TextStream::IndentScope indentScope(ts);
        ts.dumpProperty("operating colorspace", operatingColorSpace());
        ts << "\n" << indent;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const FilterEffect& effect)
{
    // Use a new stream because we want multiline mode for logging filters.
    TextStream filterStream;
    effect.externalRepresentation(filterStream, FilterRepresentation::Debugging);
    
    return ts << filterStream.release();
}

} // namespace WebCore
