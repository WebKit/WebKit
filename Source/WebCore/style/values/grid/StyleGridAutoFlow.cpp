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
#include "StyleGridAutoFlow.h"

#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<GridAutoFlow>::operator()(BuilderState& state, const CSSValue& value) -> GridAutoFlow
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueNormal:
            return CSS::Keyword::Normal { };
        case CSSValueRow:
            return CSS::Keyword::Row { };
        case CSSValueColumn:
            return CSS::Keyword::Column { };
        case CSSValueDense:
            return CSS::Keyword::Dense { };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Row { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue, 1>(state, value);
    if (!list)
        return CSS::Keyword::Normal { };

    Ref first = list->item(0);
    switch (first->valueID()) {
    case CSSValueRow:
        if (list->size() == 2) {
            Ref second = list->item(1);
            switch (second->valueID()) {
            case CSSValueDense:
                return { CSS::Keyword::Row { }, CSS::Keyword::Dense { } };
            default:
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Row { };
            }
        }
        return CSS::Keyword::Row { };
    case CSSValueColumn:
        if (list->size() == 2) {
            Ref second = list->item(1);
            switch (second->valueID()) {
            case CSSValueDense:
                return { CSS::Keyword::Column { }, CSS::Keyword::Dense { } };
            default:
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Column { };
            }
        }
        return CSS::Keyword::Column { };
    case CSSValueDense:
        if (list->size() == 2) {
            Ref second = list->item(1);
            switch (second->valueID()) {
            case CSSValueRow:
                return { CSS::Keyword::Row { }, CSS::Keyword::Dense { } };
            case CSSValueColumn:
                return { CSS::Keyword::Column { }, CSS::Keyword::Dense { } };
            default:
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Dense { };
            }
        }
        return CSS::Keyword::Dense { };
    case CSSValueNormal:
        if (list->size() != 1)
            state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Normal { };
    default:
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Row { };
    }
}

} // namespace Style
} // namespace WebCore
