/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "SVGEllipseElement.h"

#include "FloatPoint.h"
#include "RenderPath.h"
#include "SVGLength.h"
#include "SVGNames.h"

namespace WebCore {

SVGEllipseElement::SVGEllipseElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_cx(this, LengthModeWidth)
    , m_cy(this, LengthModeHeight)
    , m_rx(this, LengthModeWidth)
    , m_ry(this, LengthModeHeight)
{
}    

SVGEllipseElement::~SVGEllipseElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGEllipseElement, SVGLength, Length, length, Cx, cx, SVGNames::cxAttr, m_cx)
ANIMATED_PROPERTY_DEFINITIONS(SVGEllipseElement, SVGLength, Length, length, Cy, cy, SVGNames::cyAttr, m_cy)
ANIMATED_PROPERTY_DEFINITIONS(SVGEllipseElement, SVGLength, Length, length, Rx, rx, SVGNames::rxAttr, m_rx)
ANIMATED_PROPERTY_DEFINITIONS(SVGEllipseElement, SVGLength, Length, length, Ry, ry, SVGNames::ryAttr, m_ry)

void SVGEllipseElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::cxAttr)
        setCxBaseValue(SVGLength(this, LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::cyAttr)
        setCyBaseValue(SVGLength(this, LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::rxAttr) {
        setRxBaseValue(SVGLength(this, LengthModeWidth, attr->value()));
        if (rx().value() < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for ellipse <rx> is not allowed");
    } else if (attr->name() == SVGNames::ryAttr) {
        setRyBaseValue(SVGLength(this, LengthModeHeight, attr->value()));
        if (ry().value() < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for ellipse <ry> is not allowed");
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

void SVGEllipseElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGStyledTransformableElement::svgAttributeChanged(attrName);

    if (!renderer())
        return;

    if (attrName == SVGNames::cxAttr || attrName == SVGNames::cyAttr ||
        attrName == SVGNames::rxAttr || attrName == SVGNames::ryAttr ||
        SVGTests::isKnownAttribute(attrName) ||
        SVGLangSpace::isKnownAttribute(attrName) ||
        SVGExternalResourcesRequired::isKnownAttribute(attrName) ||
        SVGStyledTransformableElement::isKnownAttribute(attrName))
        renderer()->setNeedsLayout(true);
}

Path SVGEllipseElement::toPathData() const
{
    return Path::createEllipse(FloatPoint(cx().value(), cy().value()),
                               rx().value(), ry().value());
}
 
bool SVGEllipseElement::hasRelativeValues() const
{
    return (cx().isRelative() || cy().isRelative() ||
            rx().isRelative() || ry().isRelative());
}

}

#endif // ENABLE(SVG)
