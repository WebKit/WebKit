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
#include "SVGFEBlend.h"
#include "TextStream.h"

namespace WebCore {

SVGFEBlend::SVGFEBlend(SVGResourceFilter* filter)
    : SVGFilterEffect(filter)
    , m_mode(SVG_FEBLEND_MODE_UNKNOWN)
{
}

String SVGFEBlend::in2() const
{
    return m_in2;
}

void SVGFEBlend::setIn2(const String& in2)
{
    m_in2 = in2;
}

SVGBlendModeType SVGFEBlend::blendMode() const
{
    return m_mode;
}

void SVGFEBlend::setBlendMode(SVGBlendModeType mode)
{
    m_mode = mode;
}

static TextStream& operator<<(TextStream& ts, SVGBlendModeType t)
{
    switch (t)
    {
        case SVG_FEBLEND_MODE_UNKNOWN:
            ts << "UNKNOWN"; break;
        case SVG_FEBLEND_MODE_NORMAL:
            ts << "NORMAL"; break;
        case SVG_FEBLEND_MODE_MULTIPLY:
            ts << "MULTIPLY"; break;
        case SVG_FEBLEND_MODE_SCREEN:
            ts << "SCREEN"; break;
        case SVG_FEBLEND_MODE_DARKEN:
            ts << "DARKEN"; break;
        case SVG_FEBLEND_MODE_LIGHTEN:
            ts << "LIGHTEN"; break;
    }
    return ts;
}

TextStream& SVGFEBlend::externalRepresentation(TextStream& ts) const
{
    ts << "[type=BLEND] ";
    SVGFilterEffect::externalRepresentation(ts);
    if (!m_in2.isEmpty())
        ts << " [in2=\"" << m_in2 << "\"]";
    ts << " [blend mode=" << m_mode << "]";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
