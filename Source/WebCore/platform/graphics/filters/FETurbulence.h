/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Renata Hodovan <reni@inf.u-szeged.hu>
 * Copyright (C) 2017-2022 Apple Inc.  All rights reserved.
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

#include "ColorComponents.h"
#include "FilterEffect.h"

namespace WebCore {

enum class TurbulenceType {
    Unknown,
    FractalNoise,
    Turbulence
};

class FETurbulence : public FilterEffect {
public:
    WEBCORE_EXPORT static Ref<FETurbulence> create(TurbulenceType, float baseFrequencyX, float baseFrequencyY, int numOctaves, float seed, bool stitchTiles);

    TurbulenceType type() const { return m_type; }
    bool setType(TurbulenceType);

    float baseFrequencyX() const { return m_baseFrequencyX; }
    bool setBaseFrequencyX(float);

    float baseFrequencyY() const { return m_baseFrequencyY; }
    bool setBaseFrequencyY(float);

    float seed() const { return m_seed; }
    bool setSeed(float);

    int numOctaves() const { return m_numOctaves; }
    bool setNumOctaves(int);

    bool stitchTiles() const { return m_stitchTiles; }
    bool setStitchTiles(bool);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Ref<FETurbulence>> decode(Decoder&);

private:
    FETurbulence(TurbulenceType, float baseFrequencyX, float baseFrequencyY, int numOctaves, float seed, bool stitchTiles);

    unsigned numberOfEffectInputs() const override { return 0; }

    FloatRect calculateImageRect(const Filter&, Span<const FloatRect> inputImageRects, const FloatRect& primitiveSubregion) const override;

    std::unique_ptr<FilterEffectApplier> createSoftwareApplier() const override;

    WTF::TextStream& externalRepresentation(WTF::TextStream&, FilterRepresentation) const override;

    TurbulenceType m_type;
    float m_baseFrequencyX;
    float m_baseFrequencyY;
    int m_numOctaves;
    float m_seed;
    bool m_stitchTiles;
};

template<class Encoder>
void FETurbulence::encode(Encoder& encoder) const
{
    encoder << m_type;
    encoder << m_baseFrequencyX;
    encoder << m_baseFrequencyY;
    encoder << m_numOctaves;
    encoder << m_seed;
    encoder << m_stitchTiles;
}

template<class Decoder>
std::optional<Ref<FETurbulence>> FETurbulence::decode(Decoder& decoder)
{
    std::optional<TurbulenceType> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    std::optional<float> baseFrequencyX;
    decoder >> baseFrequencyX;
    if (!baseFrequencyX)
        return std::nullopt;

    std::optional<float> baseFrequencyY;
    decoder >> baseFrequencyY;
    if (!baseFrequencyY)
        return std::nullopt;

    std::optional<int> numOctaves;
    decoder >> numOctaves;
    if (!numOctaves)
        return std::nullopt;

    std::optional<float> seed;
    decoder >> seed;
    if (!seed)
        return std::nullopt;

    std::optional<bool> stitchTiles;
    decoder >> stitchTiles;
    if (!stitchTiles)
        return std::nullopt;

    return FETurbulence::create(*type, *baseFrequencyX, *baseFrequencyY, *numOctaves, *seed, *stitchTiles);
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::TurbulenceType> {
    using values = EnumValues<
        WebCore::TurbulenceType,

        WebCore::TurbulenceType::Unknown,
        WebCore::TurbulenceType::FractalNoise,
        WebCore::TurbulenceType::Turbulence
    >;
};

} // namespace WTF

SPECIALIZE_TYPE_TRAITS_FILTER_EFFECT(FETurbulence)
