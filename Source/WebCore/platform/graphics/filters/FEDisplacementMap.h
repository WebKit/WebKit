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
#include <wtf/text/WTFString.h>

namespace WebCore {

enum ChannelSelectorType {
    CHANNEL_UNKNOWN = 0,
    CHANNEL_R = 1,
    CHANNEL_G = 2,
    CHANNEL_B = 3,
    CHANNEL_A = 4
};

class FEDisplacementMap : public FilterEffect {
public:
    WEBCORE_EXPORT static Ref<FEDisplacementMap> create(ChannelSelectorType xChannelSelector, ChannelSelectorType yChannelSelector, float scale);

    ChannelSelectorType xChannelSelector() const { return m_xChannelSelector; }
    bool setXChannelSelector(const ChannelSelectorType);

    ChannelSelectorType yChannelSelector() const { return m_yChannelSelector; }
    bool setYChannelSelector(const ChannelSelectorType);

    float scale() const { return m_scale; }
    bool setScale(float);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Ref<FEDisplacementMap>> decode(Decoder&);

private:
    FEDisplacementMap(ChannelSelectorType xChannelSelector, ChannelSelectorType yChannelSelector, float);

    unsigned numberOfEffectInputs() const override { return 2; }

    FloatRect calculateImageRect(const Filter&, const FilterImageVector& inputs, const FloatRect& primitiveSubregion) const override;

    const DestinationColorSpace& resultColorSpace(const FilterImageVector&) const override;
    void transformInputsColorSpace(const FilterImageVector& inputs) const override;

    std::unique_ptr<FilterEffectApplier> createSoftwareApplier() const override;

    WTF::TextStream& externalRepresentation(WTF::TextStream&, FilterRepresentation) const override;

    ChannelSelectorType m_xChannelSelector;
    ChannelSelectorType m_yChannelSelector;
    float m_scale;
};

template<class Encoder>
void FEDisplacementMap::encode(Encoder& encoder) const
{
    encoder << m_xChannelSelector;
    encoder << m_yChannelSelector;
    encoder << m_scale;
}

template<class Decoder>
std::optional<Ref<FEDisplacementMap>> FEDisplacementMap::decode(Decoder& decoder)
{
    std::optional<ChannelSelectorType> xChannelSelector;
    decoder >> xChannelSelector;
    if (!xChannelSelector)
        return std::nullopt;

    std::optional<ChannelSelectorType> yChannelSelector;
    decoder >> yChannelSelector;
    if (!yChannelSelector)
        return std::nullopt;

    std::optional<float> scale;
    decoder >> scale;
    if (!scale)
        return std::nullopt;

    return FEDisplacementMap::create(*xChannelSelector, *yChannelSelector, *scale);
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::ChannelSelectorType> {
    using values = EnumValues<
        WebCore::ChannelSelectorType,

        WebCore::CHANNEL_UNKNOWN,
        WebCore::CHANNEL_R,
        WebCore::CHANNEL_G,
        WebCore::CHANNEL_B,
        WebCore::CHANNEL_A
    >;
};

} // namespace WTF

SPECIALIZE_TYPE_TRAITS_FILTER_EFFECT(FEDisplacementMap)
