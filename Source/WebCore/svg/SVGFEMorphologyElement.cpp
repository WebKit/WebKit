/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGFEMorphologyElement, SVGNames::inAttr, In1, in1)
DEFINE_ANIMATED_ENUMERATION(SVGFEMorphologyElement, SVGNames::operatorAttr, SVGOperator, svgOperator, MorphologyOperatorType)
DEFINE_ANIMATED_NUMBER_MULTIPLE_WRAPPERS(SVGFEMorphologyElement, SVGNames::radiusAttr, radiusXIdentifier(), RadiusX, radiusX)
DEFINE_ANIMATED_NUMBER_MULTIPLE_WRAPPERS(SVGFEMorphologyElement, SVGNames::radiusAttr, radiusYIdentifier(), RadiusY, radiusY)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFEMorphologyElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(in1)
    REGISTER_LOCAL_ANIMATED_PROPERTY(svgOperator)
    REGISTER_LOCAL_ANIMATED_PROPERTY(radiusX)
    REGISTER_LOCAL_ANIMATED_PROPERTY(radiusY)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGFilterPrimitiveStandardAttributes)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGFEMorphologyElement::SVGFEMorphologyElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
    , m_svgOperator(FEMORPHOLOGY_OPERATOR_ERODE)
{
    ASSERT(hasTagName(SVGNames::feMorphologyTag));
    registerAnimatedPropertiesForSVGFEMorphologyElement();
}

Ref<SVGFEMorphologyElement> SVGFEMorphologyElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEMorphologyElement(tagName, document));
}

const AtomicString& SVGFEMorphologyElement::radiusXIdentifier()
{
    static NeverDestroyed<AtomicString> s_identifier("SVGRadiusX", AtomicString::ConstructFromLiteral);
    return s_identifier;
}

const AtomicString& SVGFEMorphologyElement::radiusYIdentifier()
{
    static NeverDestroyed<AtomicString> s_identifier("SVGRadiusY", AtomicString::ConstructFromLiteral);
    return s_identifier;
}

void SVGFEMorphologyElement::setRadius(float x, float y)
{
    setRadiusXBaseValue(x);
    setRadiusYBaseValue(y);
    invalidate();
}

void SVGFEMorphologyElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::operatorAttr) {
        MorphologyOperatorType propertyValue = SVGPropertyTraits<MorphologyOperatorType>::fromString(value);
        if (propertyValue > 0)
            setSVGOperatorBaseValue(propertyValue);
        return;
    }

    if (name == SVGNames::inAttr) {
        setIn1BaseValue(value);
        return;
    }

    if (name == SVGNames::radiusAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            setRadiusXBaseValue(x);
            setRadiusYBaseValue(y);
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

RefPtr<FilterEffect> SVGFEMorphologyElement::build(SVGFilterBuilder* filterBuilder, Filter& filter)
{
    FilterEffect* input1 = filterBuilder->getEffectById(in1());
    float xRadius = radiusX();
    float yRadius = radiusY();

    if (!input1)
        return nullptr;

    if (xRadius < 0 || yRadius < 0)
        return nullptr;

    RefPtr<FilterEffect> effect = FEMorphology::create(filter, svgOperator(), xRadius, yRadius);
    effect->inputEffects().append(input1);
    return effect;
}

} // namespace WebCore
