/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFEColorMatrixElement.h"

#include "MappedAttribute.h"
#include "SVGNames.h"
#include "SVGNumberList.h"
#include "SVGResourceFilter.h"

namespace WebCore {

SVGFEColorMatrixElement::SVGFEColorMatrixElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_in1(this, SVGNames::inAttr)
    , m_type(this, SVGNames::typeAttr, FECOLORMATRIX_TYPE_UNKNOWN)
    , m_values(this, SVGNames::valuesAttr, SVGNumberList::create(SVGNames::valuesAttr))
    , m_filterEffect(0)
{
}

SVGFEColorMatrixElement::~SVGFEColorMatrixElement()
{
}

void SVGFEColorMatrixElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::typeAttr) {
        if (value == "matrix")
            setTypeBaseValue(FECOLORMATRIX_TYPE_MATRIX);
        else if (value == "saturate")
            setTypeBaseValue(FECOLORMATRIX_TYPE_SATURATE);
        else if (value == "hueRotate")
            setTypeBaseValue(FECOLORMATRIX_TYPE_HUEROTATE);
        else if (value == "luminanceToAlpha")
            setTypeBaseValue(FECOLORMATRIX_TYPE_LUMINANCETOALPHA);
    }
    else if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else if (attr->name() == SVGNames::valuesAttr)
        valuesBaseValue()->parse(value);
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

SVGFilterEffect* SVGFEColorMatrixElement::filterEffect(SVGResourceFilter* filter) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

bool SVGFEColorMatrixElement::build(FilterBuilder* builder)
{
    FilterEffect* input1 = builder->getEffectById(in1());

    if(!input1)
        return false;

    Vector<float> _values;
    SVGNumberList* numbers = values();

    ExceptionCode ec = 0;
    unsigned int nr = numbers->numberOfItems();
    for (unsigned int i = 0;i < nr;i++)
        _values.append(numbers->getItem(i, ec));

    builder->add(result(), FEColorMatrix::create(input1, static_cast<ColorMatrixType> (type()), _values));
    
    return true;
}

} //namespace WebCore

#endif // ENABLE(SVG)
