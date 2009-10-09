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
#include "SVGFEMorphology.h"
#include "SVGRenderTreeAsText.h"
#include "Filter.h"

namespace WebCore {

FEMorphology::FEMorphology(FilterEffect* in, MorphologyOperatorType type, float radiusX, float radiusY)
    : FilterEffect()
    , m_in(in)
    , m_type(type)
    , m_radiusX(radiusX)
    , m_radiusY(radiusY)
{
}

PassRefPtr<FEMorphology> FEMorphology::create(FilterEffect* in, MorphologyOperatorType type, float radiusX, float radiusY)
{
    return adoptRef(new FEMorphology(in, type, radiusX, radiusY));
}

MorphologyOperatorType FEMorphology::morphologyOperator() const
{
    return m_type;
}

void FEMorphology::setMorphologyOperator(MorphologyOperatorType type)
{
    m_type = type;
}

float FEMorphology::radiusX() const
{
    return m_radiusX;
}

void FEMorphology::setRadiusX(float radiusX)
{
    m_radiusX = radiusX;
}

float FEMorphology::radiusY() const
{
    return m_radiusY;
}

void FEMorphology::setRadiusY(float radiusY)
{
    m_radiusY = radiusY;
}

void FEMorphology::apply(Filter*)
{
}

void FEMorphology::dump()
{
}

static TextStream& operator<<(TextStream& ts, MorphologyOperatorType t)
{
    switch (t)
    {
        case FEMORPHOLOGY_OPERATOR_UNKNOWN:
            ts << "UNKNOWN"; break;
        case FEMORPHOLOGY_OPERATOR_ERODE:
            ts << "ERODE"; break;
        case FEMORPHOLOGY_OPERATOR_DILATE:
            ts << "DILATE"; break;
    }
    return ts;
}

TextStream& FEMorphology::externalRepresentation(TextStream& ts) const
{
    ts << "[type=MORPHOLOGY] ";
    FilterEffect::externalRepresentation(ts);
    ts << " [operator type=" << morphologyOperator() << "]"
        << " [radius x=" << radiusX() << " y=" << radiusY() << "]";        
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)
