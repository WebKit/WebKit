/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2026 ChangSeok Oh <changseok@webkit.org>
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
#include "StyleWebKitInitialLetter.h"

#include "CSSKeywordValue.h"
#include "CSSKeywordValueInlines.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<WebkitInitialLetter>::operator()(BuilderState& state, const CSSValue& value) -> WebkitInitialLetter
{
    // The value's shape identifies the property: consumeInitialLetter() emits a list for the
    // standard `<number> <integer>` form specifically so it cannot be mistaken for the legacy
    // `<number>{1,2}` pair.

    if (valueID(value) == CSSValueID::Normal)
        return CSS::Keyword::Normal { };

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return { toStyleFromCSSValue<WebkitInitialLetter::Size>(state, *primitiveValue) };

    if (RefPtr list = dynamicDowncast<CSSValueList>(value)) {
        if (list->size() != 2) {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
        RefPtr sizeValue = dynamicDowncast<CSSPrimitiveValue>(list->item(0));
        RefPtr sinkValue = dynamicDowncast<CSSPrimitiveValue>(list->item(1));
        if (!sizeValue || !sinkValue) {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Normal { };
        }
        return {
            toStyleFromCSSValue<WebkitInitialLetter::Size>(state, *sizeValue),
            toStyleFromCSSValue<WebkitInitialLetter::Sink>(state, *sinkValue)
        };
    }

    RefPtr pair = dynamicDowncast<CSSValuePair>(value);
    if (!pair) {
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Normal { };
    }

    RefPtr sizeValue = dynamicDowncast<CSSPrimitiveValue>(pair->first());
    if (!sizeValue) {
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Normal { };
    }
    auto size = toStyleFromCSSValue<WebkitInitialLetter::Size>(state, *sizeValue);

    auto& second = pair->second();
    if (valueID(second) == CSSValueID::Drop)
        return { size, CSS::Keyword::Drop { } };
    if (valueID(second) == CSSValueID::Raise)
        return { size, CSS::Keyword::Raise { } };

    RefPtr sinkValue = dynamicDowncast<CSSPrimitiveValue>(second);
    if (!sinkValue) {
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Normal { };
    }
    auto sink = toStyleFromCSSValue<WebkitInitialLetter::Sink>(state, *sinkValue);

    // The legacy type encoded `normal` as both-zero; preserve that.
    if (!size.value && !sink.value)
        return CSS::Keyword::Normal { };

    return { WebkitInitialLetter::LegacyTag { }, size, sink };
}

} // namespace Style
} // namespace WebCore
