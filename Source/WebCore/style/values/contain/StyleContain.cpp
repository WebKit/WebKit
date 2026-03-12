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
#include "StyleContain.h"

#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<Contain>::operator()(BuilderState& state, const CSSValue& value) -> Contain
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueNone:
            return CSS::Keyword::None { };
        case CSSValueStrict:
            return CSS::Keyword::Strict { };
        case CSSValueContent:
            return CSS::Keyword::Content { };
        case CSSValueSize:
            return { ContainValue::Size };
        case CSSValueInlineSize:
            return { ContainValue::InlineSize };
        case CSSValueLayout:
            return { ContainValue::Layout };
        case CSSValueStyle:
            return { ContainValue::Style };
        case CSSValuePaint:
            return { ContainValue::Paint };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(state, value);
    if (!list)
        return CSS::Keyword::None { };

    ContainValueEnumSet result;
    for (auto& item : *list) {
        switch (item.valueID()) {
        case CSSValueSize:
            if (result.contains(ContainValue::InlineSize)) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::None { };
            }
            result.value.add(ContainValue::Size);
            break;
        case CSSValueInlineSize:
            if (result.contains(ContainValue::Size)) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::None { };
            }
            result.value.add(ContainValue::InlineSize);
            break;
        case CSSValueLayout:
            result.value.add(ContainValue::Layout);
            break;
        case CSSValueStyle:
            result.value.add(ContainValue::Style);
            break;
        case CSSValuePaint:
            result.value.add(ContainValue::Paint);
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
