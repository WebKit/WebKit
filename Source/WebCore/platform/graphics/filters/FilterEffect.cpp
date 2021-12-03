/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2012 University of Szeged
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
#include "ImageBuffer.h"
#include "Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

FloatRect FilterEffect::calculateImageRect(const Filter& filter, const FilterImageVector& inputs, const FloatRect& primitiveSubregion) const
{
    FloatRect imageRect;
    for (auto& input : inputs)
        imageRect.unite(input->imageRect());
    return filter.clipToMaxEffectRect(imageRect, primitiveSubregion);
}

FloatRect FilterEffect::determineFilterPrimitiveSubregion(const Filter& filter)
{
    // This function implements https://www.w3.org/TR/filter-effects-1/#FilterPrimitiveSubRegion.
    FloatRect primitiveSubregion;

    // If there is no input effects, take the effect boundaries as unite rect.
    if (!m_inputEffects.isEmpty()) {
        for (auto& effect : m_inputEffects) {
            auto inputPrimitiveSubregion = effect->determineFilterPrimitiveSubregion(filter);
            primitiveSubregion.unite(inputPrimitiveSubregion);
        }
    } else
        primitiveSubregion = filter.filterRegion();

    // Don't use the input's subregion for FETile.
    if (filterType() == FilterEffect::Type::FETile)
        primitiveSubregion = filter.filterRegion();
    
    // Clip the primitive subregion to the effect geometry.
    if (auto geometry = filter.effectGeometry(*this)) {
        if (auto x = geometry->x())
            primitiveSubregion.setX(*x);
        if (auto y = geometry->y())
            primitiveSubregion.setY(*y);
        if (auto width = geometry->width())
            primitiveSubregion.setWidth(*width);
        if (auto height = geometry->height())
            primitiveSubregion.setHeight(*height);
    }

    // Clip every filter effect to the filter region.
    auto absoluteMaxEffectRect = filter.maxEffectRect(primitiveSubregion);
    absoluteMaxEffectRect.scale(filter.filterScale());

    setFilterPrimitiveSubregion(primitiveSubregion);
    setMaxEffectRect(absoluteMaxEffectRect);

    return primitiveSubregion;
}

FilterEffect* FilterEffect::inputEffect(unsigned number) const
{
    ASSERT_WITH_SECURITY_IMPLICATION(number < m_inputEffects.size());
    return m_inputEffects.at(number).get();
}

bool FilterEffect::apply(const Filter& filter)
{
    if (hasResult())
        return true;

    unsigned size = m_inputEffects.size();
    for (unsigned i = 0; i < size; ++i) {
        FilterEffect* in = m_inputEffects.at(i).get();

        // Convert input results to the current effect's color space.
        ASSERT(in->hasResult());
        transformResultColorSpace(in, i);
    }

    if (!mayProduceInvalidPremultipliedPixels()) {
        for (auto& in : m_inputEffects)
            in->correctPremultipliedResultIfNeeded();
    }

    auto inputFilterImages = this->inputFilterImages();
    auto imageRect = calculateImageRect(filter, inputFilterImages, m_filterPrimitiveSubregion);
    auto absoluteImageRect = enclosingIntRect(filter.scaledByFilterScale(imageRect));

    if (absoluteImageRect.isEmpty() || ImageBuffer::sizeNeedsClamping(absoluteImageRect.size()))
        return false;
    
    auto isAlphaImage = resultIsAlphaImage(inputFilterImages);
    auto imageColorSpace = resultColorSpace();

    m_filterImage = FilterImage::create(m_filterPrimitiveSubregion, imageRect, absoluteImageRect, isAlphaImage, filter.renderingMode(), imageColorSpace);
    if (!m_filterImage)
        return false;

    auto applier = createApplier(filter);
    if (!applier)
        return false;

    LOG_WITH_STREAM(Filters, stream
        << "FilterEffect " << filterName() << " " << this << " apply():"
        << "\n  filterPrimitiveSubregion " << m_filterPrimitiveSubregion
        << "\n  absolutePaintRect " << absoluteImageRect
        << "\n  maxEffectRect " << m_maxEffectRect
        << "\n  filter scale " << filter.filterScale());

    return applier->apply(filter, inputFilterImages, *m_filterImage);
}

void FilterEffect::clearResult()
{
    m_filterImage = nullptr;
}

void FilterEffect::clearResultsRecursive()
{
    // Clear all results, regardless that the current effect has
    // a result. Can be used if an effect is in an erroneous state.
    clearResult();
    for (auto& effect : m_inputEffects)
        effect->clearResultsRecursive();
}

FilterImageVector FilterEffect::inputFilterImages() const
{
    FilterImageVector filterImages;

    for (auto& inputEffect : m_inputEffects)
        filterImages.append(*inputEffect->filterImage());

    return filterImages;
}

void FilterEffect::correctPremultipliedResultIfNeeded()
{
    if (!hasResult() || !mayProduceInvalidPremultipliedPixels())
        return;
    m_filterImage->correctPremultipliedPixelBuffer();
}

void FilterEffect::transformResultColorSpace(const DestinationColorSpace& destinationColorSpace)
{
    if (!hasResult())
        return;
    m_filterImage->transformToColorSpace(destinationColorSpace);
}

TextStream& FilterEffect::externalRepresentation(TextStream& ts, RepresentationType representationType) const
{
    // FIXME: We should dump the subRegions of the filter primitives here later. This isn't
    // possible at the moment, because we need more detailed informations from the target object.
    
    if (representationType == RepresentationType::Debugging) {
        TextStream::IndentScope indentScope(ts);
        ts.dumpProperty("operating colorspace", operatingColorSpace());
        ts.dumpProperty("result colorspace", resultColorSpace());
        ts << "\n" << indent;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const FilterEffect& filter)
{
    // Use a new stream because we want multiline mode for logging filters.
    TextStream filterStream;
    filter.externalRepresentation(filterStream, FilterEffect::RepresentationType::Debugging);
    
    return ts << filterStream.release();
}

} // namespace WebCore
