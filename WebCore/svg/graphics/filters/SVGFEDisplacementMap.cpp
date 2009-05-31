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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFEDisplacementMap.h"
#include "SVGRenderTreeAsText.h"
#include "Filter.h"

namespace WebCore {

FEDisplacementMap::FEDisplacementMap(FilterEffect* in, FilterEffect* in2, ChannelSelectorType xChannelSelector,
    ChannelSelectorType yChannelSelector, const float& scale)
    : FilterEffect()
    , m_in(in)
    , m_in2(in2)
    , m_xChannelSelector(xChannelSelector)
    , m_yChannelSelector(yChannelSelector)
    , m_scale(scale)
{
}

PassRefPtr<FEDisplacementMap> FEDisplacementMap::create(FilterEffect* in, FilterEffect* in2,
    ChannelSelectorType xChannelSelector, ChannelSelectorType yChannelSelector, const float& scale)
{
    return adoptRef(new FEDisplacementMap(in, in2, xChannelSelector, yChannelSelector, scale));
}

ChannelSelectorType FEDisplacementMap::xChannelSelector() const
{
    return m_xChannelSelector;
}

void FEDisplacementMap::setXChannelSelector(const ChannelSelectorType xChannelSelector)
{
    m_xChannelSelector = xChannelSelector;
}

ChannelSelectorType FEDisplacementMap::yChannelSelector() const
{
    return m_yChannelSelector;
}

void FEDisplacementMap::setYChannelSelector(const ChannelSelectorType yChannelSelector)
{
    m_yChannelSelector = yChannelSelector;
}

float FEDisplacementMap::scale() const
{
    return m_scale;
}

void FEDisplacementMap::setScale(float scale)
{
    m_scale = scale;
}

void FEDisplacementMap::apply(Filter*)
{
}

void FEDisplacementMap::dump()
{
}

static TextStream& operator<<(TextStream& ts, ChannelSelectorType t)
{
    switch (t)
    {
        case CHANNEL_UNKNOWN:
            ts << "UNKNOWN"; break;
        case CHANNEL_R:
            ts << "RED"; break;
        case CHANNEL_G:
            ts << "GREEN"; break;
        case CHANNEL_B:
            ts << "BLUE"; break;
        case CHANNEL_A:
            ts << "ALPHA"; break;
    }
    return ts;
}

TextStream& FEDisplacementMap::externalRepresentation(TextStream& ts) const
{
    ts << "[type=DISPLACEMENT-MAP] ";
    FilterEffect::externalRepresentation(ts);
    ts << " [in2=" << m_in2.get() << "]"
        << " [scale=" << m_scale << "]"
        << " [x channel selector=" << m_xChannelSelector << "]"
        << " [y channel selector=" << m_yChannelSelector << "]";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)
