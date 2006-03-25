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
#include <kdom/Namespace.h>
#include "Document.h"

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGPointList.h"
#include "SVGPolyElement.h"
#include "SVGDOMImplementation.h"

using namespace WebCore;

SVGPolyElement::SVGPolyElement(const QualifiedName& tagName, Document *doc)
: SVGStyledTransformableElement(tagName, doc), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGAnimatedPoints(), SVGPolyParser()
{
}

SVGPolyElement::~SVGPolyElement()
{
}

SVGPointList *SVGPolyElement::points() const
{
    return lazy_create<SVGPointList>(m_points, this);
}

SVGPointList *SVGPolyElement::animatedPoints() const
{
    return 0;
}

void SVGPolyElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == SVGNames::pointsAttr)
        parsePoints(attr->value().deprecatedString());
    else
    {
        if(SVGTests::parseMappedAttribute(attr)) return;
        if(SVGLangSpace::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

void SVGPolyElement::svgPolyTo(double x1, double y1, int) const
{
    points()->appendItem(new SVGPoint(x1, y1, this));
}

void SVGPolyElement::notifyAttributeChange() const
{
    static bool ignoreNotifications = false;
    if (ignoreNotifications || ownerDocument()->parsing())
        return;

    SVGStyledElement::notifyAttributeChange();

    // Spec: Additionally, the 'points' attribute on the original element
    // accessed via the XML DOM (e.g., using the getAttribute() method call)
    // will reflect any changes made to points.
    String _points;
    int len = points()->numberOfItems();
    for (int i = 0; i < len; ++i) {
        SVGPoint *p = points()->getItem(i);
        _points += DeprecatedString("%1 %2 ").arg(p->x()).arg(p->y());
    }

    String p("points");
    RefPtr<Attr> attr = const_cast<SVGPolyElement *>(this)->getAttributeNode(p.impl());
    if (attr) {
        ExceptionCode ec;
        ignoreNotifications = true; // prevent recursion.
        attr->setValue(_points, ec);
        ignoreNotifications = false;
    }
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

