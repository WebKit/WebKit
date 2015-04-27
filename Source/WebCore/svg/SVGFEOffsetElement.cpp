/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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
#include "SVGFEOffsetElement.h"

#include "FilterEffect.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGFEOffsetElement, SVGNames::inAttr, In1, in1)
DEFINE_ANIMATED_NUMBER(SVGFEOffsetElement, SVGNames::dxAttr, Dx, dx)
DEFINE_ANIMATED_NUMBER(SVGFEOffsetElement, SVGNames::dyAttr, Dy, dy)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFEOffsetElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(in1)
    REGISTER_LOCAL_ANIMATED_PROPERTY(dx)
    REGISTER_LOCAL_ANIMATED_PROPERTY(dy)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGFilterPrimitiveStandardAttributes)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGFEOffsetElement::SVGFEOffsetElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feOffsetTag));
    registerAnimatedPropertiesForSVGFEOffsetElement();
}

Ref<SVGFEOffsetElement> SVGFEOffsetElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEOffsetElement(tagName, document));
}

void SVGFEOffsetElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::dxAttr) {
        setDxBaseValue(value.toFloat());
        return;
    }

    if (name == SVGNames::dyAttr) {
        setDyBaseValue(value.toFloat());
        return;
    }

    if (name == SVGNames::inAttr) {
        setIn1BaseValue(value);
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

void SVGFEOffsetElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::inAttr || attrName == SVGNames::dxAttr || attrName == SVGNames::dyAttr) {
        InstanceInvalidationGuard guard(*this);
        invalidate();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFEOffsetElement::build(SVGFilterBuilder* filterBuilder, Filter& filter)
{
    FilterEffect* input1 = filterBuilder->getEffectById(in1());

    if (!input1)
        return nullptr;

    RefPtr<FilterEffect> effect = FEOffset::create(filter, dx(), dy());
    effect->inputEffects().append(input1);
    return effect;
}

}
