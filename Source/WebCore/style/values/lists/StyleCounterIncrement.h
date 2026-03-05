/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/StylePrimitiveNumericTypes.h>

namespace WebCore {
namespace Style {

// <'counter-increment'> = [ <counter-name> <integer>?@(default=1) ]+ | none
// https://drafts.csswg.org/css-lists/#propdef-counter-increment

// <counter-increment-value> = [ <counter-name> <integer>?@(default=1) ]
struct CounterIncrementValue {
    CustomIdentifier name;
    Integer<> value;

    bool operator==(const CounterIncrementValue&) const = default;
};
template<size_t I> const auto& get(const CounterIncrementValue& value)
{
    if constexpr (!I)
        return value.name;
    else if constexpr (I == 1)
        return value.value;
}

// <counter-increment-list> = <counter-increment-value>+
using CounterIncrementList = SpaceSeparatedFixedVector<CounterIncrementValue>;

// <'counter-increment'> = <counter-increment-list> | none
struct CounterIncrement : ListOrNone<CounterIncrementList> {
    using ListOrNone<CounterIncrementList>::ListOrNone;
};

// MARK: - Conversion

template<> struct CSSValueConversion<CounterIncrementValue> { auto operator()(BuilderState&, const CSSValue&) -> CounterIncrementValue; };

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::CounterIncrementValue, 2)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::CounterIncrement)
