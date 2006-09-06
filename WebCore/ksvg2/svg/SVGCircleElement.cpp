/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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
#include "Attr.h"

#include "FloatPoint.h"

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGCircleElement.h"
#include "SVGLength.h"

using namespace WebCore;

SVGCircleElement::SVGCircleElement(const QualifiedName& tagName, Document *doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_cx(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_cy(new SVGLength(this, LM_HEIGHT, viewportElement()))
    , m_r(new SVGLength(this, LM_OTHER, viewportElement()))
{
}

SVGCircleElement::~SVGCircleElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGCircleElement, SVGLength*, Length, length, Cx, cx, SVGNames::cxAttr.localName(), m_cx.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGCircleElement, SVGLength*, Length, length, Cy, cy, SVGNames::cyAttr.localName(), m_cy.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGCircleElement, SVGLength*, Length, length, R, r, SVGNames::rAttr.localName(), m_r.get())

void SVGCircleElement::parseMappedAttribute(MappedAttribute *attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::cxAttr)
        cxBaseValue()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::cyAttr)
        cyBaseValue()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::rAttr)
        rBaseValue()->setValueAsString(value.impl());
    else
    {
        if(SVGTests::parseMappedAttribute(attr)) return;
        if(SVGLangSpace::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

Path SVGCircleElement::toPathData() const
{
    float _cx = cxBaseValue()->value(), _cy = cyBaseValue()->value();
    float _r = rBaseValue()->value();

    return Path::createCircle(FloatPoint(_cx, _cy), _r);
}

const SVGStyledElement *SVGCircleElement::pushAttributeContext(const SVGStyledElement *context)
{
    // All attribute's contexts are equal (so just take the one from 'cx').
    const SVGStyledElement *restore = cxBaseValue()->context();

    cxBaseValue()->setContext(context);
    cyBaseValue()->setContext(context);
    rBaseValue()->setContext(context);
    
    SVGStyledElement::pushAttributeContext(context);
    return restore;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

