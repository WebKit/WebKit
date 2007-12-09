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
#include "SVGFEColorMatrix.h"

namespace WebCore {

SVGFEColorMatrix::SVGFEColorMatrix(SVGResourceFilter* filter)
    : SVGFilterEffect(filter)
    , m_type(SVG_FECOLORMATRIX_TYPE_UNKNOWN)
{
}

SVGColorMatrixType SVGFEColorMatrix::type() const
{
    return m_type;
}

void SVGFEColorMatrix::setType(SVGColorMatrixType type)
{
    m_type = type;
}

const Vector<float>& SVGFEColorMatrix::values() const
{
    return m_values;
}

void SVGFEColorMatrix::setValues(const Vector<float> &values)
{
    m_values = values;
}

static TextStream& operator<<(TextStream& ts, SVGColorMatrixType t)
{
    switch (t)
    {
        case SVG_FECOLORMATRIX_TYPE_UNKNOWN:
            ts << "UNKNOWN"; break;
        case SVG_FECOLORMATRIX_TYPE_MATRIX:
            ts << "CMT_MATRIX"; break;
        case SVG_FECOLORMATRIX_TYPE_SATURATE:
            ts << "CMT_SATURATE"; break;
        case SVG_FECOLORMATRIX_TYPE_HUEROTATE:
            ts << "HUE-ROTATE"; break;
        case SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
            ts << "LUMINANCE-TO-ALPHA"; break;
    }
    return ts;
}

TextStream& SVGFEColorMatrix::externalRepresentation(TextStream& ts) const
{
    ts << "[type=COLOR-MATRIX] ";
    SVGFilterEffect::externalRepresentation(ts);
    ts << " [color matrix type=" << type() << "]"
        << " [values=" << values() << "]";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
