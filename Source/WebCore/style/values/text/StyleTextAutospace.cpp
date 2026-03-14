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
#include "StyleTextAutospace.h"

#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<TextAutospace>::operator()(BuilderState& state, const CSSValue& value) -> TextAutospace
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueNormal:
            return CSS::Keyword::Normal { };
        case CSSValueAuto:
            return CSS::Keyword::Auto { };
        case CSSValueNoAutospace:
            return CSS::Keyword::NoAutospace { };
        case CSSValueIdeographAlpha:
            return CSS::Keyword::IdeographAlpha { };
        case CSSValueIdeographNumeric:
            return CSS::Keyword::IdeographNumeric { };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::NoAutospace { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(state, value);
    if (!list)
        return CSS::Keyword::NoAutospace { };

    if (list->size() == 1) {
        switch (auto& first = list->item(0); first.valueID()) {
        case CSSValueNormal:
            return CSS::Keyword::Normal { };
        case CSSValueAuto:
            return CSS::Keyword::Auto { };
        case CSSValueNoAutospace:
            return CSS::Keyword::NoAutospace { };
        case CSSValueIdeographAlpha:
            return CSS::Keyword::IdeographAlpha { };
        case CSSValueIdeographNumeric:
            return CSS::Keyword::IdeographNumeric { };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::NoAutospace { };
        }
    }
    if (list->size() == 2) {
        switch (Ref first = list->item(0); first->valueID()) {
        case CSSValueIdeographAlpha:
            if (Ref second = list->item(1); second->valueID() == CSSValueIdeographNumeric)
                return { CSS::Keyword::IdeographAlpha { }, CSS::Keyword::IdeographNumeric { } };
            break;
        case CSSValueIdeographNumeric:
            if (Ref second = list->item(1); second->valueID() == CSSValueIdeographAlpha)
                return { CSS::Keyword::IdeographAlpha { }, CSS::Keyword::IdeographNumeric { } };
            break;
        default:
            break;
        }
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::NoAutospace { };
    }

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return CSS::Keyword::NoAutospace { };
}

} // namespace Style
} // namespace WebCore
