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
#include "SVGFEFlood.h"

#include "Filter.h"
#include "GraphicsContext.h"
#include "SVGRenderTreeAsText.h"

namespace WebCore {

FEFlood::FEFlood(FilterEffect* in, const Color& floodColor, const float& floodOpacity)
    : FilterEffect()
    , m_in(in)
    , m_floodColor(floodColor)
    , m_floodOpacity(floodOpacity)
{
}

PassRefPtr<FEFlood> FEFlood::create(FilterEffect* in, const Color& floodColor, const float& floodOpacity)
{
    return adoptRef(new FEFlood(in, floodColor, floodOpacity));
}

Color FEFlood::floodColor() const
{
    return m_floodColor;
}

void FEFlood::setFloodColor(const Color& color)
{
    m_floodColor = color;
}

float FEFlood::floodOpacity() const
{
    return m_floodOpacity;
}

void FEFlood::setFloodOpacity(float floodOpacity)
{
    m_floodOpacity = floodOpacity;
}

void FEFlood::apply(Filter*)
{
    GraphicsContext* filterContext = getEffectContext();
    if (!filterContext)
        return;

    Color color = colorWithOverrideAlpha(floodColor().rgb(), floodOpacity());
    filterContext->fillRect(FloatRect(FloatPoint(), subRegion().size()), color);
}

void FEFlood::dump()
{
}

TextStream& FEFlood::externalRepresentation(TextStream& ts) const
{
    ts << "[type=FLOOD] ";
    FilterEffect::externalRepresentation(ts);
    ts << " [color=" << floodColor() << "]"
        << " [opacity=" << floodOpacity() << "]";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)
