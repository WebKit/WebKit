/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
#include "SVGFEBlendElement.h"

#include "FEBlend.h"
#include "FilterEffect.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEBlendElement);

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGFEBlendElement, SVGNames::inAttr, In1, in1)
DEFINE_ANIMATED_STRING(SVGFEBlendElement, SVGNames::in2Attr, In2, in2)
DEFINE_ANIMATED_ENUMERATION(SVGFEBlendElement, SVGNames::modeAttr, Mode, mode, BlendMode)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFEBlendElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(in1)
    REGISTER_LOCAL_ANIMATED_PROPERTY(in2)
    REGISTER_LOCAL_ANIMATED_PROPERTY(mode)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGFilterPrimitiveStandardAttributes)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGFEBlendElement::SVGFEBlendElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
    , m_mode(BlendModeNormal)
{
    ASSERT(hasTagName(SVGNames::feBlendTag));
    registerAnimatedPropertiesForSVGFEBlendElement();
}

Ref<SVGFEBlendElement> SVGFEBlendElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEBlendElement(tagName, document));
}

void SVGFEBlendElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::modeAttr) {
        BlendMode mode = BlendModeNormal;
        if (parseBlendMode(value, mode))
            setModeBaseValue(mode);
        return;
    }

    if (name == SVGNames::inAttr) {
        setIn1BaseValue(value);
        return;
    }

    if (name == SVGNames::in2Attr) {
        setIn2BaseValue(value);
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

bool SVGFEBlendElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    FEBlend* blend = static_cast<FEBlend*>(effect);
    if (attrName == SVGNames::modeAttr)
        return blend->setBlendMode(mode());

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFEBlendElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::modeAttr) {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        return;
    }

    if (attrName == SVGNames::inAttr || attrName == SVGNames::in2Attr) {
        InstanceInvalidationGuard guard(*this);
        invalidate();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFEBlendElement::build(SVGFilterBuilder* filterBuilder, Filter& filter)
{
    auto input1 = filterBuilder->getEffectById(in1());
    auto input2 = filterBuilder->getEffectById(in2());

    if (!input1 || !input2)
        return nullptr;

    RefPtr<FilterEffect> effect = FEBlend::create(filter, mode());
    FilterEffectVector& inputEffects = effect->inputEffects();
    inputEffects.reserveCapacity(2);
    inputEffects.append(input1);
    inputEffects.append(input2);    
    return effect;
}

}
