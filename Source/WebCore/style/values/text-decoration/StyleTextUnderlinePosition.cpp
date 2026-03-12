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

#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<TextUnderlinePosition>::operator()(BuilderState& state, const CSSValue& value) -> TextUnderlinePosition
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueAuto:
            return CSS::Keyword::Auto { };
        case CSSValueFromFont:
            return { TextUnderlinePositionValue::FromFont };
        case CSSValueUnder:
            return { TextUnderlinePositionValue::Under };
        case CSSValueLeft:
            return { TextUnderlinePositionValue::Left };
        case CSSValueRight:
            return { TextUnderlinePositionValue::Right };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Auto { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(state, value);
    if (!list)
        return CSS::Keyword::Auto { };

    TextUnderlinePositionValueEnumSet result;
    for (auto& item : *list) {
        switch (item.valueID()) {
        case CSSValueFromFont:
            if (result.contains(TextUnderlinePositionValue::Under)) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Auto { };
            }
            result.value.add(TextUnderlinePositionValue::FromFont);
            break;
        case CSSValueUnder:
            if (result.contains(TextUnderlinePositionValue::FromFont)) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Auto { };
            }
            result.value.add(TextUnderlinePositionValue::Under);
            break;
        case CSSValueLeft:
            if (result.contains(TextUnderlinePositionValue::Right)) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Auto { };
            }
            result.value.add(TextUnderlinePositionValue::Left);
            break;
        case CSSValueRight:
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
