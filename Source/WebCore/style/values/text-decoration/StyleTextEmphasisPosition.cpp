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
#include "StyleTextEmphasisPosition.h"

#include "CSSKeywordValue.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<TextEmphasisPosition>::operator()(BuilderState& state, const CSSValue& value) -> TextEmphasisPosition
{
    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueOver:
            return { TextEmphasisPositionValue::Over, TextEmphasisPositionValue::Right };
        case CSSValueUnder:
            return { TextEmphasisPositionValue::Under, TextEmphasisPositionValue::Right };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return { TextEmphasisPositionValue::Over, TextEmphasisPositionValue::Right };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSKeywordValue>(state, value);
    if (!list)
        return { TextEmphasisPositionValue::Over, TextEmphasisPositionValue::Right };

    TextEmphasisPositionValueEnumSet result;
    for (Ref item : *list) {
        switch (item->valueID()) {
        case CSSValueOver:
            if (result.contains(TextEmphasisPositionValue::Under)) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return { TextEmphasisPositionValue::Over, TextEmphasisPositionValue::Right };
            }
            result.value.add(TextEmphasisPositionValue::Over);
            break;
        case CSSValueUnder:
            if (result.contains(TextEmphasisPositionValue::Over)) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return { TextEmphasisPositionValue::Over, TextEmphasisPositionValue::Right };
            }
            result.value.add(TextEmphasisPositionValue::Under);
            break;
        case CSSValueLeft:
            if (result.contains(TextEmphasisPositionValue::Right)) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return { TextEmphasisPositionValue::Over, TextEmphasisPositionValue::Right };
            }
            result.value.add(TextEmphasisPositionValue::Left);
            break;
        case CSSValueRight:
            if (result.contains(TextEmphasisPositionValue::Left)) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return { TextEmphasisPositionValue::Over, TextEmphasisPositionValue::Right };
            }
            result.value.add(TextEmphasisPositionValue::Right);
            break;
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return { TextEmphasisPositionValue::Over, TextEmphasisPositionValue::Right };
        }
    }

    // Result must contain either `over` or `under`.
    if (!result.containsAny({ TextEmphasisPositionValue::Over, TextEmphasisPositionValue::Under })) {
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return { TextEmphasisPositionValue::Over, TextEmphasisPositionValue::Right };
    }

    // If neither `left` nor `right` has been specified, `right` is added as the default.
    if (!result.containsAny({ TextEmphasisPositionValue::Left, TextEmphasisPositionValue::Right }))
        result.value.add(TextEmphasisPositionValue::Right);

    return result;
}

} // namespace Style
} // namespace WebCore
