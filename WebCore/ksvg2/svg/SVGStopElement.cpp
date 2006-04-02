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
#include "Document.h"

#include <kcanvas/device/KRenderingPaintServerGradient.h>

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGStopElement.h"
#include "SVGAnimatedNumber.h"
#include "SVGDOMImplementation.h"

using namespace WebCore;

SVGStopElement::SVGStopElement(const QualifiedName& tagName, Document *doc) : SVGStyledElement(tagName, doc)
{
}

SVGStopElement::~SVGStopElement()
{
}

SVGAnimatedNumber *SVGStopElement::offset() const
{
    return lazy_create<SVGAnimatedNumber>(m_offset, this);
}

void SVGStopElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::offsetAttr) {
        if(value.endsWith("%"))
            offset()->setBaseVal(value.deprecatedString().left(value.length() - 1).toDouble() / 100.);
        else
            offset()->setBaseVal(value.deprecatedString().toDouble());
    } else
        SVGStyledElement::parseMappedAttribute(attr);

    if (!ownerDocument()->parsing() && attached()) {
        recalcStyle(Force);
        
        SVGStyledElement *parentStyled = static_cast<SVGStyledElement *>(parentNode());
        if(parentStyled)
            parentStyled->notifyAttributeChange();
    }
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

