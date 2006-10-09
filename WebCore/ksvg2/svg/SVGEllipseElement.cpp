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
#include "SVGEllipseElement.h"

#include "FloatPoint.h"
#include "SVGHelper.h"
#include "SVGLength.h"
#include "SVGNames.h"

namespace WebCore {

SVGEllipseElement::SVGEllipseElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_cx(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_cy(new SVGLength(this, LM_HEIGHT, viewportElement()))
    , m_rx(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_ry(new SVGLength(this, LM_HEIGHT, viewportElement()))
{
}    

SVGEllipseElement::~SVGEllipseElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGEllipseElement, SVGLength*, Length, length, Cx, cx, SVGNames::cxAttr.localName(), m_cx.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGEllipseElement, SVGLength*, Length, length, Cy, cy, SVGNames::cyAttr.localName(), m_cy.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGEllipseElement, SVGLength*, Length, length, Rx, rx, SVGNames::rxAttr.localName(), m_rx.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGEllipseElement, SVGLength*, Length, length, Ry, ry, SVGNames::ryAttr.localName(), m_ry.get())

void SVGEllipseElement::parseMappedAttribute(MappedAttribute* attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::cxAttr)
        cxBaseValue()->setValueAsString(value);
    if (attr->name() == SVGNames::cyAttr)
        cyBaseValue()->setValueAsString(value);
    if (attr->name() == SVGNames::rxAttr)
        rxBaseValue()->setValueAsString(value);
    if (attr->name() == SVGNames::ryAttr)
        ryBaseValue()->setValueAsString(value);
    else {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

Path SVGEllipseElement::toPathData() const
{
    float _cx = cx()->value(), _cy = cy()->value();
    float _rx = rx()->value(), _ry = ry()->value();

    return Path::createEllipse(FloatPoint(_cx, _cy), _rx, _ry);
}

const SVGStyledElement* SVGEllipseElement::pushAttributeContext(const SVGStyledElement* context)
{
    // All attribute's contexts are equal (so just take the one from 'cx').
    const SVGStyledElement* restore = cx()->context();

    cx()->setContext(context);
    cy()->setContext(context);
    rx()->setContext(context);
    ry()->setContext(context);

    SVGStyledElement::pushAttributeContext(context);
    return restore;
}

bool SVGEllipseElement::hasPercentageValues() const
{
    if (cx()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
        cy()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
        rx()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
        ry()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE)
        return true;

    return false;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

