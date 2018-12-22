/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "FEOffset.h"
#include "FilterEffect.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFEOffsetElement);

inline SVGFEOffsetElement::SVGFEOffsetElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feOffsetTag));
    registerAttributes();
}

Ref<SVGFEOffsetElement> SVGFEOffsetElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFEOffsetElement(tagName, document));
}

void SVGFEOffsetElement::registerAttributes()
{
    auto& registry = attributeRegistry();
    if (!registry.isEmpty())
        return;
    registry.registerAttribute<SVGNames::inAttr, &SVGFEOffsetElement::m_in1>();
    registry.registerAttribute<SVGNames::dxAttr, &SVGFEOffsetElement::m_dx>();
    registry.registerAttribute<SVGNames::dyAttr, &SVGFEOffsetElement::m_dy>();
}

void SVGFEOffsetElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::dxAttr) {
        m_dx.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::dyAttr) {
        m_dy.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::inAttr) {
        m_in1.setValue(value);
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

void SVGFEOffsetElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        invalidate();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFEOffsetElement::build(SVGFilterBuilder* filterBuilder, Filter& filter)
{
    auto input1 = filterBuilder->getEffectById(in1());

    if (!input1)
        return nullptr;

    auto effect = FEOffset::create(filter, dx(), dy());
    effect->inputEffects().append(input1);
    return WTFMove(effect);
}

}
