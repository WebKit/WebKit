/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGFEMorphologyElement.h"

#include "FilterEffect.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEMorphologyElement);

inline SVGFEMorphologyElement::SVGFEMorphologyElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feMorphologyTag));
    
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFEMorphologyElement::m_in1>();
        PropertyRegistry::registerProperty<SVGNames::operatorAttr, MorphologyOperatorType, &SVGFEMorphologyElement::m_svgOperator>();
        PropertyRegistry::registerProperty<SVGNames::radiusAttr, &SVGFEMorphologyElement::m_radiusX, &SVGFEMorphologyElement::m_radiusY>();
    });
}

Ref<SVGFEMorphologyElement> SVGFEMorphologyElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEMorphologyElement(tagName, document));
}

void SVGFEMorphologyElement::setRadius(float x, float y)
{
    m_radiusX->setBaseValInternal(x);
    m_radiusY->setBaseValInternal(y);
    invalidate();
}

void SVGFEMorphologyElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::operatorAttr) {
        MorphologyOperatorType propertyValue = SVGPropertyTraits<MorphologyOperatorType>::fromString(value);
        if (propertyValue > 0)
            m_svgOperator->setBaseValInternal<MorphologyOperatorType>(propertyValue);
        return;
    }

    if (name == SVGNames::inAttr) {
        m_in1->setBaseValInternal(value);
        return;
    }

    if (name == SVGNames::radiusAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            m_radiusX->setBaseValInternal(x);
            m_radiusY->setBaseValInternal(y);
        }
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

bool SVGFEMorphologyElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    FEMorphology* morphology = static_cast<FEMorphology*>(effect);
    if (attrName == SVGNames::operatorAttr)
        return morphology->setMorphologyOperator(svgOperator());
    if (attrName == SVGNames::radiusAttr) {
        // Both setRadius functions should be evaluated separately.
        bool isRadiusXChanged = morphology->setRadiusX(radiusX());
        bool isRadiusYChanged = morphology->setRadiusY(radiusY());
        return isRadiusXChanged || isRadiusYChanged;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEMorphologyElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::operatorAttr || attrName == SVGNames::radiusAttr) {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        return;
    }

    if (attrName == SVGNames::inAttr) {
        InstanceInvalidationGuard guard(*this);
        invalidate();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFEMorphologyElement::build(SVGFilterBuilder* filterBuilder, Filter& filter) const
{
    auto input1 = filterBuilder->getEffectById(in1());
    float xRadius = radiusX();
    float yRadius = radiusY();

    if (!input1)
        return nullptr;

    if (xRadius < 0 || yRadius < 0)
        return nullptr;

    auto effect = FEMorphology::create(filter, svgOperator(), xRadius, yRadius);
    effect->inputEffects().append(input1);
    return effect;
}

} // namespace WebCore
