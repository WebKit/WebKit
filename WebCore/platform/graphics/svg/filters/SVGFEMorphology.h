/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric.seidel@kdemail.net>

    This file is part of the KDE project

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef SVGFEMorphology_H
#define SVGFEMorphology_H

#ifdef SVG_SUPPORT
#include "SVGFilterEffect.h"

namespace WebCore {

enum SVGMorphologyOperatorType {
    SVG_MORPHOLOGY_OPERATOR_UNKNOWN = 0,
    SVG_MORPHOLOGY_OPERATOR_ERODE   = 1,
    SVG_MORPHOLOGY_OPERATOR_DIALATE = 2
};

class SVGFEMorphology : public SVGFilterEffect {
public:
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

#endif // SVG_SUPPORT

#endif // SVGFEMorphology_H
