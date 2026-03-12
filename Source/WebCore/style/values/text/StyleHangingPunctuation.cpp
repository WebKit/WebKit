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
#include "StyleHangingPunctuation.h"

#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<HangingPunctuation>::operator()(BuilderState& state, const CSSValue& value) -> HangingPunctuation
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueNone:
            return CSS::Keyword::None { };
        case CSSValueFirst:
            return { HangingPunctuationValue::First };
        case CSSValueForceEnd:
            return { HangingPunctuationValue::ForceEnd };
        case CSSValueAllowEnd:
            return { HangingPunctuationValue::AllowEnd };
        case CSSValueLast:
            return { HangingPunctuationValue::Last };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(state, value);
    if (!list)
        return CSS::Keyword::None { };

    HangingPunctuationValueEnumSet result;
    for (auto& item : *list) {
        switch (item.valueID()) {
        case CSSValueFirst:
            result.value.add(HangingPunctuationValue::First);
            break;
        case CSSValueForceEnd:
            if (result.contains(HangingPunctuationValue::AllowEnd)) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::None { };
            }
            result.value.add(HangingPunctuationValue::ForceEnd);
            break;
        case CSSValueAllowEnd:
            if (result.contains(HangingPunctuationValue::ForceEnd)) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::None { };
            }
            result.value.add(HangingPunctuationValue::AllowEnd);
            break;
        case CSSValueLast:
            result.value.add(HangingPunctuationValue::Last);
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
