/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPrimitiveNumericTypes.h"

namespace WebCore {
namespace CSS {

// <linear()> = linear( [ <number> && <percentage>{0,2} ]# )
// https://drafts.csswg.org/css-easing-2/#funcdef-linear
struct LinearEasingParameters {
    struct Stop {
        struct Length {
            Percentage<> input;
            std::optional<Percentage<>> extra;

            bool operator==(const Length&) const = default;
        };

        Number<> output;
        std::optional<Length> input;

        bool operator==(const Stop&) const = default;
    };

    CommaSeparatedVector<Stop> stops;

    bool operator==(const LinearEasingParameters&) const = default;
};
using LinearEasingFunction = FunctionNotation<CSSValueLinear, LinearEasingParameters>;

DEFINE_TYPE_WRAPPER(LinearEasingParameters, stops);

template<size_t I> const auto& get(const LinearEasingParameters::Stop& value)
{
    if constexpr (!I)
        return value.output;
    else if constexpr (I == 1)
        return value.input;
}

template<size_t I> const auto& get(const LinearEasingParameters::Stop::Length& value)
{
    if constexpr (!I)
        return value.input;
    else if constexpr (I == 1)
        return value.extra;
}

} // namespace CSS
} // namespace WebCore

CSS_TUPLE_LIKE_CONFORMANCE(LinearEasingParameters, 1)
CSS_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(LinearEasingParameters::Stop, 2)
CSS_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(LinearEasingParameters::Stop::Length, 2)
