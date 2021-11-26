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

#include "Color.h"
#include "Filter.h"
#include "FilterEffectApplier.h"
#include "GeometryUtilities.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

void FilterEffect::determineAbsolutePaintRect(const Filter&)
{
    m_absolutePaintRect = IntRect();
    for (auto& effect : m_inputEffects)
        m_absolutePaintRect.unite(effect->absolutePaintRect());
    clipAbsolutePaintRect();
}

void FilterEffect::clipAbsolutePaintRect()
{
    // Filters in SVG clip to primitive subregion, while CSS doesn't.
    if (m_clipsToBounds)
        m_absolutePaintRect.intersect(enclosingIntRect(m_maxEffectRect));
    else
        m_absolutePaintRect.unite(enclosingIntRect(m_maxEffectRect));
}

FloatPoint FilterEffect::mapPointFromUserSpaceToBuffer(FloatPoint userSpacePoint) const
{
    FloatPoint absolutePoint = mapPoint(userSpacePoint, m_filterPrimitiveSubregion, m_absoluteUnclippedSubregion);
    absolutePoint.moveBy(-m_absolutePaintRect.location());
    return absolutePoint;
}

FloatRect FilterEffect::determineFilterPrimitiveSubregion(const Filter& filter)
{
    // FETile, FETurbulence, FEFlood don't have input effects, take the filter region as unite rect.
    FloatRect subregion;
    if (unsigned numberOfInputEffects = inputEffects().size()) {
        subregion = inputEffect(0)->determineFilterPrimitiveSubregion(filter);
        for (unsigned i = 1; i < numberOfInputEffects; ++i) {
            auto inputPrimitiveSubregion = inputEffect(i)->determineFilterPrimitiveSubregion(filter);
            subregion.unite(inputPrimitiveSubregion);
        }
    } else
        subregion = filter.filterRegion();

    // After calling determineFilterPrimitiveSubregion on the target effect, reset the subregion again for <feTile>.
    if (filterType() == FilterEffect::Type::FETile)
        subregion = filter.filterRegion();

    auto boundaries = effectBoundaries();
    if (hasX())
        subregion.setX(boundaries.x());
    if (hasY())
        subregion.setY(boundaries.y());
    if (hasWidth())
        subregion.setWidth(boundaries.width());
    if (hasHeight())
        subregion.setHeight(boundaries.height());

    setFilterPrimitiveSubregion(subregion);

    auto absoluteSubregion = subregion;
    absoluteSubregion.scale(filter.filterScale());
    // Save this before clipping so we can use it to map lighting points from user space to buffer coordinates.
    setUnclippedAbsoluteSubregion(absoluteSubregion);

    // Clip every filter effect to the filter region.
    auto absoluteScaledFilterRegion = filter.filterRegion();
    absoluteScaledFilterRegion.scale(filter.filterScale());
    absoluteSubregion.intersect(absoluteScaledFilterRegion);

    setMaxEffectRect(absoluteSubregion);
    return subregion;
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

    determineAbsolutePaintRect(filter);

    LOG_WITH_STREAM(Filters, stream
        << "FilterEffect " << filterName() << " " << this << " apply():"
        << "\n  filterPrimitiveSubregion " << m_filterPrimitiveSubregion
        << "\n  effectBoundaries " << m_effectBoundaries
        << "\n  absoluteUnclippedSubregion " << m_absoluteUnclippedSubregion
        << "\n  absolutePaintRect " << m_absolutePaintRect
        << "\n  maxEffectRect " << m_maxEffectRect
        << "\n  filter scale " << filter.filterScale());

    if (m_absolutePaintRect.isEmpty() || ImageBuffer::sizeNeedsClamping(m_absolutePaintRect.size()))
        return false;

    if (!mayProduceInvalidPremultipliedPixels()) {
        for (auto& in : m_inputEffects)
            in->correctPremultipliedResultIfNeeded();
    }

    m_filterImage = FilterImage::create(m_filterPrimitiveSubregion, m_absolutePaintRect, resultIsAlphaImage(), filter.renderingMode(), resultColorSpace());
    if (!m_filterImage)
        return false;

    auto applier = createApplier(filter);
    if (!applier)
        return false;

    return applier->apply(filter, inputFilterImages(), *m_filterImage);
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
        ts.dumpProperty("alpha image", resultIsAlphaImage());
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
