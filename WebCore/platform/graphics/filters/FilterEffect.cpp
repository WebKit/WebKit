/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#if ENABLE(FILTERS)
#include "FilterEffect.h"

namespace WebCore {

FilterEffect::FilterEffect()
    : m_alphaImage(false)
    , m_hasX(false)
    , m_hasY(false)
    , m_hasWidth(false)
    , m_hasHeight(false)
{
}

FilterEffect::~FilterEffect()
{
}

FloatRect FilterEffect::determineFilterPrimitiveSubregion(Filter* filter)
{
    FloatRect uniteRect;
    unsigned size = m_inputEffects.size();

    // FETurbulence, FEImage and FEFlood don't have input effects, take the filter region as unite rect.
    if (!size)
        uniteRect = filter->filterRegion();
    else {
        for (unsigned i = 0; i < size; ++i)
            uniteRect.unite(m_inputEffects.at(i)->determineFilterPrimitiveSubregion(filter));
    }

    filter->determineFilterPrimitiveSubregion(this, uniteRect);
    return m_filterPrimitiveSubregion;
}

IntRect FilterEffect::requestedRegionOfInputImageData(const FloatRect& effectRect) const
{
    ASSERT(m_effectBuffer);
    FloatPoint location = m_repaintRectInLocalCoordinates.location();
    location.move(-effectRect.x(), -effectRect.y());
    return IntRect(roundedIntPoint(location), m_effectBuffer->size());
}

FloatRect FilterEffect::drawingRegionOfInputImage(const FloatRect& srcRect) const
{
    return FloatRect(FloatPoint(srcRect.x() - m_repaintRectInLocalCoordinates.x(),
                                srcRect.y() - m_repaintRectInLocalCoordinates.y()), srcRect.size());
}

FilterEffect* FilterEffect::inputEffect(unsigned number) const
{
    ASSERT(number < m_inputEffects.size());
    return m_inputEffects.at(number).get();
}

GraphicsContext* FilterEffect::effectContext()
{
    IntRect bufferRect = enclosingIntRect(m_repaintRectInLocalCoordinates);
    m_effectBuffer = ImageBuffer::create(bufferRect.size(), LinearRGB);
    if (!m_effectBuffer)
        return 0;
    return m_effectBuffer->context();
}

TextStream& FilterEffect::externalRepresentation(TextStream& ts, int) const
{
    // FIXME: We should dump the subRegions of the filter primitives here later. This isn't
    // possible at the moment, because we need more detailed informations from the target object.
    return ts;
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
