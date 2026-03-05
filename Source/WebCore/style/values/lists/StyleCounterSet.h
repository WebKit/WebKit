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

// <'counter-set'> = [ <counter-name> <integer>?@(default=0) ]+ | none
// https://drafts.csswg.org/css-lists/#propdef-counter-set

// <counter-set-value> = [ <counter-name> <integer>?@(default=0) ]
struct CounterSetValue {
    CustomIdentifier name;
    Integer<> value;

    bool operator==(const CounterSetValue&) const = default;
};
template<size_t I> const auto& get(const CounterSetValue& value)
{
    if constexpr (!I)
        return value.name;
    else if constexpr (I == 1)
        return value.value;
}

// <counter-set-list> = <counter-set-value>+
using CounterSetList = SpaceSeparatedFixedVector<CounterSetValue>;

// <'counter-set'> = <counter-set-list> | none
struct CounterSet : ListOrNone<CounterSetList> {
    using ListOrNone<CounterSetList>::ListOrNone;
};

// MARK: - Conversion

template<> struct CSSValueConversion<CounterSetValue> { auto operator()(BuilderState&, const CSSValue&) -> CounterSetValue; };

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::CounterSetValue, 2)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::CounterSet)
