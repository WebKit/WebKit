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
#include "Attr.h"

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRectElement.h"
#include "SVGLength.h"

#include "KCanvasRenderingStyle.h"
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingFillPainter.h>
#include <kcanvas/device/KRenderingPaintServerSolid.h>

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
        xBaseValue()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::yAttr)
        yBaseValue()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::rxAttr)
        rxBaseValue()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::ryAttr)
        ryBaseValue()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::widthAttr)
        widthBaseValue()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::heightAttr)
        heightBaseValue()->setValueAsString(value.impl());
    else
    {
        if(SVGTests::parseMappedAttribute(attr)) return;
        if(SVGLangSpace::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

Path SVGRectElement::toPathData() const
{
    float _x = xBaseValue()->value(), _y = yBaseValue()->value();
    float _width = widthBaseValue()->value(), _height = heightBaseValue()->value();

    bool hasRx = hasAttribute(String("rx").impl());
    bool hasRy = hasAttribute(String("ry").impl());
    if(hasRx || hasRy)
    {
        float _rx = hasRx ? rxBaseValue()->value() : ryBaseValue()->value();
        float _ry = hasRy ? ryBaseValue()->value() : rxBaseValue()->value();
        return Path::createRoundedRectangle(FloatRect(_x, _y, _width, _height), FloatSize(_rx, _ry));
    }

    return Path::createRectangle(FloatRect(_x, _y, _width, _height));
}

const SVGStyledElement *SVGRectElement::pushAttributeContext(const SVGStyledElement *context)
{
    // All attribute's contexts are equal (so just take the one from 'x').
    const SVGStyledElement *restore = xBaseValue()->context();

    xBaseValue()->setContext(context);
    yBaseValue()->setContext(context);
    widthBaseValue()->setContext(context);
    heightBaseValue()->setContext(context);
    
    SVGStyledElement::pushAttributeContext(context);
    return restore;
}

bool SVGRectElement::hasPercentageValues() const
{
    if (xBaseValue()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
        yBaseValue()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
        widthBaseValue()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
        heightBaseValue()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
        rxBaseValue()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE ||
        ryBaseValue()->unitType() == SVGLength::SVG_LENGTHTYPE_PERCENTAGE)
        return true;

    return false;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

