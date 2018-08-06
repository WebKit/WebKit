/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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
#include "SVGFETurbulenceElement.h"

#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFETurbulenceElement);

inline SVGFETurbulenceElement::SVGFETurbulenceElement(const QualifiedName& tagName, Document& document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feTurbulenceTag));
    registerAttributes();
}

Ref<SVGFETurbulenceElement> SVGFETurbulenceElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFETurbulenceElement(tagName, document));
}

const AtomicString& SVGFETurbulenceElement::baseFrequencyXIdentifier()
{
    static NeverDestroyed<AtomicString> s_identifier("SVGBaseFrequencyX", AtomicString::ConstructFromLiteral);
    return s_identifier;
}

const AtomicString& SVGFETurbulenceElement::baseFrequencyYIdentifier()
{
    static NeverDestroyed<AtomicString> s_identifier("SVGBaseFrequencyY", AtomicString::ConstructFromLiteral);
    return s_identifier;
}

void SVGFETurbulenceElement::registerAttributes()
{
    auto& registry = attributeRegistry();
    if (!registry.isEmpty())
        return;
    registry.registerAttribute<SVGNames::baseFrequencyAttr,
        &SVGFETurbulenceElement::baseFrequencyXIdentifier, &SVGFETurbulenceElement::m_baseFrequencyX,
        &SVGFETurbulenceElement::baseFrequencyYIdentifier, &SVGFETurbulenceElement::m_baseFrequencyY>();
    registry.registerAttribute<SVGNames::numOctavesAttr, &SVGFETurbulenceElement::m_numOctaves>();
    registry.registerAttribute<SVGNames::seedAttr, &SVGFETurbulenceElement::m_seed>();
    registry.registerAttribute<SVGNames::stitchTilesAttr, SVGStitchOptions, &SVGFETurbulenceElement::m_stitchTiles>();
    registry.registerAttribute<SVGNames::typeAttr, TurbulenceType, &SVGFETurbulenceElement::m_type>();
}

void SVGFETurbulenceElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::typeAttr) {
        TurbulenceType propertyValue = SVGPropertyTraits<TurbulenceType>::fromString(value);
        if (propertyValue != TurbulenceType::Unknown)
            m_type.setValue(propertyValue);
        return;
    }

    if (name == SVGNames::stitchTilesAttr) {
        SVGStitchOptions propertyValue = SVGPropertyTraits<SVGStitchOptions>::fromString(value);
        if (propertyValue > 0)
            m_stitchTiles.setValue(propertyValue);
        return;
    }

    if (name == SVGNames::baseFrequencyAttr) {
        float x, y;
        if (parseNumberOptionalNumber(value, x, y)) {
            m_baseFrequencyX.setValue(x);
            m_baseFrequencyY.setValue(y);
        }
        return;
    }

    if (name == SVGNames::seedAttr) {
        m_seed.setValue(value.toFloat());
        return;
    }

    if (name == SVGNames::numOctavesAttr) {
        m_numOctaves.setValue(value.string().toUIntStrict());
        return;
    }

    SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
}

bool SVGFETurbulenceElement::setFilterEffectAttribute(FilterEffect* effect, const QualifiedName& attrName)
{
    FETurbulence* turbulence = static_cast<FETurbulence*>(effect);
    if (attrName == SVGNames::typeAttr)
        return turbulence->setType(type());
    if (attrName == SVGNames::stitchTilesAttr)
        return turbulence->setStitchTiles(stitchTiles());
    if (attrName == SVGNames::baseFrequencyAttr)
        return (turbulence->setBaseFrequencyX(baseFrequencyX()) || turbulence->setBaseFrequencyY(baseFrequencyY()));
    if (attrName == SVGNames::seedAttr)
        return turbulence->setSeed(seed());
    if (attrName == SVGNames::numOctavesAttr)
        return turbulence->setNumOctaves(numOctaves());

    ASSERT_NOT_REACHED();
    return false;
}

void SVGFETurbulenceElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        primitiveAttributeChanged(attrName);
        return;
    }

    SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

RefPtr<FilterEffect> SVGFETurbulenceElement::build(SVGFilterBuilder*, Filter& filter)
{
    if (baseFrequencyX() < 0 || baseFrequencyY() < 0)
        return nullptr;
    return FETurbulence::create(filter, type(), baseFrequencyX(), baseFrequencyY(), numOctaves(), seed(), stitchTiles() == SVG_STITCHTYPE_STITCH);
}

}
