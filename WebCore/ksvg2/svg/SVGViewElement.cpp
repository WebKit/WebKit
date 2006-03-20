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
#include "PlatformString.h"
#include <kdom/core/Attr.h>
#include <kdom/core/NamedAttrMap.h>

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGStringList.h"
#include "SVGViewElement.h"
#include "SVGFitToViewBox.h"
#include "SVGZoomAndPan.h"

using namespace WebCore;

SVGViewElement::SVGViewElement(const QualifiedName& tagName, Document *doc)
: SVGStyledElement(tagName, doc), SVGExternalResourcesRequired(),
SVGFitToViewBox(), SVGZoomAndPan()
{
}

SVGViewElement::~SVGViewElement()
{
}

SVGStringList *SVGViewElement::viewTarget() const
{
    return lazy_create<SVGStringList>(m_viewTarget);
}

void SVGViewElement::parseMappedAttribute(MappedAttribute *attr)
{
    String value(attr->value());
    if (attr->name() == SVGNames::viewTargetAttr) {
        viewTarget()->reset(value.deprecatedString());
    } else {
        if(SVGExternalResourcesRequired::parseMappedAttribute(attr)
           || SVGFitToViewBox::parseMappedAttribute(attr)
           || SVGZoomAndPan::parseMappedAttribute(attr))
            return;

        SVGStyledElement::parseMappedAttribute(attr);
    }
}

#endif // SVG_SUPPORT

