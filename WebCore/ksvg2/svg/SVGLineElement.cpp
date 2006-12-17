/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"

#ifdef SVG_SUPPORT
#include "SVGLineElement.h"

#include "FloatPoint.h"
#include "SVGLength.h"
#include "SVGNames.h"

namespace WebCore {

SVGLineElement::SVGLineElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_x1(this, LengthModeWidth)
    , m_y1(this, LengthModeHeight)
    , m_x2(this, LengthModeWidth)
    , m_y2(this, LengthModeHeight)
{
}

SVGLineElement::~SVGLineElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGLineElement, SVGLength, Length, length, X1, x1, SVGNames::x1Attr.localName(), m_x1)
ANIMATED_PROPERTY_DEFINITIONS(SVGLineElement, SVGLength, Length, length, Y1, y1, SVGNames::y1Attr.localName(), m_y1)
ANIMATED_PROPERTY_DEFINITIONS(SVGLineElement, SVGLength, Length, length, X2, x2, SVGNames::x2Attr.localName(), m_x2)
ANIMATED_PROPERTY_DEFINITIONS(SVGLineElement, SVGLength, Length, length, Y2, y2, SVGNames::y2Attr.localName(), m_y2)

void SVGLineElement::parseMappedAttribute(MappedAttribute* attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::x1Attr)
        setX1BaseValue(SVGLength(this, LengthModeWidth, value));
    else if (attr->name() == SVGNames::y1Attr)
        setY1BaseValue(SVGLength(this, LengthModeHeight, value));
    else if (attr->name() == SVGNames::x2Attr)
        setX2BaseValue(SVGLength(this, LengthModeWidth, value));
    else if (attr->name() == SVGNames::y2Attr)
        setY2BaseValue(SVGLength(this, LengthModeHeight, value));
    else
    {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

Path SVGLineElement::toPathData() const
{
    return Path::createLine(FloatPoint(x1().value(), y1().value()),
                            FloatPoint(x2().value(), y2().value()));
}

bool SVGLineElement::hasPercentageValues() const
{
    if (x1().unitType() == LengthTypePercentage ||
        y1().unitType() == LengthTypePercentage ||
        x2().unitType() == LengthTypePercentage ||
        y2().unitType() == LengthTypePercentage)
        return true;

    return false;
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
