/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGURIReference.h"

#include "MappedAttribute.h"

namespace WebCore {

SVGURIReference::SVGURIReference()
{
}

SVGURIReference::~SVGURIReference()
{
}

bool SVGURIReference::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name().matches(XLinkNames::hrefAttr)) {
        setHrefBaseValue(attr->value());
        return true;
    }

    return false;
}

bool SVGURIReference::isKnownAttribute(const QualifiedName& attrName)
{
    return attrName.matches(XLinkNames::hrefAttr);
}

String SVGURIReference::getTarget(const String& url)
{
    if (url.startsWith("url(")) { // URI References, ie. fill:url(#target)
        unsigned int start = url.find('#') + 1;
        unsigned int end = url.reverseFind(')');

        return url.substring(start, end - start);
    } else if (url.find('#') > -1) { // format is #target
        unsigned int start = url.find('#') + 1;
        return url.substring(start, url.length() - start);
    } else // Normal Reference, ie. style="color-profile:changeColor"
        return url;
}

}

#endif // ENABLE(SVG)
