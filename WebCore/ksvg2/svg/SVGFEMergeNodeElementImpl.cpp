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
#include <kdom/core/AttrImpl.h>

#include "SVGHelper.h"
#include "SVGFEMergeNodeElementImpl.h"
#include "SVGAnimatedStringImpl.h"

using namespace KSVG;

SVGFEMergeNodeElementImpl::SVGFEMergeNodeElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc) : SVGElementImpl(tagName, doc)
{
}

SVGFEMergeNodeElementImpl::~SVGFEMergeNodeElementImpl()
{
}

SVGAnimatedStringImpl *SVGFEMergeNodeElementImpl::in1() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedStringImpl>(m_in1, dummy);
}

void SVGFEMergeNodeElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    KDOM::DOMString value(attr->value());
    if (attr->name() == SVGNames::inAttr)
        in1()->setBaseVal(value.impl());
    else
        SVGElementImpl::parseMappedAttribute(attr);
}


// vim:ts=4:noet
#endif // SVG_SUPPORT

