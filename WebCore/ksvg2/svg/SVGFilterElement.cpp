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

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasResources.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/KCanvasFilters.h>
#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGFilterElement.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedInteger.h"
#include "KCanvasRenderingStyle.h"

using namespace WebCore;

SVGFilterElement::SVGFilterElement(const QualifiedName& tagName, Document *doc)
: SVGStyledElement(tagName, doc), SVGURIReference(), SVGLangSpace(), SVGExternalResourcesRequired()
{
    m_filter = 0;
}

SVGFilterElement::~SVGFilterElement()
{
    delete m_filter;
}

SVGAnimatedEnumeration *SVGFilterElement::filterUnits() const
{
    if (!m_filterUnits) {
        lazy_create<SVGAnimatedEnumeration>(m_filterUnits, this);
        m_filterUnits->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    }

    return m_filterUnits.get();
}

SVGAnimatedEnumeration *SVGFilterElement::primitiveUnits() const
{
    if (!m_primitiveUnits) {
        lazy_create<SVGAnimatedEnumeration>(m_primitiveUnits, this);
        m_primitiveUnits->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
    }

    return m_primitiveUnits.get();
}

SVGAnimatedLength *SVGFilterElement::x() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "-10%" were specified.
    if (!m_x) {
        lazy_create<SVGAnimatedLength>(m_x, this, LM_WIDTH, viewportElement());
        m_x->baseVal()->setValueAsString(String("-10%").impl());
    }

    return m_x.get();
}

SVGAnimatedLength *SVGFilterElement::y() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "-10%" were specified.
    if (!m_y) {
        lazy_create<SVGAnimatedLength>(m_y, this, LM_HEIGHT, viewportElement());
        m_y->baseVal()->setValueAsString(String("-10%").impl());
    }

    return m_y.get();
}

SVGAnimatedLength *SVGFilterElement::width() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "120%" were specified.
    if (!m_width) {
        lazy_create<SVGAnimatedLength>(m_width, this, LM_WIDTH, viewportElement());
        m_width->baseVal()->setValueAsString(String("120%").impl());
    }

    return m_width.get();
}

SVGAnimatedLength *SVGFilterElement::height() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "120%" were specified.
    if (!m_height) {
        lazy_create<SVGAnimatedLength>(m_height, this, LM_HEIGHT, viewportElement());
        m_height->baseVal()->setValueAsString(String("120%").impl());
    }

    return m_height.get();
}

SVGAnimatedInteger *SVGFilterElement::filterResX() const
{
    return lazy_create<SVGAnimatedInteger>(m_filterResX, this);
}

SVGAnimatedInteger *SVGFilterElement::filterResY() const
{
    return lazy_create<SVGAnimatedInteger>(m_filterResY, this);
}

void SVGFilterElement::setFilterRes(unsigned long, unsigned long) const
{
}

void SVGFilterElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::filterUnitsAttr)
    {
        if(value == "userSpaceOnUse")
            filterUnits()->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
        else if(value == "objectBoundingBox")
            filterUnits()->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    }
    else if (attr->name() == SVGNames::primitiveUnitsAttr)
    {
        if(value == "userSpaceOnUse")
            primitiveUnits()->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
        else if(value == "objectBoundingBox")
            primitiveUnits()->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    }
    else if (attr->name() == SVGNames::xAttr)
        x()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::yAttr)
        y()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::widthAttr)
        width()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::heightAttr)
        height()->baseVal()->setValueAsString(value.impl());
    else
    {
        if(SVGURIReference::parseMappedAttribute(attr)) return;
        if(SVGLangSpace::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;

        SVGStyledElement::parseMappedAttribute(attr);
    }
}

KCanvasFilter *SVGFilterElement::canvasResource()
{
    if (!attached())
        return 0;

    if (!m_filter)
        m_filter = static_cast<KCanvasFilter *>(renderingDevice()->createResource(RS_FILTER));

    bool filterBBoxMode = filterUnits()->baseVal() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
    m_filter->setFilterBoundingBoxMode(filterBBoxMode);
    
    x()->baseVal()->setBboxRelative(filterBBoxMode);
    y()->baseVal()->setBboxRelative(filterBBoxMode);
    width()->baseVal()->setBboxRelative(filterBBoxMode);
    height()->baseVal()->setBboxRelative(filterBBoxMode);
    float _x = x()->baseVal()->value(), _y = y()->baseVal()->value();
    float _width = width()->baseVal()->value(), _height = height()->baseVal()->value();
    m_filter->setFilterRect(FloatRect(_x, _y, _width, _height));
    
    bool primitiveBBoxMode = primitiveUnits()->baseVal() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
    m_filter->setEffectBoundingBoxMode(primitiveBBoxMode);
    // FIXME: When does this info get passed to the filters elements?

    // TODO : use switch/case instead?
    m_filter->clearEffects();
    for (Node *n = firstChild(); n != 0; n = n->nextSibling()) {
        SVGElement *element = svg_dynamic_cast(n);
        if(element && element->isFilterEffect()) {
            SVGFilterPrimitiveStandardAttributes *fe = static_cast<SVGFilterPrimitiveStandardAttributes *>(element);
            if (fe->filterEffect())
                m_filter->addFilterEffect(fe->filterEffect());
        }
    }
    return m_filter;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

