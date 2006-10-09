/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
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
#include "SVGHelper.h"
#include "SVGLength.h"
#include "SVGNames.h"

namespace WebCore {

SVGLineElement::SVGLineElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_x1(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_y1(new SVGLength(this, LM_HEIGHT, viewportElement()))
    , m_x2(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_y2(new SVGLength(this, LM_HEIGHT, viewportElement()))
{
}

SVGLineElement::~SVGLineElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGLineElement, SVGLength*, Length, length, X1, x1, SVGNames::x1Attr.localName(), m_x1.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGLineElement, SVGLength*, Length, length, Y1, y1, SVGNames::y1Attr.localName(), m_y1.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGLineElement, SVGLength*, Length, length, X2, x2, SVGNames::x2Attr.localName(), m_x2.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGLineElement, SVGLength*, Length, length, Y2, y2, SVGNames::y2Attr.localName(), m_y2.get())

void SVGLineElement::parseMappedAttribute(MappedAttribute* attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::x1Attr)
        x1BaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::y1Attr)
        y1BaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::x2Attr)
        x2BaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::y2Attr)
        y2BaseValue()->setValueAsString(value);
    else
    {
        if (SVGTests::parseMappedAttribute(attr)) return;
        if (SVGLangSpace::parseMappedAttribute(attr)) return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

Path SVGLineElement::toPathData() const
{
    float _x1 = x1()->value(), _y1 = y1()->value();
    float _x2 = x2()->value(), _y2 = y2()->value();

    return Path::createLine(FloatPoint(_x1, _y1), FloatPoint(_x2, _y2));
}

const SVGStyledElement* SVGLineElement::pushAttributeContext(const SVGStyledElement* context)
{
    // All attribute's contexts are equal (so just take the one from 'x1').
    const SVGStyledElement* restore = x1()->context();

    x1()->setContext(context);
    y1()->setContext(context);
    x2()->setContext(context);
    y2()->setContext(context);
    
    SVGStyledElement::pushAttributeContext(context);
    return restore;
}

bool SVGLineElement::hasPercentageValues() const
{
    if (x1()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
        y1()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
        x2()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
        y2()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE)
        return true;

    return false;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

