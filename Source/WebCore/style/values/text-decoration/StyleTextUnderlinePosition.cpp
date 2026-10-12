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
#include "StyleTextUnderlinePosition.h"

#include "CSSKeywordValue.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<TextUnderlinePosition>::operator()(BuilderState& state, const CSSValue& value) -> TextUnderlinePosition
{
    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueID::Auto:
            return CSS::Keyword::Auto { };
        case CSSValueID::FromFont:
            return { TextUnderlinePositionValue::FromFont };
        case CSSValueID::Under:
            return { TextUnderlinePositionValue::Under };
        case CSSValueID::Left:
            return { TextUnderlinePositionValue::Left };
        case CSSValueID::Right:
            return { TextUnderlinePositionValue::Right };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Auto { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSKeywordValue>(state, value);
    if (!list)
        return CSS::Keyword::Auto { };

    TextUnderlinePositionValueEnumSet result;
    for (auto& item : *list) {
        switch (item.valueID()) {
        case CSSValueID::FromFont:
            if (result.contains(TextUnderlinePositionValue::Under)) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Auto { };
            }
            result.value.add(TextUnderlinePositionValue::FromFont);
            break;
        case CSSValueID::Under:
            if (result.contains(TextUnderlinePositionValue::FromFont)) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Auto { };
            }
            result.value.add(TextUnderlinePositionValue::Under);
            break;
        case CSSValueID::Left:
            if (result.contains(TextUnderlinePositionValue::Right)) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Auto { };
            }
            result.value.add(TextUnderlinePositionValue::Left);
            break;
        case CSSValueID::Right:
            if (result.contains(TextUnderlinePositionValue::Left)) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Auto { };
            }
            result.value.add(TextUnderlinePositionValue::Right);
            break;
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Auto { };
        }
    }
    return result;
}

} // namespace Style
} // namespace WebCore
