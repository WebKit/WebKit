/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
#include "SVGFEDropShadowElement.h"

#include "RenderStyle.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGRenderStyle.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGFEDropShadowElement, SVGNames::inAttr, In1, in1)
DEFINE_ANIMATED_NUMBER(SVGFEDropShadowElement, SVGNames::dxAttr, Dx, dx)
DEFINE_ANIMATED_NUMBER(SVGFEDropShadowElement, SVGNames::dyAttr, Dy, dy)
DEFINE_ANIMATED_NUMBER_MULTIPLE_WRAPPERS(SVGFEDropShadowElement, SVGNames::stdDeviationAttr, stdDeviationXIdentifier(), StdDeviationX, stdDeviationX)
DEFINE_ANIMATED_NUMBER_MULTIPLE_WRAPPERS(SVGFEDropShadowElement, SVGNames::stdDeviationAttr, stdDeviationYIdentifier(), StdDeviationY, stdDeviationY)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFEDropShadowElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(in1)
    REGISTER_LOCAL_ANIMATED_PROPERTY(dx)
    REGISTER_LOCAL_ANIMATED_PROPERTY(dy)
    REGISTER_LOCAL_ANIMATED_PROPERTY(stdDeviationX)
    REGISTER_LOCAL_ANIMATED_PROPERTY(stdDeviationY)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGFilterPrimitiveStandardAttributes)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGFEDropShadowElement::SVGFEDropShadowElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
    , m_dx(2)
    , m_dy(2)
    , m_stdDeviationX(2)
    , m_stdDeviationY(2)
{
    ASSERT(hasTagName(SVGNames::feDropShadowTag));
    registerAnimatedPropertiesForSVGFEDropShadowElement();
}

Ref<SVGFEDropShadowElement> SVGFEDropShadowElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEDropShadowElement(tagName, document));
}

const AtomicString& SVGFEDropShadowElement::stdDeviationXIdentifier()
{
    static NeverDestroyed<AtomicString> s_identifier("SVGStdDeviationX", AtomicString::ConstructFromLiteral);
    return s_identifier;
}

const AtomicString& SVGFEDropShadowElement::stdDeviationYIdentifier()
{
    static NeverDestroyed<AtomicString> s_identifier("SVGStdDeviationY", AtomicString::ConstructFromLiteral);
    return s_identifier;
}

void SVGFEDropShadowElement::setStdDeviation(float x, float y)
{
    setStdDeviationXBaseValue(x);
    setStdDeviationYBaseValue(y);
    invalidate();
}

void SVGFEDropShadowElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
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

    if (name == SVGNames::dxAttr) {
        setDxBaseValue(value.toFloat());
        return;
    }

    if (name == SVGNames::dyAttr) {
        setDyBaseValue(value.toFloat());
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

void SVGFEDropShadowElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::inAttr || attrName == SVGNames::stdDeviationAttr || attrName == SVGNames::dxAttr || attrName == SVGNames::dyAttr) {
        InstanceInvalidationGuard guard(*this);
        invalidate();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFEDropShadowElement::build(SVGFilterBuilder* filterBuilder, Filter& filter)
{
    RenderObject* renderer = this->renderer();
    if (!renderer)
        return nullptr;

    if (stdDeviationX() < 0 || stdDeviationY() < 0)
        return nullptr;

    const SVGRenderStyle& svgStyle = renderer->style().svgStyle();
    
    const Color& color = svgStyle.floodColor();
    float opacity = svgStyle.floodOpacity();

    FilterEffect* input1 = filterBuilder->getEffectById(in1());
    if (!input1)
        return nullptr;

    RefPtr<FilterEffect> effect = FEDropShadow::create(filter, stdDeviationX(), stdDeviationY(), dx(), dy(), color, opacity);
    effect->inputEffects().append(input1);
    return effect;
}

}
