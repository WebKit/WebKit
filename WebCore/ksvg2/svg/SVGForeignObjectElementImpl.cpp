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

#include "SVGForeignObjectElementImpl.h"

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGAnimatedLengthImpl.h"
#include "RenderForeignObject.h"

#include <kxmlcore/Assertions.h>

#include "cssproperties.h"

namespace KSVG {

SVGForeignObjectElementImpl::SVGForeignObjectElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc)
: SVGStyledTransformableElementImpl(tagName, doc), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl()
{
}

SVGForeignObjectElementImpl::~SVGForeignObjectElementImpl()
{
}

SVGAnimatedLengthImpl *SVGForeignObjectElementImpl::x() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_x, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGForeignObjectElementImpl::y() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_y, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGForeignObjectElementImpl::width() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_width, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGForeignObjectElementImpl::height() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_height, this, LM_HEIGHT, viewportElement());
}

void SVGForeignObjectElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    const KDOM::AtomicString& value = attr->value();
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
        if (SVGTestsImpl::parseMappedAttribute(attr))
            return;
        if (SVGLangSpaceImpl::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequiredImpl::parseMappedAttribute(attr))
            return;
        SVGStyledTransformableElementImpl::parseMappedAttribute(attr);
    }
}

khtml::RenderObject *SVGForeignObjectElementImpl::createRenderer(RenderArena *arena, khtml::RenderStyle *style)
{
    return new (arena) RenderForeignObject(this);
}

bool SVGForeignObjectElementImpl::childShouldCreateRenderer(DOM::NodeImpl *child) const
{
    // Skip over SVG rules which disallow non-SVG kids
    return StyledElementImpl::childShouldCreateRenderer(child);
}

};

// vim:ts=4:noet
