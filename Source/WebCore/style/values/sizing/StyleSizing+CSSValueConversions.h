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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "StylePrimitiveNumericOrKeyword+CSSValueConversions.h"
#include "StyleSizing.h"

namespace WebCore {
namespace Style {

template<typename T> concept SupportsAutoKeyword = requires {
    T(CSS::Keyword::Auto { });
};

template<typename SizeType>
auto convertFromCSSValueForSizeType(const CSSValue& value, const BuilderState& state) -> SizeType
{
    switch (value.valueID()) {
    case CSSValueInvalid:
        return convertFromCSSValue<LengthPercentage<CSS::Nonnegative>>(value, state);
    case CSSValueAuto:
        if constexpr (std::constructible_from<SizeType, CSS::Keyword::Auto>)
            return CSS::Keyword::Auto { };
        else
            WTF_UNREACHABLE();
    case CSSValueNone:
        if constexpr (std::constructible_from<SizeType, CSS::Keyword::None>)
            return CSS::Keyword::None { };
        else
            WTF_UNREACHABLE();
    case CSSValueContent:
        if constexpr (std::constructible_from<SizeType, CSS::Keyword::Content>)
            return CSS::Keyword::Content { };
        else
            WTF_UNREACHABLE();
    case CSSValueMinContent:
    case CSSValueWebkitMinContent:
        return CSS::Keyword::MinContent { };
    case CSSValueMaxContent:
    case CSSValueWebkitMaxContent:
        return CSS::Keyword::MaxContent { };
    case CSSValueFitContent:
    case CSSValueWebkitFitContent:
        return CSS::Keyword::FitContent { };
    case CSSValueIntrinsic:
        return CSS::Keyword::Intrinsic { };
    case CSSValueMinIntrinsic:
        return CSS::Keyword::MinIntrinsic { };
    case CSSValueWebkitFillAvailable:
        return CSS::Keyword::WebkitFillAvailable { };
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

} // namespace Style
} // namespace WebCore
