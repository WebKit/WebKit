/*
    Copyright (C) 2009 Dirk Schulze <krit@webkit.org>

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
#include "SVGFEMorphologyElement.h"

#include "MappedAttribute.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGResourceFilter.h"

namespace WebCore {

char SVGRadiusXAttrIdentifier[] = "SVGRadiusXAttr";
char SVGRadiusYAttrIdentifier[] = "SVGRadiusYAttr";

SVGFEMorphologyElement::SVGFEMorphologyElement(const QualifiedName& tagName, Document* document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
    , m__operator(FEMORPHOLOGY_OPERATOR_ERODE)
{
}

SVGFEMorphologyElement::~SVGFEMorphologyElement()
{
}

void SVGFEMorphologyElement::setRadius(float, float)
{
    // FIXME: Needs an implementation.
}

void SVGFEMorphologyElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::operatorAttr) {
        if (value == "erode")
            set_operatorBaseValue(FEMORPHOLOGY_OPERATOR_ERODE);
        else if (value == "dilate")
            set_operatorBaseValue(FEMORPHOLOGY_OPERATOR_DILATE);
    } else if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else if (attr->name() == SVGNames::radiusAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            setRadiusXBaseValue(x);
            setRadiusYBaseValue(y);
        }
    } else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

void SVGFEMorphologyElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGFilterPrimitiveStandardAttributes::synchronizeProperty(attrName);

    if (attrName == anyQName()) {
        synchronize_operator();
        synchronizeIn1();
        synchronizeRadiusX();
        synchronizeRadiusY();
        return;
    }

    if (attrName == SVGNames::operatorAttr)
        synchronize_operator();
    else if (attrName == SVGNames::inAttr)
        synchronizeIn1();
    else if (attrName == SVGNames::radiusAttr) {
        synchronizeRadiusX();
        synchronizeRadiusY();
    }
}

bool SVGFEMorphologyElement::build(SVGResourceFilter* filterResource)
{
    FilterEffect* input1 = filterResource->builder()->getEffectById(in1());

    if (!input1)
        return false;

    RefPtr<FilterEffect> effect = FEMorphology::create(input1, static_cast<MorphologyOperatorType>(_operator()), radiusX(), radiusY());
    filterResource->addFilterEffect(this, effect.release());
    
    return true;
}

} //namespace WebCore

#endif // ENABLE(SVG)
