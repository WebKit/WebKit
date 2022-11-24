/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
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

class FEFlood : public FilterEffect {
public:
    WEBCORE_EXPORT static Ref<FEFlood> create(const Color& floodColor, float floodOpacity);

    const Color& floodColor() const { return m_floodColor; }
    bool setFloodColor(const Color&);

    float floodOpacity() const { return m_floodOpacity; }
    bool setFloodOpacity(float);

#if !USE(CG)
    // feFlood does not perform color interpolation of any kind, so the result is always in the current
    // color space regardless of the value of color-interpolation-filters.
    void setOperatingColorSpace(const DestinationColorSpace&) override { }
#endif

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Ref<FEFlood>> decode(Decoder&);

private:
    FEFlood(const Color& floodColor, float floodOpacity);

    unsigned numberOfEffectInputs() const override { return 0; }

    FloatRect calculateImageRect(const Filter&, Span<const FloatRect> inputImageRects, const FloatRect& primitiveSubregion) const override;

    std::unique_ptr<FilterEffectApplier> createSoftwareApplier() const override;

    WTF::TextStream& externalRepresentation(WTF::TextStream&, FilterRepresentation) const override;

    Color m_floodColor;
    float m_floodOpacity;
};

template<class Encoder>
void FEFlood::encode(Encoder& encoder) const
{
    encoder << m_floodColor;
    encoder << m_floodOpacity;
}

template<class Decoder>
std::optional<Ref<FEFlood>> FEFlood::decode(Decoder& decoder)
{
    std::optional<Color> floodColor;
    decoder >> floodColor;
    if (!floodColor)
        return std::nullopt;

    std::optional<float> floodOpacity;
    decoder >> floodOpacity;
    if (!floodOpacity)
        return std::nullopt;

    return FEFlood::create(*floodColor, *floodOpacity);
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_FILTER_EFFECT(FEFlood)
