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

// <steps-easing-function> = steps( <integer>, <steps-easing-function-position>? )
// <steps-easing-function-position> = jump-start | jump-end | jump-none | jump-both | start | end
//
// with range constraints, this is:
//
// <steps-easing-function> = steps( <integer [1,∞]>, jump-start )
//                         | steps( <integer [1,∞]>, jump-end )
//                         | steps( <integer [1,∞]>, jump-both )
//                         | steps( <integer [1,∞]>, start )
//                         | steps( <integer [1,∞]>, end )
//                         | steps( <integer [2,∞]>, jump-none )
// https://drafts.csswg.org/css-easing-2/#funcdef-steps
struct StepsEasingParameters {
    enum class ShouldSerializeKeyword : bool { No, Yes };

    template<typename T, typename Keyword, auto shouldSerializeKeyword = ShouldSerializeKeyword::Yes>
    struct Kind {
        static constexpr Keyword keyword = Keyword { };
        T steps;

        bool operator==(const Kind&) const = default;
    };

    using JumpStart = Kind<Integer<Range{1,Range::infinity}>, Keyword::JumpStart>;
    using JumpEnd   = Kind<Integer<Range{1,Range::infinity}>, Keyword::JumpEnd, ShouldSerializeKeyword::No>;
    using JumpBoth  = Kind<Integer<Range{1,Range::infinity}>, Keyword::JumpBoth>;
    using Start     = Kind<Integer<Range{1,Range::infinity}>, Keyword::Start>;
    using End       = Kind<Integer<Range{1,Range::infinity}>, Keyword::End, ShouldSerializeKeyword::No>;
    using JumpNone  = Kind<Integer<Range{2,Range::infinity}>, Keyword::JumpNone>;

    std::variant<
        JumpStart,
        JumpEnd,
        JumpBoth,
        Start,
        End,
        JumpNone
    > value;

    bool operator==(const StepsEasingParameters&) const = default;
};
using StepsEasingFunction = FunctionNotation<CSSValueSteps, StepsEasingParameters>;

DEFINE_TYPE_WRAPPER(StepsEasingParameters, value);

template<size_t I, typename T, typename K, auto shouldSerializeKeyword> const auto& get(const StepsEasingParameters::Kind<T, K, shouldSerializeKeyword>& value)
{
    return value.steps;
}

template<typename T, typename K, auto shouldSerializeKeyword> struct Serialize<StepsEasingParameters::Kind<T, K, shouldSerializeKeyword>> {
    void operator()(StringBuilder& builder, const StepsEasingParameters::Kind<T, K, shouldSerializeKeyword>& value)
    {
        serializationForCSS(builder, value.steps);
        if constexpr (shouldSerializeKeyword == StepsEasingParameters::ShouldSerializeKeyword::Yes) {
            builder.append(", "_s);
            serializationForCSS(builder, value.keyword);
        }
    }
};

} // namespace CSS
} // namespace WebCore

CSS_TUPLE_LIKE_CONFORMANCE(StepsEasingParameters, 1)

namespace std {

template<typename T, typename K, auto shouldSerializeKeyword> class tuple_size<WebCore::CSS::StepsEasingParameters::Kind<T, K, shouldSerializeKeyword>> : public std::integral_constant<size_t, 1> { };
template<size_t I, typename T, typename K, auto shouldSerializeKeyword> class tuple_element<I, WebCore::CSS::StepsEasingParameters::Kind<T, K, shouldSerializeKeyword>> {
public:
    using type = T;
};

}

template<typename T, typename K, auto shouldSerializeKeyword> inline constexpr bool WebCore::TreatAsTupleLike<WebCore::CSS::StepsEasingParameters::Kind<T, K, shouldSerializeKeyword>> = true;
