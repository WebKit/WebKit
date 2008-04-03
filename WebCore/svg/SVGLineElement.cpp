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
#include "SVGLineElement.h"

#include "FloatPoint.h"
#include "RenderPath.h"
#include "SVGLength.h"
#include "SVGNames.h"

namespace WebCore {

SVGLineElement::SVGLineElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_x1(this, LengthModeWidth)
    , m_y1(this, LengthModeHeight)
    , m_x2(this, LengthModeWidth)
    , m_y2(this, LengthModeHeight)
{
}

SVGLineElement::~SVGLineElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGLineElement, SVGLength, Length, length, X1, x1, SVGNames::x1Attr, m_x1)
ANIMATED_PROPERTY_DEFINITIONS(SVGLineElement, SVGLength, Length, length, Y1, y1, SVGNames::y1Attr, m_y1)
ANIMATED_PROPERTY_DEFINITIONS(SVGLineElement, SVGLength, Length, length, X2, x2, SVGNames::x2Attr, m_x2)
ANIMATED_PROPERTY_DEFINITIONS(SVGLineElement, SVGLength, Length, length, Y2, y2, SVGNames::y2Attr, m_y2)

void SVGLineElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::x1Attr)
        setX1BaseValue(SVGLength(this, LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::y1Attr)
        setY1BaseValue(SVGLength(this, LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::x2Attr)
        setX2BaseValue(SVGLength(this, LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::y2Attr)
        setY2BaseValue(SVGLength(this, LengthModeHeight, attr->value()));
    else
    {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

void SVGLineElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGStyledTransformableElement::svgAttributeChanged(attrName);

    if (!renderer())
        return;

    if (attrName == SVGNames::x1Attr || attrName == SVGNames::y1Attr ||
        attrName == SVGNames::x2Attr || attrName == SVGNames::y2Attr ||
        SVGTests::isKnownAttribute(attrName) ||
        SVGLangSpace::isKnownAttribute(attrName) ||
        SVGExternalResourcesRequired::isKnownAttribute(attrName) ||
        SVGStyledTransformableElement::isKnownAttribute(attrName))
        renderer()->setNeedsLayout(true);
}

Path SVGLineElement::toPathData() const
{
    return Path::createLine(FloatPoint(x1().value(), y1().value()),
                            FloatPoint(x2().value(), y2().value()));
}

bool SVGLineElement::hasRelativeValues() const
{
    return (x1().isRelative() || y1().isRelative() ||
            x2().isRelative() || y2().isRelative());
}

}

#endif // ENABLE(SVG)
