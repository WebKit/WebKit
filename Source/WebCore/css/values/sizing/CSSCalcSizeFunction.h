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

#include <WebCore/CSSCalcTree.h>
#include <WebCore/CSSValueTypes.h>
#include <wtf/UniqueRef.h>
#include <wtf/Variant.h>

namespace WebCore {

struct ComputedStyleDependencies;

namespace CSS {

struct CalcSizeFunction;

// Each argument is a `<calc-sum>`, held as a tree so it serializes without an enclosing `calc()`.
using CalcSizeCalculation = CSSCalc::Tree;

// <calc-size-basis> = [ <size-keyword> | <calc-sum> | <calc-size()> | any ]
using CalcSizeBasis = Variant<
    Keyword::Any,
    Keyword::Auto,
    Keyword::Content,
    Keyword::MinContent,
    Keyword::WebkitMinContent,
    Keyword::MaxContent,
    Keyword::WebkitMaxContent,
    Keyword::FitContent,
    Keyword::WebkitFitContent,
    Keyword::Stretch,
    Keyword::WebkitFillAvailable,
    Keyword::Intrinsic,
    Keyword::MinIntrinsic,
    CalcSizeCalculation,
    UniqueRef<CalcSizeFunction>
>;

// <calc-size()> = calc-size( <calc-size-basis>, <calc-sum> )
//
// Valid only where a grammar names it, and nestable only inside another calc-size().
//
// https://drafts.csswg.org/css-values-5/#calc-size
struct CalcSizeParameters {
    CalcSizeParameters(CalcSizeBasis&&, CalcSizeCalculation&&);

    CalcSizeParameters(const CalcSizeParameters&);
    CalcSizeParameters& operator=(const CalcSizeParameters&);
    CalcSizeParameters(CalcSizeParameters&&);
    CalcSizeParameters& operator=(CalcSizeParameters&&);
    ~CalcSizeParameters();

    // Returns the keyword the function acts as other than for resolving the size, or CSSValueID::Invalid.
    CSSValueID basisKeyword() const;

    void collectComputedStyleDependencies(ComputedStyleDependencies&) const;

    bool operator==(const CalcSizeParameters&) const;

    CalcSizeBasis basis;
    CalcSizeCalculation calculation;
};

template<size_t I> const auto& get(const CalcSizeParameters& value)
{
    if constexpr (!I)
        return value.basis;
    else if constexpr (I == 1)
        return value.calculation;
}

// Wrapped in a named type because the basis holds this recursively, and an alias to a template
// instantiation cannot be forward declared.
using CalcSizeFunctionValue = FunctionNotation<CSSValueID::CalcSize, CalcSizeParameters>;
DEFINE_TYPE_WRAPPER(CalcSizeFunction, CalcSizeFunctionValue);

// Overload of operator== for UniqueRef<CalcSizeFunction> to make CalcSizeBasis's operator== work.
inline bool operator==(const UniqueRef<CalcSizeFunction>& a, const UniqueRef<CalcSizeFunction>& b)
{
    return arePointingToEqualData(a, b);
}

template<> struct Serialize<CalcSizeParameters> {
    void operator()(StringBuilder&, const SerializationContext&, const CalcSizeParameters&);
};

} // namespace CSS
} // namespace WebCore

DEFINE_COMMA_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::CalcSizeParameters, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::CSS::CalcSizeFunction)
