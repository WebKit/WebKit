/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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
#include "FETile.h"

#include "AffineTransform.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "Pattern.h"

namespace WebCore {

FETile::FETile()
    : FilterEffect()
{
}

PassRefPtr<FETile> FETile::create()
{
    return adoptRef(new FETile);
}

FloatRect FETile::determineFilterPrimitiveSubregion(Filter* filter)
{
    inputEffect(0)->determineFilterPrimitiveSubregion(filter);

    filter->determineFilterPrimitiveSubregion(this, filter->filterRegion());
    return filterPrimitiveSubregion();
}

void FETile::apply(Filter* filter)
{
    FilterEffect* in = inputEffect(0);
    in->apply(filter);
    if (!in->resultImage())
        return;

    GraphicsContext* filterContext = effectContext();
    if (!filterContext)
        return;

    setIsAlphaImage(in->isAlphaImage());

    IntRect tileRect = enclosingIntRect(in->repaintRectInLocalCoordinates());

    // Source input needs more attention. It has the size of the filterRegion but gives the
    // size of the cutted sourceImage back. This is part of the specification and optimization.
    if (in->isSourceInput()) {
        FloatRect filterRegion = filter->filterRegion();
        filterRegion.scale(filter->filterResolution().width(), filter->filterResolution().height());
        tileRect = enclosingIntRect(filterRegion);
    }

    OwnPtr<ImageBuffer> tileImage = ImageBuffer::create(tileRect.size());
    GraphicsContext* tileImageContext = tileImage->context();
    tileImageContext->drawImageBuffer(in->resultImage(), DeviceColorSpace, IntPoint());
    RefPtr<Pattern> pattern = Pattern::create(tileImage->copyImage(), true, true);

    AffineTransform matrix;
    matrix.translate(in->repaintRectInLocalCoordinates().x() - repaintRectInLocalCoordinates().x(),
                     in->repaintRectInLocalCoordinates().y() - repaintRectInLocalCoordinates().y());
    pattern.get()->setPatternSpaceTransform(matrix);

    filterContext->setFillPattern(pattern);
    filterContext->fillRect(FloatRect(FloatPoint(), repaintRectInLocalCoordinates().size()));
}

void FETile::dump()
{
}

TextStream& FETile::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feTile";
    FilterEffect::externalRepresentation(ts);
    ts << "]\n";
    inputEffect(0)->externalRepresentation(ts, indent + 1);

    return ts;
}

} // namespace WebCore

#endif // ENABLE(FILTERS)

