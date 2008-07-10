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
#include "SVGFEGaussianBlurElement.h"

#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGResourceFilter.h"

namespace WebCore {

SVGFEGaussianBlurElement::SVGFEGaussianBlurElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_stdDeviationX(0.0f)
    , m_stdDeviationY(0.0f)
    , m_filterEffect(0)
{
}

SVGFEGaussianBlurElement::~SVGFEGaussianBlurElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFEGaussianBlurElement, String, In1, in1, SVGNames::inAttr)
ANIMATED_PROPERTY_DEFINITIONS_WITH_CUSTOM_IDENTIFIER(SVGFEGaussianBlurElement, float, StdDeviationX, stdDeviationX, SVGNames::stdDeviationAttr, "stdDeviationX")
ANIMATED_PROPERTY_DEFINITIONS_WITH_CUSTOM_IDENTIFIER(SVGFEGaussianBlurElement, float, StdDeviationY, stdDeviationY, SVGNames::stdDeviationAttr, "stdDeviationY")

void SVGFEGaussianBlurElement::setStdDeviation(float stdDeviationX, float stdDeviationY)
{
}

void SVGFEGaussianBlurElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::stdDeviationAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            setStdDeviationXBaseValue(x);
            setStdDeviationYBaseValue(y);
        }
    } else if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

SVGFilterEffect* SVGFEGaussianBlurElement::filterEffect(SVGResourceFilter* filter) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

bool SVGFEGaussianBlurElement::build(FilterBuilder* builder)
{
    FilterEffect* input1 = builder->getEffectById(in1());

    if(!input1)
        return false;

    builder->add(result(), FEGaussianBlur::create(input1, stdDeviationX(), stdDeviationY()));

    return true;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
