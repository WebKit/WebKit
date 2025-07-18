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
#include "StyleTextIndent.h"

#include "StyleBuilderChecking.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Blending.h"

namespace WebCore {
namespace Style {

auto CSSValueConversion<TextIndent>::operator()(BuilderState& state, const CSSValue& value) -> TextIndent
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return toStyleFromCSSValue<TextIndentLength>(state, *primitiveValue);

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(state, value);
    if (!list)
        return 0_css_px;

    std::optional<TextIndentLength> length;
    std::optional<CSS::Keyword::Hanging> hanging;
    std::optional<CSS::Keyword::EachLine> eachLine;

    for (RefPtr primitiveValue : *list) {
        if (primitiveValue->valueID() == CSSValueHanging)
            hanging = CSS::Keyword::Hanging { };
        else if (primitiveValue->valueID() == CSSValueEachLine)
            eachLine = CSS::Keyword::EachLine { };
        else
            length = toStyleFromCSSValue<TextIndentLength>(state, *primitiveValue);
    }

    if (!length) {
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return 0_css_px;
    }

    return TextIndent { WTFMove(*length), hanging, eachLine };
}

// MARK: - Blending

auto Blending<TextIndent>::canBlend(const TextIndent& a, const TextIndent& b) -> bool
{
    return a.hanging == b.hanging
        && a.eachLine == b.eachLine
        && Style::canBlend(a.length, b.length);
}

auto Blending<TextIndent>::blend(const TextIndent& a, const TextIndent& b, const BlendingContext& context) -> TextIndent
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    ASSERT(a.hanging == b.hanging);
    ASSERT(a.eachLine == b.eachLine);
    return TextIndent { Style::blend(a.length, b.length, context), a.hanging, a.eachLine };
}

} // namespace Style
} // namespace WebCore
