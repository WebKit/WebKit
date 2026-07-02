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
#include "StyleMarginTrim.h"

#include "CSSKeywordValue.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<MarginTrim>::operator()(BuilderState& state, const CSSValue& value) -> MarginTrim
{
    // See if value is "block" or "inline" before trying to parse a list
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueBlock:
            return { Style::MarginTrimSide::BlockStart, Style::MarginTrimSide::BlockEnd };
        case CSSValueInline:
            return { Style::MarginTrimSide::InlineStart, Style::MarginTrimSide::InlineEnd };
        case CSSValueBlockStart:
            return { Style::MarginTrimSide::BlockStart };
        case CSSValueBlockEnd:
            return { Style::MarginTrimSide::BlockEnd };
        case CSSValueInlineStart:
            return { Style::MarginTrimSide::InlineStart };
        case CSSValueInlineEnd:
            return { Style::MarginTrimSide::InlineEnd };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSKeywordValue>(state, value);
    if (!list)
        return CSS::Keyword::None { };

    MarginTrimSideEnumSet result;
    for (Ref item : *list) {
        switch (item->valueID()) {
        case CSSValueBlock:
            result.value.add({ Style::MarginTrimSide::BlockStart, Style::MarginTrimSide::BlockEnd });
            break;
        case CSSValueInline:
            result.value.add({ Style::MarginTrimSide::InlineStart, Style::MarginTrimSide::InlineEnd });
            break;
        case CSSValueBlockStart:
            result.value.add(Style::MarginTrimSide::BlockStart);
            break;
        case CSSValueBlockEnd:
            result.value.add(Style::MarginTrimSide::BlockEnd);
            break;
        case CSSValueInlineStart:
            result.value.add(Style::MarginTrimSide::InlineStart);
            break;
        case CSSValueInlineEnd:
            result.value.add(Style::MarginTrimSide::InlineEnd);
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
