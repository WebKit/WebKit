/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

    float baseFrequencyX() const { return m_baseFrequencyX->currentValue(); }
    float baseFrequencyY() const { return m_baseFrequencyY->currentValue(); }
    int numOctaves() const { return m_numOctaves->currentValue(); }
    float seed() const { return m_seed->currentValue(); }
    SVGStitchOptions stitchTiles() const { return m_stitchTiles->currentValue<SVGStitchOptions>(); }
    TurbulenceType type() const { return m_type->currentValue<TurbulenceType>(); }

    SVGAnimatedNumber& baseFrequencyXAnimated() { return m_baseFrequencyX; }
    SVGAnimatedNumber& baseFrequencyYAnimated() { return m_baseFrequencyY; }
    SVGAnimatedInteger& numOctavesAnimated() { return m_numOctaves; }
    SVGAnimatedNumber& seedAnimated() { return m_seed; }
    SVGAnimatedEnumeration& stitchTilesAnimated() { return m_stitchTiles; }
    SVGAnimatedEnumeration& typeAnimated() { return m_type; }

private:
    SVGFETurbulenceElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFETurbulenceElement, SVGFilterPrimitiveStandardAttributes>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool setFilterEffectAttribute(FilterEffect*, const QualifiedName& attrName) override;
    RefPtr<FilterEffect> build(SVGFilterBuilder*, Filter&) const override;

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedNumber> m_baseFrequencyX { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedNumber> m_baseFrequencyY { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedInteger> m_numOctaves { SVGAnimatedInteger::create(this, 1) };
    Ref<SVGAnimatedNumber> m_seed { SVGAnimatedNumber::create(this) };
    Ref<SVGAnimatedEnumeration> m_stitchTiles { SVGAnimatedEnumeration::create(this, SVG_STITCHTYPE_NOSTITCH) };
    Ref<SVGAnimatedEnumeration> m_type { SVGAnimatedEnumeration::create(this, TurbulenceType::Turbulence) };
};

} // namespace WebCore
