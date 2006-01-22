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

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasResources.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/KCanvasFilters.h>
#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGFilterElementImpl.h"
#include "SVGFilterPrimitiveStandardAttributesImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedIntegerImpl.h"
#include "KCanvasRenderingStyle.h"

using namespace KSVG;

SVGFilterElementImpl::SVGFilterElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc)
: SVGStyledElementImpl(tagName, doc), SVGURIReferenceImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl()
{
    m_filter = 0;
}

SVGFilterElementImpl::~SVGFilterElementImpl()
{
    delete m_filter;
}

SVGAnimatedEnumerationImpl *SVGFilterElementImpl::filterUnits() const
{
    if (!m_filterUnits) {
        lazy_create<SVGAnimatedEnumerationImpl>(m_filterUnits, this);
        m_filterUnits->setBaseVal(SVG_UNIT_TYPE_OBJECTBOUNDINGBOX);
    }

    return m_filterUnits.get();
}

SVGAnimatedEnumerationImpl *SVGFilterElementImpl::primitiveUnits() const
{
    if (!m_primitiveUnits) {
        lazy_create<SVGAnimatedEnumerationImpl>(m_primitiveUnits, this);
        m_primitiveUnits->setBaseVal(SVG_UNIT_TYPE_USERSPACEONUSE);
    }

    return m_primitiveUnits.get();
}

SVGAnimatedLengthImpl *SVGFilterElementImpl::x() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "-10%" were specified.
    if (!m_x) {
        lazy_create<SVGAnimatedLengthImpl>(m_x, this, LM_WIDTH, viewportElement());
        m_x->baseVal()->setValueAsString(KDOM::DOMString("-10%").impl());
    }

    return m_x.get();
}

SVGAnimatedLengthImpl *SVGFilterElementImpl::y() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "-10%" were specified.
    if (!m_y) {
        lazy_create<SVGAnimatedLengthImpl>(m_y, this, LM_HEIGHT, viewportElement());
        m_y->baseVal()->setValueAsString(KDOM::DOMString("-10%").impl());
    }

    return m_y.get();
}

SVGAnimatedLengthImpl *SVGFilterElementImpl::width() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "120%" were specified.
    if (!m_width) {
        lazy_create<SVGAnimatedLengthImpl>(m_width, this, LM_WIDTH, viewportElement());
        m_width->baseVal()->setValueAsString(KDOM::DOMString("120%").impl());
    }

    return m_width.get();
}

SVGAnimatedLengthImpl *SVGFilterElementImpl::height() const
{
    // Spec : If the attribute is not specified, the effect is as if a value of "120%" were specified.
    if (!m_height) {
        lazy_create<SVGAnimatedLengthImpl>(m_height, this, LM_HEIGHT, viewportElement());
        m_height->baseVal()->setValueAsString(KDOM::DOMString("120%").impl());
    }

    return m_height.get();
}

SVGAnimatedIntegerImpl *SVGFilterElementImpl::filterResX() const
{
    return lazy_create<SVGAnimatedIntegerImpl>(m_filterResX, this);
}

SVGAnimatedIntegerImpl *SVGFilterElementImpl::filterResY() const
{
    return lazy_create<SVGAnimatedIntegerImpl>(m_filterResY, this);
}

void SVGFilterElementImpl::setFilterRes(unsigned long, unsigned long) const
{
}

void SVGFilterElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    KDOM::DOMString value(attr->value());
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
        if(SVGURIReferenceImpl::parseMappedAttribute(attr)) return;
        if(SVGLangSpaceImpl::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequiredImpl::parseMappedAttribute(attr)) return;

        SVGStyledElementImpl::parseMappedAttribute(attr);
    }
}

KCanvasFilter *SVGFilterElementImpl::canvasResource()
{
    if(!canvas())
        return 0;

    if(!m_filter)
        m_filter = static_cast<KCanvasFilter *>(QPainter::renderingDevice()->createResource(RS_FILTER));

    bool filterBBoxMode = filterUnits()->baseVal() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
    m_filter->setFilterBoundingBoxMode(filterBBoxMode);
    
    x()->baseVal()->setBboxRelative(filterBBoxMode);
    y()->baseVal()->setBboxRelative(filterBBoxMode);
    width()->baseVal()->setBboxRelative(filterBBoxMode);
    height()->baseVal()->setBboxRelative(filterBBoxMode);
    float _x = x()->baseVal()->value(), _y = y()->baseVal()->value();
    float _width = width()->baseVal()->value(), _height = height()->baseVal()->value();
    if(filterBBoxMode)
        m_filter->setFilterRect(FloatRect(_x * 100.f, _y * 100.f, _width * 100.f, _height * 100.f));
    else
        m_filter->setFilterRect(FloatRect(_x, _y, _width, _height));
    
    bool primitiveBBoxMode = primitiveUnits()->baseVal() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
    m_filter->setEffectBoundingBoxMode(primitiveBBoxMode);
    // FIXME: When does this info get passed to the filters elements?

    // TODO : use switch/case instead?
    for(KDOM::NodeImpl *n = firstChild(); n != 0; n = n->nextSibling())
    {
        SVGElementImpl *element = svg_dynamic_cast(n);
        if(element && element->isFilterEffect()) {
            SVGFilterPrimitiveStandardAttributesImpl *fe = static_cast<SVGFilterPrimitiveStandardAttributesImpl *>(element);
            if (fe->filterEffect())
                m_filter->addFilterEffect(fe->filterEffect());
        }
    }
    return m_filter;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

