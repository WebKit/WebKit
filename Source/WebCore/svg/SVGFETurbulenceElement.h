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

#pragma once

#include "FETurbulence.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedInteger.h"
#include "SVGAnimatedNumber.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

enum SVGStitchOptions {
    SVG_STITCHTYPE_UNKNOWN  = 0,
    SVG_STITCHTYPE_STITCH   = 1,
    SVG_STITCHTYPE_NOSTITCH = 2
};

template<>
struct SVGPropertyTraits<SVGStitchOptions> {
    static unsigned highestEnumValue() { return SVG_STITCHTYPE_NOSTITCH; }

    static String toString(SVGStitchOptions type)
    {
        switch (type) {
        case SVG_STITCHTYPE_UNKNOWN:
            return emptyString();
        case SVG_STITCHTYPE_STITCH:
            return "stitch"_s;
        case SVG_STITCHTYPE_NOSTITCH:
            return "noStitch"_s;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static SVGStitchOptions fromString(const String& value)
    {
        if (value == "stitch")
            return SVG_STITCHTYPE_STITCH;
        if (value == "noStitch")
            return SVG_STITCHTYPE_NOSTITCH;
        return SVG_STITCHTYPE_UNKNOWN;
    }
};

template<>
struct SVGPropertyTraits<TurbulenceType> {
    static unsigned highestEnumValue() { return static_cast<unsigned>(TurbulenceType::Turbulence); }

    static String toString(TurbulenceType type)
    {
        switch (type) {
        case TurbulenceType::Unknown:
            return emptyString();
        case TurbulenceType::FractalNoise:
            return "fractalNoise"_s;
        case TurbulenceType::Turbulence:
            return "turbulence"_s;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static TurbulenceType fromString(const String& value)
    {
        if (value == "fractalNoise")
            return TurbulenceType::FractalNoise;
        if (value == "turbulence")
            return TurbulenceType::Turbulence;
        return TurbulenceType::Unknown;
    }
};

class SVGFETurbulenceElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_ISO_ALLOCATED(SVGFETurbulenceElement);
public:
    static Ref<SVGFETurbulenceElement> create(const QualifiedName&, Document&);

    float baseFrequencyX() const { return m_baseFrequencyX.currentValue(attributeOwnerProxy()); }
    float baseFrequencyY() const { return m_baseFrequencyY.currentValue(attributeOwnerProxy()); }
    int numOctaves() const { return m_numOctaves.currentValue(attributeOwnerProxy()); }
    float seed() const { return m_seed.currentValue(attributeOwnerProxy()); }
    SVGStitchOptions stitchTiles() const { return m_stitchTiles.currentValue(attributeOwnerProxy()); }
    TurbulenceType type() const { return m_type.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedNumber> baseFrequencyXAnimated() { return m_baseFrequencyX.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> baseFrequencyYAnimated() { return m_baseFrequencyY.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedInteger> numOctavesAnimated() { return m_numOctaves.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedNumber> seedAnimated() { return m_seed.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedEnumeration> stitchTilesAnimated() { return m_stitchTiles.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedEnumeration> typeAnimated() { return m_type.animatedProperty(attributeOwnerProxy()); }

private:
    SVGFETurbulenceElement(const QualifiedName&, Document&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGFETurbulenceElement, SVGFilterPrimitiveStandardAttributes>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }
    static void registerAttributes();

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const final { return m_attributeOwnerProxy; }
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool setFilterEffectAttribute(FilterEffect*, const QualifiedName& attrName) override;
    RefPtr<FilterEffect> build(SVGFilterBuilder*, Filter&) override;

    static const AtomicString& baseFrequencyXIdentifier();
    static const AtomicString& baseFrequencyYIdentifier();

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedNumberAttribute m_baseFrequencyX;
    SVGAnimatedNumberAttribute m_baseFrequencyY;
    SVGAnimatedIntegerAttribute m_numOctaves { 1 };
    SVGAnimatedNumberAttribute m_seed;
    SVGAnimatedEnumerationAttribute<SVGStitchOptions> m_stitchTiles { SVG_STITCHTYPE_NOSTITCH };
    SVGAnimatedEnumerationAttribute<TurbulenceType> m_type { TurbulenceType::Turbulence };
};

} // namespace WebCore
