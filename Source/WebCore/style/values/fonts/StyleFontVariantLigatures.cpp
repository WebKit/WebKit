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

#include "config.h"
#include "StyleFontVariantLigatures.h"

#include "CSSKeywordValue.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<FontVariantLigatures>::operator()(BuilderState& state, const CSSValue& value) -> FontVariantLigatures
{
    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueID::Normal:
            return CSS::Keyword::Normal { };
        case CSSValueID::None:
            return CSS::Keyword::None { };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSKeywordValue>(state, value);
    if (!list)
        return CSS::Keyword::Normal { };

    using enum WebCore::FontVariantLigatures;

    auto common = Normal;
    auto discretionary = Normal;
    auto historical = Normal;
    auto contextual = Normal;

    for (auto& item : *list) {
        switch (item.valueID()) {
        case CSSValueID::NoCommonLigatures:
            common = No;
            break;
        case CSSValueID::CommonLigatures:
            common = Yes;
            break;
        case CSSValueID::NoDiscretionaryLigatures:
            discretionary = No;
            break;
        case CSSValueID::DiscretionaryLigatures:
            discretionary = Yes;
            break;
        case CSSValueID::NoHistoricalLigatures:
            historical = No;
            break;
        case CSSValueID::HistoricalLigatures:
            historical = Yes;
            break;
        case CSSValueID::Contextual:
            contextual = Yes;
            break;
        case CSSValueID::NoContextual:
            contextual = No;
            break;
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
    }

    return FontVariantLigatures::Platform { common, discretionary, historical, contextual };
}

} // namespace Style
} // namespace WebCore
