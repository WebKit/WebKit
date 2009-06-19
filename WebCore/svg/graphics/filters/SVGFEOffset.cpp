/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>
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
#include "SVGFEOffset.h"

#include "Filter.h"
#include "GraphicsContext.h"
#include "SVGRenderTreeAsText.h"

namespace WebCore {

FEOffset::FEOffset(FilterEffect* in, const float& dx, const float& dy)
    : FilterEffect()
    , m_in(in)
    , m_dx(dx)
    , m_dy(dy)
{
}

PassRefPtr<FEOffset> FEOffset::create(FilterEffect* in, const float& dx, const float& dy)
{
    return adoptRef(new FEOffset(in, dx, dy));
}

float FEOffset::dx() const
{
    return m_dx;
}

void FEOffset::setDx(float dx)
{
    m_dx = dx;
}

float FEOffset::dy() const
{
    return m_dy;
}

void FEOffset::setDy(float dy)
{
    m_dy = dy;
}

void FEOffset::apply(Filter* filter)
{
    m_in->apply(filter);
    if (!m_in->resultImage())
        return;

    GraphicsContext* filterContext = getEffectContext();
    if (!filterContext)
        return;

    if (filter->effectBoundingBoxMode()) {
        setDx(dx() * filter->sourceImageRect().width());
        setDy(dy() * filter->sourceImageRect().height());
    }

    FloatRect dstRect = FloatRect(dx() + m_in->subRegion().x() - subRegion().x(),
                                  dy() + m_in->subRegion().y() - subRegion().y(),
                                  m_in->subRegion().width(),
                                  m_in->subRegion().height());

    filterContext->drawImage(m_in->resultImage()->image(), dstRect);
}

void FEOffset::dump()
{
}

TextStream& FEOffset::externalRepresentation(TextStream& ts) const
{
    ts << "[type=OFFSET] "; 
    FilterEffect::externalRepresentation(ts);
    ts << " [dx=" << dx() << " dy=" << dy() << "]";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)
