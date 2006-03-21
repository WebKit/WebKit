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
#include <kdom/core/Attr.h>

#include "SVGNames.h"
#include "SVGHelper.h"
//#include "SVGDocument.h"
#include "SVGTextPositioningElement.h"
#include "SVGAnimatedLengthList.h"
#include "SVGAnimatedNumberList.h"

using namespace WebCore;

SVGTextPositioningElement::SVGTextPositioningElement(const QualifiedName& tagName, Document *doc)
: SVGTextContentElement(tagName, doc)
{
}

SVGTextPositioningElement::~SVGTextPositioningElement()
{
}

SVGAnimatedLengthList *SVGTextPositioningElement::x() const
{
    return lazy_create<SVGAnimatedLengthList>(m_x, this);
}

SVGAnimatedLengthList *SVGTextPositioningElement::y() const
{
    return lazy_create<SVGAnimatedLengthList>(m_y, this);
}

SVGAnimatedLengthList *SVGTextPositioningElement::dx() const
{
    return lazy_create<SVGAnimatedLengthList>(m_dx, this);
}

SVGAnimatedLengthList *SVGTextPositioningElement::dy() const
{
    return lazy_create<SVGAnimatedLengthList>(m_dy, this);
}

SVGAnimatedNumberList *SVGTextPositioningElement::rotate() const
{
    return lazy_create<SVGAnimatedNumberList>(m_rotate, this);
}

void SVGTextPositioningElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
    
    if (attr->name() == SVGNames::xAttr)
        x()->baseVal()->parse(value.deprecatedString(), this, LM_WIDTH);
    else if (attr->name() == SVGNames::yAttr)
        y()->baseVal()->parse(value.deprecatedString(), this, LM_HEIGHT);
    else if (attr->name() == SVGNames::dxAttr)
        dx()->baseVal()->parse(value.deprecatedString(), this, LM_WIDTH);
    else if (attr->name() == SVGNames::dyAttr)
        dy()->baseVal()->parse(value.deprecatedString(), this, LM_HEIGHT);
    else if (attr->name() == SVGNames::rotateAttr)
        rotate()->baseVal()->parse(value.deprecatedString(), this);
    else
        SVGTextContentElement::parseMappedAttribute(attr);
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

