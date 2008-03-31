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
#include "SVGFETurbulence.h"
#include "TextStream.h"

namespace WebCore {

SVGFETurbulence::SVGFETurbulence(SVGResourceFilter* filter)
    : SVGFilterEffect(filter)
    , m_baseFrequencyX(0.0f)
    , m_baseFrequencyY(0.0f)
    , m_numOctaves(0)
    , m_seed(0)
    , m_stitchTiles(false)
    , m_type(SVG_TURBULENCE_TYPE_UNKNOWN)
{
}

SVGTurbulanceType SVGFETurbulence::type() const
{
    return m_type;
}

void SVGFETurbulence::setType(SVGTurbulanceType type)
{
    m_type = type;
}

float SVGFETurbulence::baseFrequencyY() const
{
    return m_baseFrequencyY;
}

void SVGFETurbulence::setBaseFrequencyY(float baseFrequencyY)
{
    m_baseFrequencyY = baseFrequencyY;
}

float SVGFETurbulence::baseFrequencyX() const
{
    return m_baseFrequencyX;
}

void SVGFETurbulence::setBaseFrequencyX(float baseFrequencyX)
{
       m_baseFrequencyX = baseFrequencyX;
}

float SVGFETurbulence::seed() const
{
    return m_seed; 
}

void SVGFETurbulence::setSeed(float seed)
{
    m_seed = seed;
}

int SVGFETurbulence::numOctaves() const
{
    return m_numOctaves;
}

void SVGFETurbulence::setNumOctaves(bool numOctaves)
{
    m_numOctaves = numOctaves;
}

bool SVGFETurbulence::stitchTiles() const
{
    return m_stitchTiles;
}

void SVGFETurbulence::setStitchTiles(bool stitch)
{
    m_stitchTiles = stitch;
}

static TextStream& operator<<(TextStream& ts, SVGTurbulanceType t)
{
    switch (t)
    {
        case SVG_TURBULENCE_TYPE_UNKNOWN:
            ts << "UNKNOWN"; break;
        case SVG_TURBULENCE_TYPE_TURBULENCE:
            ts << "TURBULANCE"; break;
        case SVG_TURBULENCE_TYPE_FRACTALNOISE:
            ts << "NOISE"; break;
    }
    return ts;
}

TextStream& SVGFETurbulence::externalRepresentation(TextStream& ts) const
{
    ts << "[type=TURBULENCE] ";
    SVGFilterEffect::externalRepresentation(ts);
    ts << " [turbulence type=" << type() << "]"
        << " [base frequency x=" << baseFrequencyX() << " y=" << baseFrequencyY() << "]"
        << " [seed=" << seed() << "]"
        << " [num octaves=" << numOctaves() << "]"
        << " [stitch tiles=" << stitchTiles() << "]";
    return ts;

}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
