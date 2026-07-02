/*
 * Copyright (C) 2026 Suraj Thanugundla <contact@surajt.com>
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
#include "StyleOverflowClipMargin.h"

#include "StyleBuilderChecking.h"
#include "StyleKeyword+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<OverflowClipMargin>::operator()(BuilderState& state, const CSSValue& value) -> OverflowClipMargin
{
    using namespace CSS::Literals;

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return toStyleFromCSSValue<OverflowClipMargin::Length>(state, *primitiveValue);

    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (auto valueID = keywordValue->valueID(); valueID) {
        case CSSValueBorderBox:
        case CSSValueContentBox:
        case CSSValuePaddingBox:
            return fromCSSValueID<VisualBox>(valueID);
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return 0_css_px;
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSValue, 2>(state, value);
    if (!list)
        return 0_css_px;

    std::optional<VisualBox> referenceBox;
    std::optional<OverflowClipMargin::Length> length;

    for (Ref item : *list) {
        if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(item)) {
            if (length) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return 0_css_px;
            }
            length = toStyleFromCSSValue<OverflowClipMargin::Length>(state, *primitiveValue);
        } else if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(item)) {
            if (referenceBox) {
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return 0_css_px;
            }
            switch (auto valueID = keywordValue->valueID(); valueID) {
            case CSSValueBorderBox:
            case CSSValueContentBox:
            case CSSValuePaddingBox:
                referenceBox = fromCSSValueID<VisualBox>(valueID);
                break;
            default:
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return 0_css_px;
            }
        } else {
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return 0_css_px;
        }
    }

    return { *referenceBox, *length };
}

} // namespace Style
} // namespace WebCore
