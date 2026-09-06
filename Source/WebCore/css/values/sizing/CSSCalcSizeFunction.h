/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/CSSCalcValue.h>
#include <WebCore/CSSValueTypes.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>
#include <wtf/Variant.h>

namespace WebCore {

struct ComputedStyleDependencies;

namespace CSS {

// Forward declared and stored in a UniqueRef so that a nested calc-size(), the uncommon case, does
// not grow CalcSize.
struct CalcSize;

// A keyword basis, including `any`, is held as a CSSValueID. Which sizing keywords are valid depends
// on the property the function appears in, so that is checked while parsing rather than encoded
// here. Declared outside CalcSize so UniqueRef<CalcSize> does not need a complete type.
//
// FIXME: Use CSS::LengthPercentage<> for the <calc-sum> alternatives once the unbounded range has a
// Serialize<>.
using CalcSizeBasis = Variant<CSSValueID, Ref<CSSCalc::Value>, UniqueRef<CalcSize>>;

// <calc-size()> = calc-size( <calc-size-basis>, <calc-sum> )
// <calc-size-basis> = [ <size-keyword> | <calc-sum> | <calc-size()> | any ]
//
// Valid only where a grammar names it, and nestable only inside another calc-size().
//
// Both arguments are `<calc-sum>` productions, held as calculations even when the argument is a
// single value like `10px`, so that they serialize without an enclosing `calc()`.
//
// https://drafts.csswg.org/css-values-5/#calc-size
struct CalcSize {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(CalcSize);

    CalcSize(CalcSizeBasis&&, Ref<CSSCalc::Value>&& calculation);

    CalcSize(const CalcSize&);
    CalcSize& operator=(const CalcSize&);
    CalcSize(CalcSize&&);
    CalcSize& operator=(CalcSize&&);
    ~CalcSize();

    // Returns the keyword the function acts as for everything other than resolving the size, or
    // CSSValueInvalid if the basis is not a keyword.
    CSSValueID basisKeyword() const;

    void collectComputedStyleDependencies(ComputedStyleDependencies&) const;

    bool operator==(const CalcSize&) const;

    CalcSizeBasis basis;
    Ref<CSSCalc::Value> calculation;
};
using CalcSizeFunction = FunctionNotation<CSSValueCalcSize, CalcSize>;

template<size_t I> const auto& get(const CalcSize& value)
{
    if constexpr (!I)
        return value.basis;
    else if constexpr (I == 1)
        return value.calculation;
}

template<> struct Serialize<CalcSize> {
    void operator()(StringBuilder&, const SerializationContext&, const CalcSize&);
};

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::CalcSize, 2)
