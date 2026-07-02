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

#include "config.h"
#include "StyleTextTransform.h"

#include "CSSKeywordValue.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<TextTransform>::operator()(BuilderState& state, const CSSValue& value) -> TextTransform
{
    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueNone:
            return CSS::Keyword::None { };
        case CSSValueCapitalize:
            return { TextTransformValue::Capitalize };
        case CSSValueUppercase:
            return { TextTransformValue::Uppercase };
        case CSSValueLowercase:
            return { TextTransformValue::Lowercase };
        case CSSValueFullWidth:
            return { TextTransformValue::FullWidth };
        case CSSValueFullSizeKana:
            return { TextTransformValue::FullSizeKana };
        case CSSValueMathAuto:
            return { TextTransformValue::MathAuto };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSKeywordValue>(state, value);
    if (!list)
        return CSS::Keyword::None { };

    TextTransformValueEnumSet result;
    for (Ref item : *list) {
        switch (item->valueID()) {
        case CSSValueCapitalize:
            if (result.containsAny({ TextTransformValue::Uppercase, TextTransformValue::Lowercase })) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::None { };
            }
            result.value.add(TextTransformValue::Capitalize);
            break;
        case CSSValueUppercase:
            if (result.containsAny({ TextTransformValue::Capitalize, TextTransformValue::Lowercase })) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::None { };
            }
            result.value.add(TextTransformValue::Uppercase);
            break;
        case CSSValueLowercase:
            if (result.containsAny({ TextTransformValue::Capitalize, TextTransformValue::Uppercase })) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::None { };
            }
            result.value.add(TextTransformValue::Lowercase);
            break;
        case CSSValueFullWidth:
            result.value.add(TextTransformValue::FullWidth);
            break;
        case CSSValueFullSizeKana:
            result.value.add(TextTransformValue::FullSizeKana);
            break;
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }
    }
    return result;
}

} // namespace Style
} // namespace WebCore
