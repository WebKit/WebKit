/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
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
#include "FEOffset.h"

#include "Filter.h"
#include "GraphicsContext.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

FEOffset::FEOffset(Filter& filter, float dx, float dy)
    : FilterEffect(filter)
    , m_dx(dx)
    , m_dy(dy)
{
}

Ref<FEOffset> FEOffset::create(Filter& filter, float dx, float dy)
{
    return adoptRef(*new FEOffset(filter, dx, dy));
}

void FEOffset::setDx(float dx)
{
    m_dx = dx;
}

void FEOffset::setDy(float dy)
{
    m_dy = dy;
}

void FEOffset::determineAbsolutePaintRect()
{
    FloatRect paintRect = inputEffect(0)->absolutePaintRect();
    Filter& filter = this->filter();
    paintRect.move(filter.scaledByFilterResolution({ m_dx, m_dy }));
    if (clipsToBounds())
        paintRect.intersect(maxEffectRect());
    else
        paintRect.unite(maxEffectRect());
    setAbsolutePaintRect(enclosingIntRect(paintRect));
}

void FEOffset::platformApplySoftware()
{
    FilterEffect* in = inputEffect(0);

    ImageBuffer* resultImage = createImageBufferResult();
    ImageBuffer* inBuffer = in->imageBufferResult();
    if (!resultImage || !inBuffer)
        return;

    setIsAlphaImage(in->isAlphaImage());

    FloatRect drawingRegion = drawingRegionOfInputImage(in->absolutePaintRect());
    Filter& filter = this->filter();
    drawingRegion.move(filter.scaledByFilterResolution({ m_dx, m_dy }));
    resultImage->context().drawImageBuffer(*inBuffer, drawingRegion);
}

TextStream& FEOffset::externalRepresentation(TextStream& ts, RepresentationType representation) const
{
    ts << indent << "[feOffset";
    FilterEffect::externalRepresentation(ts, representation);
    ts << " dx=\"" << dx() << "\" dy=\"" << dy() << "\"]\n";

    TextStream::IndentScope indentScope(ts);
    inputEffect(0)->externalRepresentation(ts, representation);
    return ts;
}

} // namespace WebCore
