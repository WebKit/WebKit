/*
    Copyright (C) 2006 Apple Computer, Inc.

    This file is part of the WebKit project

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

#include "SVGForeignObjectElement.h"

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGAnimatedLength.h"
#include "RenderForeignObject.h"

#include <wtf/Assertions.h>

#include "CSSPropertyNames.h"

namespace WebCore {

SVGForeignObjectElement::SVGForeignObjectElement(const QualifiedName& tagName, Document *doc)
: SVGStyledTransformableElement(tagName, doc), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired()
{
}

SVGForeignObjectElement::~SVGForeignObjectElement()
{
}

SVGAnimatedLength *SVGForeignObjectElement::x() const
{
    return lazy_create<SVGAnimatedLength>(m_x, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLength *SVGForeignObjectElement::y() const
{
    return lazy_create<SVGAnimatedLength>(m_y, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLength *SVGForeignObjectElement::width() const
{
    return lazy_create<SVGAnimatedLength>(m_width, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLength *SVGForeignObjectElement::height() const
{
    return lazy_create<SVGAnimatedLength>(m_height, this, LM_HEIGHT, viewportElement());
}

void SVGForeignObjectElement::parseMappedAttribute(MappedAttribute *attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::xAttr)
        x()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::yAttr)
        y()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::widthAttr) {
        width()->baseVal()->setValueAsString(value.impl());
        addCSSProperty(attr, CSS_PROP_WIDTH, value);
    } else if (attr->name() == SVGNames::heightAttr) {
        height()->baseVal()->setValueAsString(value.impl());
        addCSSProperty(attr, CSS_PROP_HEIGHT, value);
    } else {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

RenderObject *SVGForeignObjectElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderForeignObject(this);
}

bool SVGForeignObjectElement::childShouldCreateRenderer(WebCore::Node *child) const
{
    // Skip over SVG rules which disallow non-SVG kids
    return StyledElement::childShouldCreateRenderer(child);
}

};

// vim:ts=4:noet
#endif // SVG_SUPPORT

