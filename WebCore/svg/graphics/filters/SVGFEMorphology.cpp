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
#include "SVGFEMorphology.h"
#include "TextStream.h"

namespace WebCore {

SVGFEMorphology::SVGFEMorphology(SVGResourceFilter* filter)
    : SVGFilterEffect(filter)
    , m_operator(SVG_MORPHOLOGY_OPERATOR_UNKNOWN)
    , m_radiusX(0.0f)
    , m_radiusY(0.0f)
{
}

SVGMorphologyOperatorType SVGFEMorphology::morphologyOperator() const
{
    return m_operator;
}

void SVGFEMorphology::setMorphologyOperator(SVGMorphologyOperatorType _operator)
{
    m_operator = _operator;
}

float SVGFEMorphology::radiusX() const
{
    return m_radiusX;
}

void SVGFEMorphology::setRadiusX(float radiusX)
{
    m_radiusX = radiusX;
}

float SVGFEMorphology::radiusY() const
{
    return m_radiusY;
}

void SVGFEMorphology::setRadiusY(float radiusY)
{
    m_radiusY = radiusY;
}

static TextStream& operator<<(TextStream& ts, SVGMorphologyOperatorType t)
{
    switch (t)
    {
        case SVG_MORPHOLOGY_OPERATOR_UNKNOWN:
            ts << "UNKNOWN"; break;
        case SVG_MORPHOLOGY_OPERATOR_ERODE:
            ts << "ERODE"; break;
        case SVG_MORPHOLOGY_OPERATOR_DIALATE:
            ts << "DIALATE"; break;
    }
    return ts;
}

TextStream& SVGFEMorphology::externalRepresentation(TextStream& ts) const
{
    ts << "[type=MORPHOLOGY-OPERATOR] ";
    SVGFilterEffect::externalRepresentation(ts);
    ts << " [operator type=" << morphologyOperator() << "]"
        << " [radius x=" << radiusX() << " y=" << radiusY() << "]";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
