/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>

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

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGRenderTreeAsText.h"
#include "SVGFEFlood.h"

namespace WebCore {

SVGFEFlood::SVGFEFlood(SVGResourceFilter* filter)
    : SVGFilterEffect(filter)
    , m_floodColor()
    , m_floodOpacity(0.0f)
{
}

Color SVGFEFlood::floodColor() const
{
    return m_floodColor;
}

void SVGFEFlood::setFloodColor(const Color& color)
{
    m_floodColor = color;
}

float SVGFEFlood::floodOpacity() const
{
    return m_floodOpacity;
}

void SVGFEFlood::setFloodOpacity(float floodOpacity)
{
    m_floodOpacity = floodOpacity;
}

TextStream& SVGFEFlood::externalRepresentation(TextStream& ts) const
{
    ts << "[type=FLOOD] ";
    SVGFilterEffect::externalRepresentation(ts);
    ts << " [color=" << floodColor() << "]"
        << " [opacity=" << floodOpacity() << "]";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
