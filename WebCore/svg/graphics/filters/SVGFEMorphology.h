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

#ifndef SVGFEMorphology_h
#define SVGFEMorphology_h

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFilterEffect.h"

namespace WebCore {

enum SVGMorphologyOperatorType {
    SVG_MORPHOLOGY_OPERATOR_UNKNOWN = 0,
    SVG_MORPHOLOGY_OPERATOR_ERODE   = 1,
    SVG_MORPHOLOGY_OPERATOR_DIALATE = 2
};

class SVGFEMorphology : public SVGFilterEffect {
public:
    SVGFEMorphology(SVGResourceFilter*);

    SVGMorphologyOperatorType morphologyOperator() const;
    void setMorphologyOperator(SVGMorphologyOperatorType);

    float radiusX() const;
    void setRadiusX(float);

    float radiusY() const;
    void setRadiusY(float);

    virtual TextStream& externalRepresentation(TextStream&) const;

private:
    SVGMorphologyOperatorType m_operator;
    float m_radiusX;
    float m_radiusY;
};

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)

#endif // SVGFEMorphology_h
