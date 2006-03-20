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
#include "SVGTextContentElement.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedEnumeration.h"

using namespace WebCore;

SVGTextContentElement::SVGTextContentElement(const QualifiedName& tagName, Document *doc)
: SVGStyledElement(tagName, doc), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired()
{
}

SVGTextContentElement::~SVGTextContentElement()
{
}

SVGAnimatedLength *SVGTextContentElement::textLength() const
{
    return lazy_create<SVGAnimatedLength>(m_textLength, this, LM_WIDTH);
}

SVGAnimatedEnumeration *SVGTextContentElement::lengthAdjust() const
{
    return lazy_create<SVGAnimatedEnumeration>(m_lengthAdjust, this);
}

long SVGTextContentElement::getNumberOfChars() const
{
    return 0;
}

float SVGTextContentElement::getComputedTextLength() const
{
    return 0.;
}

float SVGTextContentElement::getSubStringLength(unsigned long charnum, unsigned long nchars) const
{
    return 0.;
}

SVGPoint *SVGTextContentElement::getStartPositionOfChar(unsigned long charnum) const
{
    return 0;
}

SVGPoint *SVGTextContentElement::getEndPositionOfChar(unsigned long charnum) const
{
    return 0;
}

SVGRect *SVGTextContentElement::getExtentOfChar(unsigned long charnum) const
{
    return 0;
}

float SVGTextContentElement::getRotationOfChar(unsigned long charnum) const
{
    return 0.;
}

long SVGTextContentElement::getCharNumAtPosition(SVGPoint *point) const
{
    return 0;
}

void SVGTextContentElement::selectSubString(unsigned long charnum, unsigned long nchars) const
{
}

void SVGTextContentElement::parseMappedAttribute(MappedAttribute *attr)
{
    String value(attr->value());
    
    //if (attr->name() == SVGNames::lengthAdjustAttr)
    //    x()->baseVal()->setValueAsString(value.impl());
    //else
    {
        if(SVGTests::parseMappedAttribute(attr)) return;
        if(SVGLangSpace::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;

        SVGStyledElement::parseMappedAttribute(attr);
    }
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

