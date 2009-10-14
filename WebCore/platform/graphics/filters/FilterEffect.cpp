/*
    Copyright (C) Alex Mathews <possessedpenguinbob@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(FILTERS)
#include "FilterEffect.h"

namespace WebCore {

FilterEffect::FilterEffect()
    : m_hasX(false)
    , m_hasY(false)
    , m_hasWidth(false)
    , m_hasHeight(false)
    , m_alphaImage(false)
{
}

FilterEffect::~FilterEffect()
{
}

FloatRect FilterEffect::calculateUnionOfChildEffectSubregions(Filter* filter, FilterEffect* in)
{
    return in->calculateEffectRect(filter);
}

FloatRect FilterEffect::calculateUnionOfChildEffectSubregions(Filter* filter, FilterEffect* in, FilterEffect* in2)
{
    FloatRect uniteEffectRect = in->calculateEffectRect(filter);
    uniteEffectRect.unite(in2->calculateEffectRect(filter));
    return uniteEffectRect;
}

FloatRect FilterEffect::calculateEffectRect(Filter* filter)
{
    setUnionOfChildEffectSubregions(uniteChildEffectSubregions(filter));
    filter->calculateEffectSubRegion(this);
    return subRegion();
}

IntRect FilterEffect::calculateDrawingIntRect(const FloatRect& effectRect)
{
    IntPoint location = roundedIntPoint(FloatPoint(subRegion().x() - effectRect.x(),
                                                   subRegion().y() - effectRect.y()));
    return IntRect(location, resultImage()->size());
}

FloatRect FilterEffect::calculateDrawingRect(const FloatRect& srcRect)
{
    FloatPoint startPoint = FloatPoint(srcRect.x() - subRegion().x(), srcRect.y() - subRegion().y());
    FloatRect drawingRect = FloatRect(startPoint, srcRect.size());
    return drawingRect;
}

GraphicsContext* FilterEffect::getEffectContext()
{
    IntRect bufferRect = enclosingIntRect(subRegion());
    m_effectBuffer = ImageBuffer::create(bufferRect.size(), LinearRGB);
    return m_effectBuffer->context();
}

TextStream& FilterEffect::externalRepresentation(TextStream& ts) const
{
    return ts;
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
