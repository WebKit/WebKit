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
#include "SVGCircleElement.h"

#include "FloatPoint.h"
#include "SVGNames.h"

namespace WebCore {

SVGCircleElement::SVGCircleElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_cx(SVGLength(this, LengthModeWidth))
    , m_cy(SVGLength(this, LengthModeHeight))
    , m_r(SVGLength(this, LengthModeOther))
{
}

SVGCircleElement::~SVGCircleElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGCircleElement, SVGLength, Length, length, Cx, cx, SVGNames::cxAttr.localName(), m_cx)
ANIMATED_PROPERTY_DEFINITIONS(SVGCircleElement, SVGLength, Length, length, Cy, cy, SVGNames::cyAttr.localName(), m_cy)
ANIMATED_PROPERTY_DEFINITIONS(SVGCircleElement, SVGLength, Length, length, R, r, SVGNames::rAttr.localName(), m_r)

void SVGCircleElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::cxAttr)
        setCxBaseValue(SVGLength(this, LengthModeWidth, attr->value()));       
    else if (attr->name() == SVGNames::cyAttr)
        setCyBaseValue(SVGLength(this, LengthModeHeight, attr->value()));
    else if (attr->name() == SVGNames::rAttr) {
        setRBaseValue(SVGLength(this, LengthModeOther, attr->value()));
        if (r().value() < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for circle <r> is not allowed");
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

void SVGCircleElement::notifyAttributeChange() const
{
    if (!ownerDocument()->parsing())
        rebuildRenderer();

    SVGStyledElement::notifyAttributeChange();
}

Path SVGCircleElement::toPathData() const
{
    return Path::createCircle(FloatPoint(cx().value(), cy().value()), r().value());
}

bool SVGCircleElement::hasRelativeValues() const
{
    return (cx().isRelative() || cy().isRelative() || r().isRelative());
}
 
}

#endif // ENABLE(SVG)

// vim:ts=4:noet
