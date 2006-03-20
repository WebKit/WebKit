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
#include "Node.h"
#include <kdom/core/Attr.h>

#include "SVGNames.h"
#include "XLinkNames.h"
#include "SVGHelper.h"
#include "SVGURIReference.h"
#include "SVGStyledElement.h"
#include "SVGAnimatedString.h"

using namespace WebCore;

SVGURIReference::SVGURIReference()
{
}

SVGURIReference::~SVGURIReference()
{
}

SVGAnimatedString *SVGURIReference::href() const
{
    //const SVGStyledElement *context = dynamic_cast<const SVGStyledElement *>(this);
    return lazy_create<SVGAnimatedString>(m_href, (const SVGStyledElement *)0); // FIXME: 0 is a hack
}

bool SVGURIReference::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name().matches(XLinkNames::hrefAttr)) {
        href()->setBaseVal(attr->value().impl());
        return true;
    }

    return false;
}

DeprecatedString SVGURIReference::getTarget(const DeprecatedString &url)
{
    if(url.startsWith(DeprecatedString::fromLatin1("url("))) // URI References, ie. fill:url(#target)
    {
        unsigned int start = url.find('#') + 1;
        unsigned int end = url.findRev(')');

        return url.mid(start, end - start);
    }
    else if(url.find('#') > -1) // format is #target
    {
        unsigned int start = url.find('#') + 1;
        return url.mid(start, url.length() - start);
    }
    else // Normal Reference, ie. style="color-profile:changeColor"
        return url;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

