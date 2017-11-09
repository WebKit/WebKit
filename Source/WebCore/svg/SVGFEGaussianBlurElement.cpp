/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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
#include "SVGFEGaussianBlurElement.h"

#include "FilterEffect.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGFEGaussianBlurElement, SVGNames::inAttr, In1, in1)
DEFINE_ANIMATED_NUMBER_MULTIPLE_WRAPPERS(SVGFEGaussianBlurElement, SVGNames::stdDeviationAttr, stdDeviationXIdentifier(), StdDeviationX, stdDeviationX)
DEFINE_ANIMATED_NUMBER_MULTIPLE_WRAPPERS(SVGFEGaussianBlurElement, SVGNames::stdDeviationAttr, stdDeviationYIdentifier(), StdDeviationY, stdDeviationY)
DEFINE_ANIMATED_ENUMERATION(SVGFEGaussianBlurElement, SVGNames::edgeModeAttr, EdgeMode, edgeMode, EdgeModeType)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFEGaussianBlurElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(in1)
    REGISTER_LOCAL_ANIMATED_PROPERTY(stdDeviationX)
    REGISTER_LOCAL_ANIMATED_PROPERTY(stdDeviationY)
    REGISTER_LOCAL_ANIMATED_PROPERTY(edgeMode)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGFilterPrimitiveStandardAttributes)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGFEGaussianBlurElement::SVGFEGaussianBlurElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
    , m_edgeMode(EDGEMODE_NONE)
{
    ASSERT(hasTagName(SVGNames::feGaussianBlurTag));
    registerAnimatedPropertiesForSVGFEGaussianBlurElement();
}

Ref<SVGFEGaussianBlurElement> SVGFEGaussianBlurElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEGaussianBlurElement(tagName, document));
}

const AtomicString& SVGFEGaussianBlurElement::stdDeviationXIdentifier()
{
    static NeverDestroyed<AtomicString> s_identifier("SVGStdDeviationX", AtomicString::ConstructFromLiteral);
    return s_identifier;
}

const AtomicString& SVGFEGaussianBlurElement::stdDeviationYIdentifier()
{
    static NeverDestroyed<AtomicString> s_identifier("SVGStdDeviationY", AtomicString::ConstructFromLiteral);
    return s_identifier;
}

void SVGFEGaussianBlurElement::setStdDeviation(float x, float y)
{
    setStdDeviationXBaseValue(x);
    setStdDeviationYBaseValue(y);
    invalidate();
}

void SVGFEGaussianBlurElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::stdDeviationAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            setStdDeviationXBaseValue(x);
            setStdDeviationYBaseValue(y);
        }
        return;
    }

    if (name == SVGNames::inAttr) {
        setIn1BaseValue(value);
        return;
    }

    if (name == SVGNames::edgeModeAttr) {
        auto propertyValue = SVGPropertyTraits<EdgeModeType>::fromString(value);
        if (propertyValue > 0)
            setEdgeModeBaseValue(propertyValue);
        else
            document().accessSVGExtensions().reportWarning("feGaussianBlur: problem parsing edgeMode=\"" + value + "\". Filtered element will not be displayed.");
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

void SVGFEGaussianBlurElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::inAttr || attrName == SVGNames::stdDeviationAttr || attrName == SVGNames::edgeModeAttr) {
        InstanceInvalidationGuard guard(*this);
        invalidate();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFEGaussianBlurElement::build(SVGFilterBuilder* filterBuilder, Filter& filter)
{
    auto input1 = filterBuilder->getEffectById(in1());

    if (!input1)
        return nullptr;

    if (stdDeviationX() < 0 || stdDeviationY() < 0)
        return nullptr;

    RefPtr<FilterEffect> effect = FEGaussianBlur::create(filter, stdDeviationX(), stdDeviationY(), edgeMode());
    effect->inputEffects().append(input1);
    return effect;
}

}
