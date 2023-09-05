/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Color.h"
#include "ColorConversion.h"
#include "ColorTypes.h"
#include <wtf/MathExtras.h>

namespace WebCore {

template<typename ColorType> ColorType makeColorTypeByNormalizingComponents(const ColorComponents<float, 4>&);
template<typename ColorType> Color makeCanonicalColor(ColorType, OptionSet<Color::Flags> flags = { });


// MARK: - Normalization

template<typename ComponentType> struct WhitenessBlackness {
    ComponentType whiteness;
    ComponentType blackness;
};

template<typename ComponentType> static auto normalizeClampedWhitenessBlacknessDisallowingNone(ComponentType whiteness, ComponentType blackness) -> WhitenessBlackness<ComponentType>
{
    //   If the sum of these two arguments is greater than 100%, then at
    //   computed-value time they are further normalized to add up to 100%, with
    //   the same relative ratio.
    if (auto sum = whiteness + blackness; sum >= 100)
        return { whiteness * 100.0 / sum, blackness * 100.0 / sum };

    return { whiteness, blackness };
}

template<typename ComponentType> static auto normalizeClampedWhitenessBlacknessAllowingNone(ComponentType whiteness, ComponentType blackness) -> WhitenessBlackness<ComponentType>
{
    if (std::isnan(whiteness) || std::isnan(blackness))
        return { whiteness, blackness };

    return normalizeClampedWhitenessBlacknessDisallowingNone(whiteness, blackness);
}

template<typename ComponentType> inline auto normalizeWhitenessBlackness(ComponentType whiteness, ComponentType blackness) -> WhitenessBlackness<ComponentType>
{
    //   Values outside of these ranges are not invalid, but are clamped to the
    //   ranges defined here at computed-value time.
    WhitenessBlackness<ComponentType> result {
        clampTo<ComponentType>(whiteness, 0.0, 100.0),
        clampTo<ComponentType>(blackness, 0.0, 100.0)
    };

    //   If the sum of these two arguments is greater than 100%, then at
    //   computed-value time they are further normalized to add up to 100%, with
    //   the same relative ratio.
    if (auto sum = result.whiteness + result.blackness; sum >= 100) {
        result.whiteness *= 100.0 / sum;
        result.blackness *= 100.0 / sum;
    }

    return result;
}

template<typename ComponentType> inline ComponentType normalizeHue(ComponentType hue)
{
    return std::fmod(std::fmod(hue, 360.0) + 360.0, 360.0);
}

template<typename ColorType> inline ColorType makeColorTypeByNormalizingComponents(const ColorComponents<float, 4>& colorComponents)
{
    return makeFromComponentsClamping<ColorType>(colorComponents);
}

template<> inline HWBA<float> makeColorTypeByNormalizingComponents<HWBA<float>>(const ColorComponents<float, 4>& colorComponents)
{
    auto [hue, whiteness, blackness, alpha] = colorComponents;
    auto [normalizedWhitness, normalizedBlackness] = normalizeWhitenessBlackness(whiteness, blackness);

    return makeFromComponentsClamping<HWBA<float>>(hue, normalizedWhitness, normalizedBlackness, alpha);
}

// MARK: - Canonicalization

template<typename ColorType> inline Color makeCanonicalColor(ColorType color, OptionSet<Color::Flags> flags)
{
    return { color, flags };
}

template<> inline Color makeCanonicalColor<HWBA<float>>(HWBA<float> color, OptionSet<Color::Flags> flags)
{
    return { convertColor<SRGBA<uint8_t>>(color), flags };
}

template<> inline Color makeCanonicalColor<HSLA<float>>(HSLA<float> color, OptionSet<Color::Flags> flags)
{
    return { convertColor<SRGBA<uint8_t>>(color), flags };
}

}
