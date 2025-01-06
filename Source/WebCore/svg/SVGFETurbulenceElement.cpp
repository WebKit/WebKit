/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
#include "SVGFETurbulenceElement.h"

#include "NodeName.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGFETurbulenceElement);

inline SVGFETurbulenceElement::SVGFETurbulenceElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::feTurbulenceTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::baseFrequencyAttr, &SVGFETurbulenceElement::m_baseFrequencyX, &SVGFETurbulenceElement::m_baseFrequencyY>();
        PropertyRegistry::registerProperty<SVGNames::numOctavesAttr, &SVGFETurbulenceElement::m_numOctaves>();
        PropertyRegistry::registerProperty<SVGNames::seedAttr, &SVGFETurbulenceElement::m_seed>();
        PropertyRegistry::registerProperty<SVGNames::stitchTilesAttr, SVGStitchOptions, &SVGFETurbulenceElement::m_stitchTiles>();
        PropertyRegistry::registerProperty<SVGNames::typeAttr, TurbulenceType, &SVGFETurbulenceElement::m_type>();
    });
}

Ref<SVGFETurbulenceElement> SVGFETurbulenceElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFETurbulenceElement(tagName, document));
}

void SVGFETurbulenceElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::typeAttr: {
        TurbulenceType propertyValue = SVGPropertyTraits<TurbulenceType>::fromString(newValue);
        if (propertyValue != TurbulenceType::Unknown)
            Ref { m_type }->setBaseValInternal<TurbulenceType>(propertyValue);
        break;
    }
    case AttributeNames::stitchTilesAttr: {
        SVGStitchOptions propertyValue = SVGPropertyTraits<SVGStitchOptions>::fromString(newValue);
        if (propertyValue > 0)
            Ref { m_stitchTiles }->setBaseValInternal<SVGStitchOptions>(propertyValue);
        break;
    }
    case AttributeNames::baseFrequencyAttr:
        if (auto result = parseNumberOptionalNumber(newValue)) {
            Ref { m_baseFrequencyX }->setBaseValInternal(result->first);
            Ref { m_baseFrequencyY }->setBaseValInternal(result->second);
        }
        break;
    case AttributeNames::seedAttr:
        Ref { m_seed }->setBaseValInternal(newValue.toFloat());
        break;
    case AttributeNames::numOctavesAttr:
        Ref { m_numOctaves }->setBaseValInternal(parseInteger<unsigned>(newValue).value_or(0));
        break;
    default:
        break;
    }
    SVGFilterPrimitiveStandardAttributes::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

bool SVGFETurbulenceElement::setFilterEffectAttribute(FilterEffect& filterEffect, const QualifiedName& attrName)
{
    auto& effect = downcast<FETurbulence>(filterEffect);
    switch (attrName.nodeName()) {
    case AttributeNames::typeAttr:
        return effect.setType(type());
    case AttributeNames::stitchTilesAttr:
        return effect.setStitchTiles(stitchTiles());
    case AttributeNames::baseFrequencyAttr: {
        bool baseFrequencyXChanged = effect.setBaseFrequencyX(baseFrequencyX());
        bool baseFrequencyYChanged = effect.setBaseFrequencyY(baseFrequencyY());
        return baseFrequencyXChanged || baseFrequencyYChanged;
    }
    case AttributeNames::seedAttr:
        return effect.setSeed(seed());
    case AttributeNames::numOctavesAttr:
        return effect.setNumOctaves(numOctaves());
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void SVGFETurbulenceElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFETurbulenceElement::createFilterEffect(const FilterEffectVector&, const GraphicsContext&) const
{
    if (baseFrequencyX() < 0 || baseFrequencyY() < 0)
        return nullptr;

    return FETurbulence::create(type(), baseFrequencyX(), baseFrequencyY(), numOctaves(), seed(), stitchTiles() == SVG_STITCHTYPE_STITCH);
}

} // namespace WebCore
