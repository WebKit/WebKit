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
#include <kdom/Namespace.h>
#include "DocumentImpl.h"

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGPointListImpl.h"
#include "SVGPolyElementImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace WebCore;

SVGPolyElementImpl::SVGPolyElementImpl(const QualifiedName& tagName, DocumentImpl *doc)
: SVGStyledTransformableElementImpl(tagName, doc), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl(), SVGAnimatedPointsImpl(), SVGPolyParser()
{
}

SVGPolyElementImpl::~SVGPolyElementImpl()
{
}

SVGPointListImpl *SVGPolyElementImpl::points() const
{
    return lazy_create<SVGPointListImpl>(m_points, this);
}

SVGPointListImpl *SVGPolyElementImpl::animatedPoints() const
{
    return 0;
}

void SVGPolyElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == SVGNames::pointsAttr)
        parsePoints(DOMString(attr->value()).qstring());
    else
    {
        if(SVGTestsImpl::parseMappedAttribute(attr)) return;
        if(SVGLangSpaceImpl::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequiredImpl::parseMappedAttribute(attr)) return;
        SVGStyledTransformableElementImpl::parseMappedAttribute(attr);
    }
}

void SVGPolyElementImpl::svgPolyTo(double x1, double y1, int) const
{
    points()->appendItem(new SVGPointImpl(x1, y1, this));
}

void SVGPolyElementImpl::notifyAttributeChange() const
{
    static bool ignoreNotifications = false;
    if (ignoreNotifications || ownerDocument()->parsing())
        return;

    SVGStyledElementImpl::notifyAttributeChange();

    // Spec: Additionally, the 'points' attribute on the original element
    // accessed via the XML DOM (e.g., using the getAttribute() method call)
    // will reflect any changes made to points.
    DOMString _points;
    int len = points()->numberOfItems();
    for (int i = 0; i < len; ++i) {
        SVGPointImpl *p = points()->getItem(i);
        _points += QString("%1 %2 ").arg(p->x()).arg(p->y());
    }

    DOMString p("points");
    RefPtr<AttrImpl> attr = const_cast<SVGPolyElementImpl *>(this)->getAttributeNode(p.impl());
    if (attr) {
        int exceptionCode;
        ignoreNotifications = true; // prevent recursion.
        attr->setValue(_points, exceptionCode);
        ignoreNotifications = false;
    }
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

