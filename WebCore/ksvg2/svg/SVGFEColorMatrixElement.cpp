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
#include <DeprecatedStringList.h>

#include "Attr.h"

#include <kcanvas/KCanvasResources.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingPaintServerGradient.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFEColorMatrixElement.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedNumberList.h"
#include "SVGDOMImplementation.h"

using namespace WebCore;

SVGFEColorMatrixElement::SVGFEColorMatrixElement(const QualifiedName& tagName, Document *doc) : 
SVGFilterPrimitiveStandardAttributes(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFEColorMatrixElement::~SVGFEColorMatrixElement()
{
    delete m_filterEffect;
}

SVGAnimatedString *SVGFEColorMatrixElement::in1() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedString>(m_in1, dummy);
}

SVGAnimatedEnumeration *SVGFEColorMatrixElement::type() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedEnumeration>(m_type, dummy);
}

SVGAnimatedNumberList *SVGFEColorMatrixElement::values() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumberList>(m_values, dummy);
}

void SVGFEColorMatrixElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
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
        values()->baseVal()->parse(value.deprecatedString(), this);
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

KCanvasFEColorMatrix *SVGFEColorMatrixElement::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<KCanvasFEColorMatrix *>(renderingDevice()->createFilterEffect(FE_COLOR_MATRIX));
    if (!m_filterEffect)
        return 0;
        
    m_filterEffect->setIn(String(in1()->baseVal()).deprecatedString());
    setStandardAttributes(m_filterEffect);
    Q3ValueList<float> _values;
    SVGNumberList *numbers = values()->baseVal();
    unsigned int nr = numbers->numberOfItems();
    for(unsigned int i = 0;i < nr;i++)
        _values.append(numbers->getItem(i)->value());
    m_filterEffect->setValues(_values);
    m_filterEffect->setType((KCColorMatrixType)(type()->baseVal() - 1));
    
    return m_filterEffect;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

