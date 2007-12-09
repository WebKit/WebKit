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
#include "SVGFEDisplacementMap.h"

namespace WebCore {

SVGFEDisplacementMap::SVGFEDisplacementMap(SVGResourceFilter* filter)
    : SVGFilterEffect(filter)
    , m_xChannelSelector(SVG_CHANNEL_UNKNOWN)
    , m_yChannelSelector(SVG_CHANNEL_UNKNOWN)
    , m_scale(0)
{
}

String SVGFEDisplacementMap::in2() const
{
    return m_in2;
}

void SVGFEDisplacementMap::setIn2(const String &in2)
{
    m_in2 = in2;
}

SVGChannelSelectorType SVGFEDisplacementMap::xChannelSelector() const
{
    return m_xChannelSelector;
}

void SVGFEDisplacementMap::setXChannelSelector(const SVGChannelSelectorType xChannelSelector)
{
    m_xChannelSelector = xChannelSelector;
}

SVGChannelSelectorType SVGFEDisplacementMap::yChannelSelector() const
{
    return m_yChannelSelector;
}

void SVGFEDisplacementMap::setYChannelSelector(const SVGChannelSelectorType yChannelSelector)
{
    m_yChannelSelector = yChannelSelector;
}

float SVGFEDisplacementMap::scale() const
{
    return m_scale;
}

void SVGFEDisplacementMap::setScale(float scale)
{
    m_scale = scale;
}

static TextStream& operator<<(TextStream& ts, SVGChannelSelectorType t)
{
    switch (t)
    {
        case SVG_CHANNEL_UNKNOWN:
            ts << "UNKNOWN"; break;
        case SVG_CHANNEL_R:
            ts << "RED"; break;
        case SVG_CHANNEL_G:
            ts << "GREEN"; break;
        case SVG_CHANNEL_B:
            ts << "BLUE"; break;
        case SVG_CHANNEL_A:
            ts << "ALPHA"; break;
    }
    return ts;
}

TextStream& SVGFEDisplacementMap::externalRepresentation(TextStream& ts) const
{
    ts << "[type=DISPLACEMENT-MAP] ";
    SVGFilterEffect::externalRepresentation(ts);
    if (!in2().isEmpty())
        ts << " [in2=" << in2() << "]";
    ts << " [scale=" << m_scale << "]"
        << " [x channel selector=" << m_xChannelSelector << "]"
        << " [y channel selector=" << m_yChannelSelector << "]";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
