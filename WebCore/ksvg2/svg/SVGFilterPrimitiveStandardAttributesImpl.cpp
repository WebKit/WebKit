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
#include <kdom/core/AttrImpl.h>
#include <kxmlcore/Assertions.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGFilterPrimitiveStandardAttributesImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGStyledElementImpl.h"
#include "SVGFilterElementImpl.h"

#include <kcanvas/KCanvasFilters.h>

using namespace KSVG;

SVGFilterPrimitiveStandardAttributesImpl::SVGFilterPrimitiveStandardAttributesImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc)
: SVGStyledElementImpl(tagName, doc)
{
    m_x = m_y = m_width = m_height = 0;
    m_result = 0;
}

SVGFilterPrimitiveStandardAttributesImpl::~SVGFilterPrimitiveStandardAttributesImpl()
{
    if(m_x)
        m_x->deref();
    if(m_y)
        m_y->deref();
    if(m_width)
        m_width->deref();
    if(m_height)
        m_height->deref();
    if(m_result)
        m_result->deref();
}

SVGAnimatedLengthImpl *SVGFilterPrimitiveStandardAttributesImpl::x() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "0%" were specified.
    return lazy_create<SVGAnimatedLengthImpl>(m_x, this, LM_WIDTH);
}

SVGAnimatedLengthImpl *SVGFilterPrimitiveStandardAttributesImpl::y() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "0%" were specified.
    return lazy_create<SVGAnimatedLengthImpl>(m_y, this, LM_HEIGHT);
}

SVGAnimatedLengthImpl *SVGFilterPrimitiveStandardAttributesImpl::width() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "100%" were specified.
    if(!m_width)
    {
        lazy_create<SVGAnimatedLengthImpl>(m_width, this, LM_WIDTH);
        m_width->baseVal()->setValueAsString(KDOM::DOMString("100%").impl());
        return m_width;
    }

    return m_width;
}

SVGAnimatedLengthImpl *SVGFilterPrimitiveStandardAttributesImpl::height() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "100%" were specified.
    if(!m_height)
    {
        lazy_create<SVGAnimatedLengthImpl>(m_height, this, LM_HEIGHT);
        m_height->baseVal()->setValueAsString(KDOM::DOMString("100%").impl());
        return m_height;
    }

    return m_height;
}

SVGAnimatedStringImpl *SVGFilterPrimitiveStandardAttributesImpl::result() const
{
    return lazy_create<SVGAnimatedStringImpl>(m_result, this);
}

void SVGFilterPrimitiveStandardAttributesImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    const KDOM::AtomicString& value = attr->value();
    if (attr->name() == SVGNames::xAttr)
        x()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::yAttr)
        y()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::widthAttr)
        width()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::heightAttr)
        height()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::resultAttr)
        result()->setBaseVal(value.impl());
    else
        return SVGStyledElementImpl::parseMappedAttribute(attr);
}

void SVGFilterPrimitiveStandardAttributesImpl::setStandardAttributes(KCanvasFilterEffect *filterEffect) const
{
    ASSERT(filterEffect);
    if (!filterEffect)
        return;
    bool bbox = false;
    if(parentNode() && parentNode()->hasTagName(SVGNames::filterTag))
        bbox = static_cast<SVGFilterElementImpl *>(parentNode())->primitiveUnits()->baseVal() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;

    x()->baseVal()->setBboxRelative(bbox);
    y()->baseVal()->setBboxRelative(bbox);
    width()->baseVal()->setBboxRelative(bbox);
    height()->baseVal()->setBboxRelative(bbox);
    float _x = x()->baseVal()->value(), _y = y()->baseVal()->value();
    float _width = width()->baseVal()->value(), _height = height()->baseVal()->value();
    if(bbox)
        filterEffect->setSubRegion(QRect(int(_x * 100.f), int(_y * 100.f), int(_width * 100.f), int(_height * 100.f)));
    else
        filterEffect->setSubRegion(QRect(int(_x), int(_y), int(_width), int(_height)));

    filterEffect->setResult(KDOM::DOMString(result()->baseVal()).qstring());
}

// vim:ts=4:noet
