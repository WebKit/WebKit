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

#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFETurbulenceElement);

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

void SVGFETurbulenceElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::typeAttr) {
        TurbulenceType propertyValue = SVGPropertyTraits<TurbulenceType>::fromString(value);
        if (propertyValue != TurbulenceType::Unknown)
            m_type->setBaseValInternal<TurbulenceType>(propertyValue);
        return;
    }

    if (name == SVGNames::stitchTilesAttr) {
        SVGStitchOptions propertyValue = SVGPropertyTraits<SVGStitchOptions>::fromString(value);
        if (propertyValue > 0)
            m_stitchTiles->setBaseValInternal<SVGStitchOptions>(propertyValue);
        return;
    }

    if (name == SVGNames::baseFrequencyAttr) {
        if (auto result = parseNumberOptionalNumber(value)) {
            m_baseFrequencyX->setBaseValInternal(result->first);
            m_baseFrequencyY->setBaseValInternal(result->second);
        }
        return;
    }

    if (name == SVGNames::seedAttr) {
        m_seed->setBaseValInternal(value.toFloat());
        return;
    }

    if (name == SVGNames::numOctavesAttr) {
        m_numOctaves->setBaseValInternal(parseInteger<unsigned>(value).value_or(0));
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

bool SVGFETurbulenceElement::setFilterEffectAttribute(FilterEffect& effect, const QualifiedName& attrName)
{
    auto& feTurbulence = downcast<FETurbulence>(effect);
    if (attrName == SVGNames::typeAttr)
        return feTurbulence.setType(type());
    if (attrName == SVGNames::stitchTilesAttr)
        return feTurbulence.setStitchTiles(stitchTiles());
    if (attrName == SVGNames::baseFrequencyAttr) {
        bool baseFrequencyXChanged = feTurbulence.setBaseFrequencyX(baseFrequencyX());
        bool baseFrequencyYChanged = feTurbulence.setBaseFrequencyY(baseFrequencyY());
        return baseFrequencyXChanged || baseFrequencyYChanged;
    }
    if (attrName == SVGNames::seedAttr)
        return feTurbulence.setSeed(seed());
    if (attrName == SVGNames::numOctavesAttr)
        return feTurbulence.setNumOctaves(numOctaves());

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
