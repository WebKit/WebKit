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

#include "FilterEffect.h"

namespace WebCore {

class FEOffset : public FilterEffect {
public:
    WEBCORE_EXPORT static Ref<FEOffset> create(float dx, float dy);

    float dx() const { return m_dx; }
    bool setDx(float);

    float dy() const { return m_dy; }
    bool setDy(float);

    static IntOutsets calculateOutsets(const FloatSize& offset);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Ref<FEOffset>> decode(Decoder&);

private:
    FEOffset(float dx, float dy);

    FloatRect calculateImageRect(const Filter&, Span<const FloatRect> inputImageRects, const FloatRect& primitiveSubregion) const override;

    bool resultIsAlphaImage(const FilterImageVector& inputs) const override;

    std::unique_ptr<FilterEffectApplier> createSoftwareApplier() const override;

    WTF::TextStream& externalRepresentation(WTF::TextStream&, FilterRepresentation) const override;

    float m_dx;
    float m_dy;
};

template<class Encoder>
void FEOffset::encode(Encoder& encoder) const
{
    encoder << m_dx;
    encoder << m_dy;
}

template<class Decoder>
std::optional<Ref<FEOffset>> FEOffset::decode(Decoder& decoder)
{
    std::optional<float> dx;
    decoder >> dx;
    if (!dx)
        return std::nullopt;

    std::optional<float> dy;
    decoder >> dy;
    if (!dy)
        return std::nullopt;

    return FEOffset::create(*dx, *dy);
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_FILTER_EFFECT(FEOffset)
