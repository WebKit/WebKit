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
#include "SVGFETileElement.h"

#include "FETile.h"
#include "FilterEffect.h"
#include "SVGFilterBuilder.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFETileElement);

inline SVGFETileElement::SVGFETileElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feTileTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::inAttr, &SVGFETileElement::m_in1>();
    });
}

Ref<SVGFETileElement> SVGFETileElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFETileElement(tagName, document));
}

void SVGFETileElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::inAttr) {
        m_in1->setBaseValInternal(value);
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

void SVGFETileElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::inAttr) {
        InstanceInvalidationGuard guard(*this);
        invalidate();
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFETileElement::build(SVGFilterBuilder* filterBuilder, Filter& filter) const
{
    auto input1 = filterBuilder->getEffectById(in1());

    if (!input1)
        return nullptr;

    auto effect = FETile::create(filter);
    effect->inputEffects().append(input1);
    return effect;
}

}
