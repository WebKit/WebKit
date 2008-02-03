/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

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
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGTextPositioningElement.h"

#include "SVGLengthList.h"
#include "SVGNames.h"
#include "SVGNumberList.h"

namespace WebCore {

SVGTextPositioningElement::SVGTextPositioningElement(const QualifiedName& tagName, Document* doc)
    : SVGTextContentElement(tagName, doc)
    , m_x(new SVGLengthList(SVGNames::xAttr))
    , m_y(new SVGLengthList(SVGNames::yAttr))
    , m_dx(new SVGLengthList(SVGNames::dxAttr))
    , m_dy(new SVGLengthList(SVGNames::dyAttr))
    , m_rotate(new SVGNumberList(SVGNames::rotateAttr))
{
}

SVGTextPositioningElement::~SVGTextPositioningElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGTextPositioningElement, SVGLengthList*, LengthList, lengthList, X, x, SVGNames::xAttr, m_x.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGTextPositioningElement, SVGLengthList*, LengthList, lengthList, Y, y, SVGNames::yAttr, m_y.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGTextPositioningElement, SVGLengthList*, LengthList, lengthList, Dx, dx, SVGNames::dxAttr, m_dx.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGTextPositioningElement, SVGLengthList*, LengthList, lengthList, Dy, dy, SVGNames::dyAttr, m_dy.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGTextPositioningElement, SVGNumberList*, NumberList, numberList, Rotate, rotate, SVGNames::rotateAttr, m_rotate.get())

void SVGTextPositioningElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::xAttr)
        xBaseValue()->parse(attr->value(), this, LengthModeWidth);
    else if (attr->name() == SVGNames::yAttr)
        yBaseValue()->parse(attr->value(), this, LengthModeHeight);
    else if (attr->name() == SVGNames::dxAttr)
        dxBaseValue()->parse(attr->value(), this, LengthModeWidth);
    else if (attr->name() == SVGNames::dyAttr)
        dyBaseValue()->parse(attr->value(), this, LengthModeHeight);
    else if (attr->name() == SVGNames::rotateAttr)
        rotateBaseValue()->parse(attr->value());
    else
        SVGTextContentElement::parseMappedAttribute(attr);
}

}

#endif // ENABLE(SVG)
