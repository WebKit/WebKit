/*
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleInitialLetter.h"

#include "CSSKeywordValue.h"
#include "CSSKeywordValueInlines.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueKeywords.h"
#include "CSSValuePair.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<InitialLetter>::operator()(BuilderState& state, const CSSValue& value) -> InitialLetter
{
    // The parser produces: `normal` (keyword), `<number>` (size only), or a pair of
    // `<number>` and either an `<integer>` sink or a `drop`/`raise` keyword.

    if (valueID(value) == CSSValueNormal)
        return CSS::Keyword::Normal { };

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return { toStyleFromCSSValue<InitialLetter::Size>(state, *primitiveValue) };

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
    auto size = toStyleFromCSSValue<InitialLetter::Size>(state, *sizeValue);

    auto& second = pair->second();
    if (valueID(second) == CSSValueDrop)
        return { size, CSS::Keyword::Drop { } };
    if (valueID(second) == CSSValueRaise)
        return { size, CSS::Keyword::Raise { } };

    if (RefPtr sinkValue = dynamicDowncast<CSSPrimitiveValue>(second))
        return { size, toStyleFromCSSValue<InitialLetter::Sink>(state, *sinkValue) };

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return CSS::Keyword::Normal { };
}

} // namespace Style
} // namespace WebCore
