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
#include <kdom/core/NodeImpl.h>
#include <kdom/core/AttrImpl.h>

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGURIReferenceImpl.h"
#include "SVGStyledElementImpl.h"
#include "SVGAnimatedStringImpl.h"

using namespace KSVG;

SVGURIReferenceImpl::SVGURIReferenceImpl()
{
    m_href = 0;
}

SVGURIReferenceImpl::~SVGURIReferenceImpl()
{
    if(m_href)
        m_href->deref();
}

SVGAnimatedStringImpl *SVGURIReferenceImpl::href() const
{
    //const SVGStyledElementImpl *context = dynamic_cast<const SVGStyledElementImpl *>(this);
    return lazy_create<SVGAnimatedStringImpl>(m_href, (const SVGStyledElementImpl *)0); // FIXME: 0 is a hack
}

bool SVGURIReferenceImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    if (attr->name() == SVGNames::hrefAttr) // || attr->name() == XLinkNames::hrefAttr)
    {
        href()->setBaseVal(attr->value().impl());
        return true;
    }

    return false;
}

QString SVGURIReferenceImpl::getTarget(const QString &url)
{
    if(url.startsWith(QString::fromLatin1("url("))) // URI References, ie. fill:url(#target)
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
