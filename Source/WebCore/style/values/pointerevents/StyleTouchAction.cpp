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
#include "StyleTouchAction.h"

#include "CSSKeywordValue.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<TouchAction>::operator()(BuilderState& state, const CSSValue& value) -> TouchAction
{
    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueAuto:
            return CSS::Keyword::Auto { };
        case CSSValueNone:
            return CSS::Keyword::None { };
        case CSSValueManipulation:
            return CSS::Keyword::Manipulation { };
        case CSSValuePanX:
            return TouchActionValue::PanX;
        case CSSValuePanY:
            return TouchActionValue::PanY;
        case CSSValuePinchZoom:
            return TouchActionValue::PinchZoom;
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Auto { };
        }
    }

    auto list = requiredListDowncast<CSSValueList, CSSKeywordValue>(state, value);
    if (!list)
        return CSS::Keyword::Auto { };

    TouchActionValueEnumSet result;
    for (Ref item : *list) {
        switch (item->valueID()) {
        case CSSValuePanX:
            result.value.add(TouchActionValue::PanX);
            break;
        case CSSValuePanY:
            result.value.add(TouchActionValue::PanY);
            break;
        case CSSValuePinchZoom:
            result.value.add(TouchActionValue::PinchZoom);
            break;
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Auto { };
        }
    }
    return result;
}

// MARK: - Platform

auto ToPlatform<TouchAction>::operator()(const TouchAction& value) -> OptionSet<WebCore::TouchAction>
{
    return WTF::switchOn(value,
        [](const CSS::Keyword::Auto&) -> OptionSet<WebCore::TouchAction> {
            return WebCore::TouchAction::Auto;
        },
        [](const CSS::Keyword::None&) -> OptionSet<WebCore::TouchAction> {
            return WebCore::TouchAction::None;
        },
        [](const CSS::Keyword::Manipulation&) -> OptionSet<WebCore::TouchAction> {
            return WebCore::TouchAction::Manipulation;
        },
        [](const TouchActionValueEnumSet& set) -> OptionSet<WebCore::TouchAction> {
            OptionSet<WebCore::TouchAction> result;
            for (auto action : set) {
                switch (action) {
                case TouchActionValue::PanX:
                    result.add(WebCore::TouchAction::PanX);
                    break;
                case TouchActionValue::PanY:
                    result.add(WebCore::TouchAction::PanY);
                    break;
                case TouchActionValue::PinchZoom:
                    result.add(WebCore::TouchAction::PinchZoom);
                    break;
                }
            }
            return result;
        }
    );
}

} // namespace Style
} // namespace WebCore
