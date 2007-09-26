/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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

#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
#include "SVGFEColorMatrixElement.h"

#include "SVGNames.h"
#include "SVGNumberList.h"
#include "SVGResourceFilter.h"

namespace WebCore {

SVGFEColorMatrixElement::SVGFEColorMatrixElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_type(0)
    , m_values(new SVGNumberList)
    , m_filterEffect(0)
{
}

SVGFEColorMatrixElement::~SVGFEColorMatrixElement()
{
    delete m_filterEffect;
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFEColorMatrixElement, String, String, string, In1, in1, SVGNames::inAttr.localName(), m_in1)
ANIMATED_PROPERTY_DEFINITIONS(SVGFEColorMatrixElement, int, Enumeration, enumeration, Type, type, SVGNames::typeAttr.localName(), m_type)
ANIMATED_PROPERTY_DEFINITIONS(SVGFEColorMatrixElement, SVGNumberList*, NumberList, numberList, Values, values, SVGNames::valuesAttr.localName(), m_values.get())

void SVGFEColorMatrixElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::typeAttr) {
        if (value == "matrix")
            setTypeBaseValue(SVG_FECOLORMATRIX_TYPE_MATRIX);
        else if (value == "saturate")
            setTypeBaseValue(SVG_FECOLORMATRIX_TYPE_SATURATE);
        else if (value == "hueRotate")
            setTypeBaseValue(SVG_FECOLORMATRIX_TYPE_HUEROTATE);
        else if (value == "luminanceToAlpha")
            setTypeBaseValue(SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA);
    }
    else if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else if (attr->name() == SVGNames::valuesAttr)
        valuesBaseValue()->parse(value);
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

SVGFEColorMatrix* SVGFEColorMatrixElement::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<SVGFEColorMatrix*>(SVGResourceFilter::createFilterEffect(FE_COLOR_MATRIX));
    if (!m_filterEffect)
        return 0;
        
    m_filterEffect->setIn(in1());
    setStandardAttributes(m_filterEffect);
    Vector<float> _values;
    SVGNumberList* numbers = values();

    ExceptionCode ec = 0;
    unsigned int nr = numbers->numberOfItems();
    for (unsigned int i = 0;i < nr;i++)
        _values.append(numbers->getItem(i, ec));
    m_filterEffect->setValues(_values);
    m_filterEffect->setType((SVGColorMatrixType) type());
    
    return m_filterEffect;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
