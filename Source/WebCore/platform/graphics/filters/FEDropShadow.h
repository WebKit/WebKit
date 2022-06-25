/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2021-2022 Apple Inc.  All rights reserved.
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

#include "Color.h"
#include "FilterEffect.h"

namespace WebCore {
    
class FEDropShadow : public FilterEffect {
public:
    WEBCORE_EXPORT static Ref<FEDropShadow> create(float stdX, float stdY, float dx, float dy, const Color& shadowColor, float shadowOpacity);

    float stdDeviationX() const { return m_stdX; }
    bool setStdDeviationX(float);

    float stdDeviationY() const { return m_stdY; }
    bool setStdDeviationY(float);

    float dx() const { return m_dx; }
    bool setDx(float);

    float dy() const { return m_dy; }
    bool setDy(float);

    const Color& shadowColor() const { return m_shadowColor; } 
    bool setShadowColor(const Color&);

    float shadowOpacity() const { return m_shadowOpacity; }
    bool setShadowOpacity(float);

    static IntOutsets calculateOutsets(const FloatSize& offset, const FloatSize& stdDeviation);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Ref<FEDropShadow>> decode(Decoder&);

private:
    FEDropShadow(float stdX, float stdY, float dx, float dy, const Color& shadowColor, float shadowOpacity);

    FloatRect calculateImageRect(const Filter&, const FilterImageVector& inputs, const FloatRect& primitiveSubregion) const override;

    std::unique_ptr<FilterEffectApplier> createSoftwareApplier() const override;

    WTF::TextStream& externalRepresentation(WTF::TextStream&, FilterRepresentation) const override;

    float m_stdX;
    float m_stdY;
    float m_dx;
    float m_dy;
    Color m_shadowColor;
    float m_shadowOpacity;
};

template<class Encoder>
void FEDropShadow::encode(Encoder& encoder) const
{
    encoder << m_stdX;
    encoder << m_stdY;
    encoder << m_dx;
    encoder << m_dy;
    encoder << m_shadowColor;
    encoder << m_shadowOpacity;
}

template<class Decoder>
std::optional<Ref<FEDropShadow>> FEDropShadow::decode(Decoder& decoder)
{
    std::optional<float> stdX;
    decoder >> stdX;
    if (!stdX)
        return std::nullopt;

    std::optional<float> stdY;
    decoder >> stdY;
    if (!stdY)
        return std::nullopt;

    std::optional<float> dx;
    decoder >> dx;
    if (!dx)
        return std::nullopt;

    std::optional<float> dy;
    decoder >> dy;
    if (!dy)
        return std::nullopt;

    std::optional<Color> shadowColor;
    decoder >> shadowColor;
    if (!shadowColor)
        return std::nullopt;

    std::optional<float> shadowOpacity;
    decoder >> shadowOpacity;
    if (!shadowOpacity)
        return std::nullopt;

    return FEDropShadow::create(*stdX, *stdY, *dx, *dy, *shadowColor, *shadowOpacity);
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_FILTER_EFFECT(FEDropShadow)
