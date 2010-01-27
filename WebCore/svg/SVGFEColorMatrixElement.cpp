/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFEColorMatrixElement.h"

#include "MappedAttribute.h"
#include "SVGNames.h"
#include "SVGNumberList.h"
#include "SVGResourceFilter.h"

namespace WebCore {

SVGFEColorMatrixElement::SVGFEColorMatrixElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_type(FECOLORMATRIX_TYPE_UNKNOWN)
    , m_values(SVGNumberList::create(SVGNames::valuesAttr))
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

void SVGFEColorMatrixElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGFilterPrimitiveStandardAttributes::synchronizeProperty(attrName);

    if (attrName == anyQName()) {
        synchronizeType();
        synchronizeIn1();
        synchronizeValues();
        return;
    }

    if (attrName == SVGNames::typeAttr)
        synchronizeType();
    else if (attrName == SVGNames::inAttr)
        synchronizeIn1();
    else if (attrName == SVGNames::valuesAttr)
        synchronizeValues();
}

bool SVGFEColorMatrixElement::build(SVGResourceFilter* filterResource)
{
    FilterEffect* input1 = filterResource->builder()->getEffectById(in1());

    if (!input1)
        return false;

    Vector<float> _values;
    SVGNumberList* numbers = values();

    ExceptionCode ec = 0;
    unsigned int nr = numbers->numberOfItems();
    for (unsigned int i = 0;i < nr;i++)
        _values.append(numbers->getItem(i, ec));

    RefPtr<FilterEffect> effect = FEColorMatrix::create(input1, static_cast<ColorMatrixType>(type()), _values);
    filterResource->addFilterEffect(this, effect.release());
    
    return true;
}

} //namespace WebCore

#endif // ENABLE(SVG)
