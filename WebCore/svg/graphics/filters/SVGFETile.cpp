/*
    Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
                  2009 Dirk Schulze <krit@webkit.org>

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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFETile.h"

#include "Filter.h"
#include "GraphicsContext.h"
#include "Pattern.h"
#include "TransformationMatrix.h"
#include "SVGRenderTreeAsText.h"

namespace WebCore {

FETile::FETile(FilterEffect* in)
    : FilterEffect()
    , m_in(in)
{
}

PassRefPtr<FETile> FETile::create(FilterEffect* in)
{
    return adoptRef(new FETile(in));
}

FloatRect FETile::uniteChildEffectSubregions(Filter* filter)
{
    m_in->calculateEffectRect(filter);
    return filter->filterRegion();
}

void FETile::apply(Filter* filter)
{
    m_in->apply(filter);
    if (!m_in->resultImage())
        return;

    GraphicsContext* filterContext = getEffectContext();
    if (!filterContext)
        return;

    IntRect tileRect = enclosingIntRect(m_in->subRegion());

    // Source input needs more attention. It has the size of the filterRegion but gives the
    // size of the cutted sourceImage back. This is part of the specification and optimization.
    if (m_in->isSourceInput())
        tileRect = enclosingIntRect(filter->filterRegion());

    OwnPtr<ImageBuffer> tileImage = ImageBuffer::create(tileRect.size(), false);
    GraphicsContext* tileImageContext = tileImage->context();
    tileImageContext->drawImage(m_in->resultImage()->image(), IntPoint());
    RefPtr<Pattern> pattern = Pattern::create(tileImage->image(), true, true);

    TransformationMatrix matrix;
    matrix.translate(m_in->subRegion().x() - subRegion().x(), m_in->subRegion().y() - subRegion().y());
    pattern.get()->setPatternSpaceTransform(matrix);

    filterContext->setFillPattern(pattern);
    filterContext->fillRect(FloatRect(FloatPoint(), subRegion().size()));
}

void FETile::dump()
{
}

TextStream& FETile::externalRepresentation(TextStream& ts) const
{
    ts << "[type=TILE]";
    FilterEffect::externalRepresentation(ts);
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)

