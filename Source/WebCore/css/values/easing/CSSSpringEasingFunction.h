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

// FIXME: Contexts that allow calc() should not be defined using a closed interval - https://drafts.csswg.org/css-values-4/#calc-range
// If spring() ever goes further with standardization, the allowable ranges for `mass` and `stiffness` should be reconsidered as the
// std::nextafter() clamping is non-obvious.

// <spring()> = spring( <number [>0,∞]> <number [>0,∞]> <number [0,∞]> <number> )
// Non-standard
struct SpringEasingParameters {
    static constexpr auto NextAfterZero = std::numeric_limits<double>::denorm_min();
    static constexpr auto Positive = Range { NextAfterZero, Range::infinity };

    Number<Positive> mass;
    Number<Positive> stiffness;
    Number<Nonnegative> damping;
    Number<> initialVelocity;

    bool operator==(const SpringEasingParameters&) const = default;
};
using SpringEasingFunction = FunctionNotation<CSSValueSpring, SpringEasingParameters>;

template<size_t I> const auto& get(const SpringEasingParameters& value)
{
    if constexpr (!I)
        return value.mass;
    else if constexpr (I == 1)
        return value.stiffness;
    else if constexpr (I == 2)
        return value.damping;
    else if constexpr (I == 3)
        return value.initialVelocity;
}

} // namespace CSS
} // namespace WebCore

CSS_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(SpringEasingParameters, 4)
