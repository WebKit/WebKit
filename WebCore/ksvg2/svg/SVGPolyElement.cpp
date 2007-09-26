/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGPolyElement.h"

#include "Document.h"
#include "FloatPoint.h"
#include "SVGNames.h"
#include "SVGPointList.h"

namespace WebCore {

SVGPolyElement::SVGPolyElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , SVGAnimatedPoints()
    , SVGPolyParser()
{
    m_ignoreAttributeChanges = false;
}

SVGPolyElement::~SVGPolyElement()
{
}

SVGPointList* SVGPolyElement::points() const
{
    if (!m_points)
        m_points = new SVGPointList(this);

    return m_points.get();
}

SVGPointList* SVGPolyElement::animatedPoints() const
{
    // FIXME!
    return 0;
}

void SVGPolyElement::parseMappedAttribute(MappedAttribute* attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::pointsAttr) {
        ExceptionCode ec = 0;
        points()->clear(ec);
        if (!parsePoints(value) && !m_ignoreAttributeChanges) {
            points()->clear(ec);
            document()->accessSVGExtensions()->reportError("Problem parsing points=\"" + value + "\"");
        }
    } else {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

void SVGPolyElement::svgPolyTo(double x1, double y1, int) const
{
    ExceptionCode ec = 0;
    points()->appendItem(FloatPoint::narrowPrecision(x1, y1), ec);
}

void SVGPolyElement::notifyAttributeChange() const
{
    if (m_ignoreAttributeChanges || ownerDocument()->parsing())
        return;

    m_ignoreAttributeChanges = true;
    rebuildRenderer();

    ExceptionCode ec = 0;

    // Spec: Additionally, the 'points' attribute on the original element
    // accessed via the XML DOM (e.g., using the getAttribute() method call)
    // will reflect any changes made to points.
    String _points;
    int len = points()->numberOfItems();
    for (int i = 0; i < len; ++i) {
        FloatPoint p = points()->getItem(i, ec);
        _points += String::format("%.6lg %.6lg ", p.x(), p.y());
    }
    
    RefPtr<Attr> attr = const_cast<SVGPolyElement*>(this)->getAttributeNode(SVGNames::pointsAttr.localName());
    if (attr) {
        ExceptionCode ec = 0;
        attr->setValue(_points, ec);
    }

    m_ignoreAttributeChanges = false;

    SVGStyledElement::notifyAttributeChange();
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
