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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleMarginTrim.h"

#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<MarginTrim>::operator()(BuilderState& state, const CSSValue& value) -> MarginTrim
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueNone:
            return CSS::Keyword::None { };
        case CSSValueBlock:
            return MarginTrim::blockTrims();
        case CSSValueInline:
            return MarginTrim::inlineTrims();
        case CSSValueBlockStart:
            return CSS::Keyword::BlockStart { };
        case CSSValueBlockEnd:
            return CSS::Keyword::BlockEnd { };
        case CSSValueInlineStart:
            return CSS::Keyword::InlineStart { };
        case CSSValueInlineEnd:
            return CSS::Keyword::InlineEnd { };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(state, value);
    if (!list)
        return CSS::Keyword::None { };

    MarginTrim result;

    for (Ref item : *list) {
        switch (item->valueID()) {
        case CSSValueBlock:
            result.add(MarginTrim::blockTrims());
            break;
        case CSSValueInline:
            result.add(MarginTrim::inlineTrims());
            break;
        case CSSValueBlockStart:
            result.add(CSS::Keyword::BlockStart { });
            break;
        case CSSValueBlockEnd:
            result.add(CSS::Keyword::BlockEnd { });
            break;
        case CSSValueInlineStart:
            result.add(CSS::Keyword::InlineStart { });
            break;
        case CSSValueInlineEnd:
            result.add(CSS::Keyword::InlineEnd { });
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
