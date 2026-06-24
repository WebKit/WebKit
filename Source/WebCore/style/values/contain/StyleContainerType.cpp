/*
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleContainerType.h"

#include "CSSKeywordValue.h"
#include "CSSValuePair.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<ContainerType>::operator()(BuilderState& state, const CSSValue& value) -> ContainerType
{
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueNormal:
            return CSS::Keyword::Normal { };
        case CSSValueSize:
            return { ContainerTypeValue::Size };
        case CSSValueInlineSize:
            return { ContainerTypeValue::InlineSize };
        case CSSValueScrollState:
            return { ContainerTypeValue::ScrollState };
        default:
            break;
        }
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Normal { };
    }

    // The `[ size | inline-size ] || scroll-state` combination arrives as a CSSValuePair.
    if (RefPtr pair = dynamicDowncast<CSSValuePair>(value)) {
        ContainerType::EnumSet result;
        auto addComponent = [&](const CSSValue& component) -> bool {
            RefPtr keyword = dynamicDowncast<CSSKeywordValue>(component);
            if (!keyword)
                return false;
            switch (keyword->valueID()) {
            case CSSValueSize:
                result.value.add(ContainerTypeValue::Size);
                return true;
            case CSSValueInlineSize:
                result.value.add(ContainerTypeValue::InlineSize);
                return true;
            case CSSValueScrollState:
                result.value.add(ContainerTypeValue::ScrollState);
                return true;
            default:
                return false;
            }
        };
        if (addComponent(pair->first()) && addComponent(pair->second()))
            return result;
    }

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return CSS::Keyword::Normal { };
}

} // namespace Style
} // namespace WebCore
