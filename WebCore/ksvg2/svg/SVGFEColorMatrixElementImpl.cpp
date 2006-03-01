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
#include <QStringList.h>

#include <kdom/core/AttrImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasResources.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFEColorMatrixElementImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedNumberListImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace WebCore;

SVGFEColorMatrixElementImpl::SVGFEColorMatrixElementImpl(const QualifiedName& tagName, DocumentImpl *doc) : 
SVGFilterPrimitiveStandardAttributesImpl(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFEColorMatrixElementImpl::~SVGFEColorMatrixElementImpl()
{
    delete m_filterEffect;
}

SVGAnimatedStringImpl *SVGFEColorMatrixElementImpl::in1() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedStringImpl>(m_in1, dummy);
}

SVGAnimatedEnumerationImpl *SVGFEColorMatrixElementImpl::type() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedEnumerationImpl>(m_type, dummy);
}

SVGAnimatedNumberListImpl *SVGFEColorMatrixElementImpl::values() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberListImpl>(m_values, dummy);
}

void SVGFEColorMatrixElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    DOMString value(attr->value());
    if (attr->name() == SVGNames::typeAttr)
    {
        if(value == "matrix")
            type()->setBaseVal(SVG_FECOLORMATRIX_TYPE_MATRIX);
        else if(value == "saturate")
            type()->setBaseVal(SVG_FECOLORMATRIX_TYPE_SATURATE);
        else if(value == "hueRotate")
            type()->setBaseVal(SVG_FECOLORMATRIX_TYPE_HUEROTATE);
        else if(value == "luminanceToAlpha")
            type()->setBaseVal(SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA);
    }
    else if (attr->name() == SVGNames::inAttr)
        in1()->setBaseVal(value.impl());
    else if (attr->name() == SVGNames::valuesAttr)
        values()->baseVal()->parse(value.qstring(), this);
    else
        SVGFilterPrimitiveStandardAttributesImpl::parseMappedAttribute(attr);
}

KCanvasFEColorMatrix *SVGFEColorMatrixElementImpl::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<KCanvasFEColorMatrix *>(renderingDevice()->createFilterEffect(FE_COLOR_MATRIX));
    if (!m_filterEffect)
        return 0;
        
    m_filterEffect->setIn(DOMString(in1()->baseVal()).qstring());
    setStandardAttributes(m_filterEffect);
    Q3ValueList<float> _values;
    SVGNumberListImpl *numbers = values()->baseVal();
    unsigned int nr = numbers->numberOfItems();
    for(unsigned int i = 0;i < nr;i++)
        _values.append(numbers->getItem(i)->value());
    m_filterEffect->setValues(_values);
    m_filterEffect->setType((KCColorMatrixType)(type()->baseVal() - 1));
    
    return m_filterEffect;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

