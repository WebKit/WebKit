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
#if SVG_SUPPORT
#include "Attr.h"

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGCircleElement.h"
#include "SVGAnimatedLength.h"

#include <kcanvas/KCanvasCreator.h>

using namespace WebCore;

SVGCircleElement::SVGCircleElement(const QualifiedName& tagName, Document *doc)
: SVGStyledTransformableElement(tagName, doc), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired()
{
}

SVGCircleElement::~SVGCircleElement()
{
}

SVGAnimatedLength *SVGCircleElement::cx() const
{
    return lazy_create<SVGAnimatedLength>(m_cx, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLength *SVGCircleElement::cy() const
{
    return lazy_create<SVGAnimatedLength>(m_cy, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLength *SVGCircleElement::r() const
{
    return lazy_create<SVGAnimatedLength>(m_r, this, LM_OTHER, viewportElement());
}

void SVGCircleElement::parseMappedAttribute(MappedAttribute *attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::cxAttr)
        cx()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::cyAttr)
        cy()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::rAttr)
        r()->baseVal()->setValueAsString(value.impl());
    else
    {
        if(SVGTests::parseMappedAttribute(attr)) return;
        if(SVGLangSpace::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

KCanvasPath* SVGCircleElement::toPathData() const
{
    float _cx = cx()->baseVal()->value(), _cy = cy()->baseVal()->value();
    float _r = r()->baseVal()->value();

    return KCanvasCreator::self()->createCircle(_cx, _cy, _r);
}

const SVGStyledElement *SVGCircleElement::pushAttributeContext(const SVGStyledElement *context)
{
    // All attribute's contexts are equal (so just take the one from 'cx').
    const SVGStyledElement *restore = cx()->baseVal()->context();

    cx()->baseVal()->setContext(context);
    cy()->baseVal()->setContext(context);
    r()->baseVal()->setContext(context);
    
    SVGStyledElement::pushAttributeContext(context);
    return restore;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

