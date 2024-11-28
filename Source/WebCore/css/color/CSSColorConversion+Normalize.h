/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSColorDescriptors.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSPrimitiveValue.h"
#include "ColorNormalization.h"
#include <limits>
#include <optional>

namespace WebCore {

// MARK: - normalizeAndClampNumericComponents

template<typename Descriptor, unsigned Index>
CSS::Number<> normalizeAndClampNumericComponents(CSS::NumberRaw<> number)
{
    constexpr auto info = std::get<Index>(Descriptor::components);

    if constexpr (info.type == ColorComponentType::Angle)
        return CSS::NumberRaw<> { normalizeHue(number.value) };
    else if constexpr (info.min == -std::numeric_limits<double>::infinity() && info.max == std::numeric_limits<double>::infinity())
        return CSS::NumberRaw<> { number.value };
    else if constexpr (info.min == -std::numeric_limits<double>::infinity())
        return CSS::NumberRaw<> { std::min(number.value, info.max) };
    else if constexpr (info.max == std::numeric_limits<double>::infinity())
        return CSS::NumberRaw<> { std::max(number.value, info.min) };
    else
        return CSS::NumberRaw<> { std::clamp(number.value, info.min, info.max) };
}

template<typename Descriptor, unsigned Index>
CSS::Number<> normalizeAndClampNumericComponents(CSS::PercentageRaw<> percent)
{
    constexpr auto info = std::get<Index>(Descriptor::components);

    if constexpr (info.min == -std::numeric_limits<double>::infinity() && info.max == std::numeric_limits<double>::infinity())
        return CSS::NumberRaw<> { percent.value * info.percentMultiplier };
    else if constexpr (info.min == -std::numeric_limits<double>::infinity())
        return CSS::NumberRaw<> { std::min(percent.value * info.percentMultiplier, info.max) };
    else if constexpr (info.max == std::numeric_limits<double>::infinity())
        return CSS::NumberRaw<> { std::max(percent.value * info.percentMultiplier, info.min) };
    else
        return CSS::NumberRaw<> { std::clamp(percent.value * info.percentMultiplier, info.min, info.max) };
}

template<typename Descriptor, unsigned Index>
CSS::Number<> normalizeAndClampNumericComponents(CSS::AngleRaw<> angle)
{
    constexpr auto info = std::get<Index>(Descriptor::components);
    static_assert(info.type == ColorComponentType::Angle);

    return CSS::NumberRaw<> { normalizeHue(CSSPrimitiveValue::computeDegrees(angle.type, angle.value)) };
}

template<typename Descriptor, unsigned Index>
auto normalizeAndClampNumericComponentsIntoCanonicalRepresentation(const CSS::None& none) -> GetCSSColorParseTypeWithCalcComponentResult<typename Descriptor::Canonical, Index>
{
    return none;
}

template<typename Descriptor, unsigned Index, typename Raw>
auto normalizeAndClampNumericComponentsIntoCanonicalRepresentation(const CSS::PrimitiveNumeric<Raw>& value) -> GetCSSColorParseTypeWithCalcComponentResult<typename Descriptor::Canonical, Index>
{
    return WTF::switchOn(value,
        [](Raw value) -> GetCSSColorParseTypeWithCalcComponentResult<typename Descriptor::Canonical, Index> {
            return normalizeAndClampNumericComponents<Descriptor, Index>(value);
        },
        [](const CSS::UnevaluatedCalc<Raw>& value) -> GetCSSColorParseTypeWithCalcComponentResult<typename Descriptor::Canonical, Index> {
            return CSS::PrimitiveNumeric<Raw> { value };
        }
    );
}

template<typename Descriptor, unsigned Index, typename... Ts>
auto normalizeAndClampNumericComponentsIntoCanonicalRepresentation(const std::variant<Ts...>& variant) -> GetCSSColorParseTypeWithCalcComponentResult<typename Descriptor::Canonical, Index>
{
    return WTF::switchOn(variant, [](auto value) { return normalizeAndClampNumericComponentsIntoCanonicalRepresentation<Descriptor, Index>(value); });
}

template<typename Descriptor, unsigned Index>
auto normalizeAndClampNumericComponentsIntoCanonicalRepresentation(const std::optional<GetCSSColorParseTypeWithCalcComponentResult<Descriptor, Index>>& optional) -> std::optional<GetCSSColorParseTypeWithCalcComponentResult<typename Descriptor::Canonical, Index>>
{
    return optional ? std::make_optional(normalizeAndClampNumericComponentsIntoCanonicalRepresentation<Descriptor, Index>(*optional)) : std::nullopt;
}

// MARK: - normalizeNumericComponents

template<typename Descriptor, unsigned Index>
CSS::Number<> normalizeNumericComponents(CSS::NumberRaw<> number)
{
    constexpr auto info = std::get<Index>(Descriptor::components);

    if constexpr (info.type == ColorComponentType::Angle)
        return CSS::NumberRaw<> { normalizeHue(number.value) };
    else
        return CSS::NumberRaw<> { number.value };
}

template<typename Descriptor, unsigned Index>
CSS::Number<> normalizeNumericComponents(CSS::PercentageRaw<> percent)
{
    constexpr auto info = std::get<Index>(Descriptor::components);

    return CSS::NumberRaw<> { percent.value * info.percentMultiplier };
}

template<typename Descriptor, unsigned Index>
CSS::Number<> normalizeNumericComponents(CSS::AngleRaw<> angle)
{
    constexpr auto info = std::get<Index>(Descriptor::components);
    static_assert(info.type == ColorComponentType::Angle);

    return CSS::NumberRaw<> { normalizeHue(CSSPrimitiveValue::computeDegrees(angle.type, angle.value)) };
}

template<typename Descriptor, unsigned Index>
auto normalizeNumericComponentsIntoCanonicalRepresentation(const CSS::None& none) -> GetCSSColorParseTypeWithCalcComponentResult<typename Descriptor::Canonical, Index>
{
    return none;
}

template<typename Descriptor, unsigned Index, typename Raw>
auto normalizeNumericComponentsIntoCanonicalRepresentation(const CSS::PrimitiveNumeric<Raw>& value) -> GetCSSColorParseTypeWithCalcComponentResult<typename Descriptor::Canonical, Index>
{
    return WTF::switchOn(value,
        [](Raw value) -> GetCSSColorParseTypeWithCalcComponentResult<typename Descriptor::Canonical, Index> {
            return normalizeNumericComponents<Descriptor, Index>(value);
        },
        [](const CSS::UnevaluatedCalc<Raw>& value) -> GetCSSColorParseTypeWithCalcComponentResult<typename Descriptor::Canonical, Index> {
            return CSS::PrimitiveNumeric<Raw> { value };
        }
    );
}

template<typename Descriptor, unsigned Index, typename... Ts>
auto normalizeNumericComponentsIntoCanonicalRepresentation(const std::variant<Ts...>& variant) -> GetCSSColorParseTypeWithCalcComponentResult<typename Descriptor::Canonical, Index>
{
    return WTF::switchOn(variant, [](auto value) { return normalizeNumericComponentsIntoCanonicalRepresentation<Descriptor, Index>(value); });
}

template<typename Descriptor, unsigned Index>
auto normalizeNumericComponentsIntoCanonicalRepresentation(const std::optional<GetCSSColorParseTypeWithCalcComponentResult<Descriptor, Index>>& optional) -> std::optional<GetCSSColorParseTypeWithCalcComponentResult<typename Descriptor::Canonical, Index>>
{
    return optional ? std::make_optional(normalizeNumericComponentsIntoCanonicalRepresentation<Descriptor, Index>(*optional)) : std::nullopt;
}

} // namespace WebCore
