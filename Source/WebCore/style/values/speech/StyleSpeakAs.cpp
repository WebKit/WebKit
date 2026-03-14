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
#include "StyleSpeakAs.h"

#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<SpeakAs>::operator()(BuilderState& state, const CSSValue& value) -> SpeakAs
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueNone:
        case CSSValueNormal:
            return CSS::Keyword::Normal { };
        case CSSValueSpellOut:
            return { SpeakAsValue::SpellOut };
        case CSSValueDigits:
            return { SpeakAsValue::Digits };
        case CSSValueLiteralPunctuation:
            return { SpeakAsValue::LiteralPunctuation };
        case CSSValueNoPunctuation:
            return { SpeakAsValue::NoPunctuation };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(state, value);
    if (!list)
        return CSS::Keyword::Normal { };

    SpeakAsValueEnumSet result;
    for (auto& item : *list) {
        switch (item.valueID()) {
        case CSSValueSpellOut:
            result.value.add(SpeakAsValue::SpellOut);
            break;
        case CSSValueDigits:
            result.value.add(SpeakAsValue::Digits);
            break;
        case CSSValueLiteralPunctuation:
            if (result.contains(SpeakAsValue::NoPunctuation)) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Normal { };
            }
            result.value.add(SpeakAsValue::LiteralPunctuation);
            break;
        case CSSValueNoPunctuation:
            if (result.contains(SpeakAsValue::LiteralPunctuation)) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Normal { };
            }
            result.value.add(SpeakAsValue::NoPunctuation);
            break;
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
    }
    return result;
}

} // namespace Style
} // namespace WebCore
