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

#include "StyleBuilderChecking.h"
#include "StylePrimitiveKeyword+CSSValueConversion.h"
#include "StyleTextEdge.h"

namespace WebCore {
namespace Style {

template<TextEdgeDerived T> struct CSSValueConversion<T> {
    auto operator()(BuilderState& state, const CSSValue& value) -> T
    {
        using Keyword = typename T::Keyword;

        if (is<CSSPrimitiveValue>(value)) {
            switch (value.valueID()) {
            case Keyword::value:
                return Keyword { };
            case CSSValueText:
                return { TextEdgeOver::Text, TextEdgeUnder::Text };
            case CSSValueIdeographic:
                return { TextEdgeOver::Ideographic, TextEdgeUnder::Ideographic };
            case CSSValueIdeographicInk:
                return { TextEdgeOver::IdeographicInk, TextEdgeUnder::IdeographicInk };
            case CSSValueCap:
                return { TextEdgeOver::Cap, TextEdgeUnder::Text };
            case CSSValueEx:
                return { TextEdgeOver::Ex, TextEdgeUnder::Text };
            default:
                break;
            }

            state.setCurrentPropertyInvalidAtComputedValueTime();
            return Keyword { };
        }

        auto pair = requiredPairDowncast<CSSPrimitiveValue>(state, value);
        if (!pair)
            return Keyword { };

        return {
            toStyleFromCSSValue<TextEdgeOver>(state, pair->first),
            toStyleFromCSSValue<TextEdgeUnder>(state, pair->second),
        };
    }
};

} // namespace Style
} // namespace WebCore
