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
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+RawTypes.h"
#include "ColorNormalization.h"
#include <limits>
#include <optional>

namespace WebCore {

// MARK: - normalizeAndClampNumericComponents

template<typename Descriptor, unsigned Index>
NumberRaw normalizeAndClampNumericComponents(NumberRaw number)
{
    constexpr auto info = std::get<Index>(Descriptor::components);

    if constexpr (info.type == ColorComponentType::Angle)
        return { normalizeHue(number.value) };
    else if constexpr (info.min == -std::numeric_limits<double>::infinity() && info.max == std::numeric_limits<double>::infinity())
        return { number.value };
    else if constexpr (info.min == -std::numeric_limits<double>::infinity())
        return { std::min(number.value, info.max) };
    else if constexpr (info.max == std::numeric_limits<double>::infinity())
        return { std::max(number.value, info.min) };
    else
        return { std::clamp(number.value, info.min, info.max) };
}

template<typename Descriptor, unsigned Index>
NumberRaw normalizeAndClampNumericComponents(PercentageRaw percent)
{
    constexpr auto info = std::get<Index>(Descriptor::components);

    if constexpr (info.min == -std::numeric_limits<double>::infinity() && info.max == std::numeric_limits<double>::infinity())
        return { percent.value * info.percentMultiplier };
    else if constexpr (info.min == -std::numeric_limits<double>::infinity())
        return { std::min(percent.value * info.percentMultiplier, info.max) };
    else if constexpr (info.max == std::numeric_limits<double>::infinity())
        return { std::max(percent.value * info.percentMultiplier, info.min) };
    else
        return { std::clamp(percent.value * info.percentMultiplier, info.min, info.max) };
}

template<typename Descriptor, unsigned Index>
NumberRaw normalizeAndClampNumericComponents(AngleRaw angle)
{
    constexpr auto info = std::get<Index>(Descriptor::components);
    static_assert(info.type == ColorComponentType::Angle);

    return { normalizeHue(CSSPrimitiveValue::computeDegrees(angle.type, angle.value)) };
}

template<typename Descriptor, unsigned Index, typename T>
auto normalizeAndClampNumericComponents(const T& value) -> T
{
    return value;
}

template<typename Descriptor, unsigned Index, typename... Ts>
auto normalizeAndClampNumericComponents(const std::variant<Ts...>& variant) -> std::variant<Ts...>
{
    return WTF::switchOn(variant, [](auto value) -> std::variant<Ts...> { return normalizeAndClampNumericComponents<Descriptor, Index>(value); });
}

template<typename Descriptor, unsigned Index, typename T>
auto normalizeAndClampNumericComponents(const std::optional<T>& optional) -> std::optional<T>
{
    if (!optional)
        return std::nullopt;
    return normalizeAndClampNumericComponents<Descriptor, Index>(*optional);
}

template<typename Descriptor, unsigned Index>
auto normalizeAndClampNumericComponentsIntoCanonicalRepresentation(const GetComponentResultWithCalcResult<Descriptor, Index>& variant) -> GetComponentResultWithCalcResult<typename Descriptor::Canonical, Index>
{
    return WTF::switchOn(variant, [](auto value) -> GetComponentResultWithCalcResult<typename Descriptor::Canonical, Index> { return normalizeAndClampNumericComponents<Descriptor, Index>(value); });
}

template<typename Descriptor, unsigned Index>
auto normalizeAndClampNumericComponentsIntoCanonicalRepresentation(const std::optional<GetComponentResultWithCalcResult<Descriptor, Index>>& optional) -> std::optional<GetComponentResultWithCalcResult<typename Descriptor::Canonical, Index>>
{
    if (!optional)
        return std::nullopt;
    return normalizeAndClampNumericComponentsIntoCanonicalRepresentation<Descriptor, Index>(*optional);
}

// MARK: - normalizeNumericComponents

template<typename Descriptor, unsigned Index>
NumberRaw normalizeNumericComponents(NumberRaw number)
{
    constexpr auto info = std::get<Index>(Descriptor::components);

    if constexpr (info.type == ColorComponentType::Angle)
        return { normalizeHue(number.value) };
    else
        return { number.value };
}

template<typename Descriptor, unsigned Index>
NumberRaw normalizeNumericComponents(PercentageRaw percent)
{
    constexpr auto info = std::get<Index>(Descriptor::components);

    return { percent.value * info.percentMultiplier };
}

template<typename Descriptor, unsigned Index>
NumberRaw normalizeNumericComponents(AngleRaw angle)
{
    constexpr auto info = std::get<Index>(Descriptor::components);
    static_assert(info.type == ColorComponentType::Angle);

    return { normalizeHue(CSSPrimitiveValue::computeDegrees(angle.type, angle.value)) };
}

template<typename Descriptor, unsigned Index, typename T>
auto normalizeNumericComponents(const T& value) -> T
{
    // Passthrough any non-numeric types.
    return value;
}

template<typename Descriptor, unsigned Index, typename... Ts>
auto normalizeNumericComponents(const std::variant<Ts...>& variant) -> std::variant<Ts...>
{
    return WTF::switchOn(variant, [](auto value) -> std::variant<Ts...> { return normalizeNumericComponents<Descriptor, Index>(value); });
}

template<typename Descriptor, unsigned Index, typename T>
auto normalizeNumericComponents(const std::optional<T>& optional) -> std::optional<T>
{
    if (!optional)
        return std::nullopt;
    return normalizeNumericComponents<Descriptor, Index>(*optional);
}

template<typename Descriptor, unsigned Index>
auto normalizeNumericComponentsIntoCanonicalRepresentation(const GetComponentResultWithCalcResult<Descriptor, Index>& variant) -> GetComponentResultWithCalcResult<typename Descriptor::Canonical, Index>
{
    return WTF::switchOn(variant, [](auto value) -> GetComponentResultWithCalcResult<typename Descriptor::Canonical, Index> { return normalizeNumericComponents<Descriptor, Index>(value); });
}

template<typename Descriptor, unsigned Index>
auto normalizeNumericComponentsIntoCanonicalRepresentation(const std::optional<GetComponentResultWithCalcResult<Descriptor, Index>>& optional) -> std::optional<GetComponentResultWithCalcResult<typename Descriptor::Canonical, Index>>
{
    if (!optional)
        return std::nullopt;
    return normalizeNumericComponentsIntoCanonicalRepresentation<Descriptor, Index>(*optional);
}

} // namespace WebCore
