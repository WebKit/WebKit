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
#ifdef SVG_SUPPORT
#include "SVGRectElement.h"

#include "Attr.h"
#include "KRenderingDevice.h"
#include "KRenderingFillPainter.h"
#include "KRenderingPaintServerSolid.h"
#include "SVGHelper.h"
#include "SVGLength.h"
#include "SVGNames.h"

namespace WebCore {

SVGRectElement::SVGRectElement(const QualifiedName& tagName, Document *doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_x(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_y(new SVGLength(this, LM_HEIGHT, viewportElement()))
    , m_width(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_height(new SVGLength(this, LM_HEIGHT, viewportElement()))
    , m_rx(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_ry(new SVGLength(this, LM_HEIGHT, viewportElement()))
{
}

SVGRectElement::~SVGRectElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGRectElement, SVGLength*, Length, length, X, x, SVGNames::xAttr.localName(), m_x.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGRectElement, SVGLength*, Length, length, Y, y, SVGNames::yAttr.localName(), m_y.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGRectElement, SVGLength*, Length, length, Width, width, SVGNames::widthAttr.localName(), m_width.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGRectElement, SVGLength*, Length, length, Height, height, SVGNames::heightAttr.localName(), m_height.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGRectElement, SVGLength*, Length, length, Rx, rx, SVGNames::rxAttr.localName(), m_rx.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGRectElement, SVGLength*, Length, length, Ry, ry, SVGNames::ryAttr.localName(), m_ry.get())

void SVGRectElement::parseMappedAttribute(MappedAttribute *attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::xAttr)
        xBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::yAttr)
        yBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::rxAttr)
        rxBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::ryAttr)
        ryBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::widthAttr)
        widthBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::heightAttr)
        heightBaseValue()->setValueAsString(value);
    else {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

Path SVGRectElement::toPathData() const
{
    FloatRect rect(x()->value(), y()->value(), width()->value(), height()->value());

    bool hasRx = hasAttribute("rx");
    bool hasRy = hasAttribute("ry");
    if (hasRx || hasRy) {
        float _rx = hasRx ? rx()->value() : ry()->value();
        float _ry = hasRy ? ry()->value() : rx()->value();
        return Path::createRoundedRectangle(rect, FloatSize(_rx, _ry));
    }

    return Path::createRectangle(rect);
}

const SVGStyledElement *SVGRectElement::pushAttributeContext(const SVGStyledElement *context)
{
    // All attribute's contexts are equal (so just take the one from 'x').
    const SVGStyledElement *restore = x()->context();

    x()->setContext(context);
    y()->setContext(context);
    width()->setContext(context);
    height()->setContext(context);
    
    SVGStyledElement::pushAttributeContext(context);
    return restore;
}

bool SVGRectElement::hasPercentageValues() const
{
    if (x()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
        y()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
        width()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
        height()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
        rx()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
        ry()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE)
        return true;

    return false;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

