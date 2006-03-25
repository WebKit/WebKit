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

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRectElement.h"
#include "SVGAnimatedLength.h"

#include "KCanvasRenderingStyle.h"
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingFillPainter.h>
#include <kcanvas/device/KRenderingPaintServerSolid.h>
#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>

namespace WebCore {

SVGRectElement::SVGRectElement(const QualifiedName& tagName, Document *doc)
: SVGStyledTransformableElement(tagName, doc), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired()
{
}

SVGRectElement::~SVGRectElement()
{
}

SVGAnimatedLength *SVGRectElement::x() const
{
    return lazy_create<SVGAnimatedLength>(m_x, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLength *SVGRectElement::y() const
{
    return lazy_create<SVGAnimatedLength>(m_y, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLength *SVGRectElement::width() const
{
    return lazy_create<SVGAnimatedLength>(m_width, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLength *SVGRectElement::height() const
{
    return lazy_create<SVGAnimatedLength>(m_height, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLength *SVGRectElement::rx() const
{
    return lazy_create<SVGAnimatedLength>(m_rx, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLength *SVGRectElement::ry() const
{
    return lazy_create<SVGAnimatedLength>(m_ry, this, LM_HEIGHT, viewportElement());
}

void SVGRectElement::parseMappedAttribute(MappedAttribute *attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::xAttr)
        x()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::yAttr)
        y()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::rxAttr)
        rx()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::ryAttr)
        ry()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::widthAttr)
        width()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::heightAttr)
        height()->baseVal()->setValueAsString(value.impl());
    else
    {
        if(SVGTests::parseMappedAttribute(attr)) return;
        if(SVGLangSpace::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

KCanvasPath* SVGRectElement::toPathData() const
{
    float _x = x()->baseVal()->value(), _y = y()->baseVal()->value();
    float _width = width()->baseVal()->value(), _height = height()->baseVal()->value();

    bool hasRx = hasAttribute(String("rx").impl());
    bool hasRy = hasAttribute(String("ry").impl());
    if(hasRx || hasRy)
    {
        float _rx = hasRx ? rx()->baseVal()->value() : ry()->baseVal()->value();
        float _ry = hasRy ? ry()->baseVal()->value() : rx()->baseVal()->value();
        return KCanvasCreator::self()->createRoundedRectangle(_x, _y, _width, _height, _rx, _ry);
    }

    return KCanvasCreator::self()->createRectangle(_x, _y, _width, _height);
}

const SVGStyledElement *SVGRectElement::pushAttributeContext(const SVGStyledElement *context)
{
    // All attribute's contexts are equal (so just take the one from 'x').
    const SVGStyledElement *restore = x()->baseVal()->context();

    x()->baseVal()->setContext(context);
    y()->baseVal()->setContext(context);
    width()->baseVal()->setContext(context);
    height()->baseVal()->setContext(context);
    
    SVGStyledElement::pushAttributeContext(context);
    return restore;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

