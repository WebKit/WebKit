/*
 Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 
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
#include "SVGMPathElement.h"
#include "SVGPathElement.h"

namespace WebCore {

SVGMPathElement::SVGMPathElement(const QualifiedName& qname, Document* doc)
    : SVGElement(qname, doc)
{
}

SVGMPathElement::~SVGMPathElement()
{
}

void SVGMPathElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (SVGURIReference::parseMappedAttribute(attr))
        return;
    SVGElement::parseMappedAttribute(attr);
}

SVGPathElement* SVGMPathElement::pathElement()
{
    Element* target = document()->getElementById(getTarget(SVGURIReference::href()));
    if (target && target->hasTagName(SVGNames::pathTag))
        return static_cast<SVGPathElement*>(target);
    return 0;
}

} // namespace WebCore
