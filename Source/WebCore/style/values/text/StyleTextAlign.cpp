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
#include "StyleTextAlign.h"

#include "Document.h"
#include "Element.h"
#include "RenderStyle.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveKeyword+CSSValueConversion.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<TextAlign>::operator()(BuilderState& state, const CSSValue& value) -> TextAlign
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return TextAlign::Start;

    if (!primitiveValue->isValueID()) {
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return TextAlign::Start;
    }

    auto& parentStyle = state.parentStyle();

    // User agents are expected to have a rule in their user agent stylesheet that matches 'th' elements that have a parent
    // node whose computed value for the 'text-align' property is its initial value, whose declaration block consists of
    // just a single declaration that sets the 'text-align' property to the value 'center'.
    // https://html.spec.whatwg.org/multipage/rendering.html#rendering
    if (primitiveValue->valueID() == CSSValueInternalThCenter) {
        if (parentStyle.textAlign() == TextAlign::Start)
            return TextAlign::Center;
        return parentStyle.textAlign();
    }

    if (primitiveValue->valueID() == CSSValueWebkitMatchParent || primitiveValue->valueID() == CSSValueMatchParent) {
        RefPtr element = state.element();

        if (element && element == state.document().documentElement())
            return TextAlign::Start;
        if (parentStyle.textAlign() == TextAlign::Start)
            return parentStyle.writingMode().isBidiLTR() ? TextAlign::Left : TextAlign::Right;
        if (parentStyle.textAlign() == TextAlign::End)
            return parentStyle.writingMode().isBidiLTR() ? TextAlign::Right : TextAlign::Left;

        return parentStyle.textAlign();
    }

    return fromCSSValue<TextAlign>(value);
}

} // namespace Style
} // namespace WebCore
