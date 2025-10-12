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
#include "StyleWebKitTextStrokeWidth.h"

#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<WebkitTextStrokeWidth>::operator()(BuilderState& state, const CSSValue& value) -> WebkitTextStrokeWidth
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return 0_css_px;

    auto handleKeyword = [&](double result) -> WebkitTextStrokeWidth {
        Ref emsValue = CSSPrimitiveValue::create(result, CSSUnitType::CSS_EM);
        return toStyleFromCSSValue<WebkitTextStrokeWidth::Length>(state, emsValue);
    };

    if (primitiveValue->isValueID()) {
        switch (primitiveValue->valueID()) {
        case CSSValueThin:
            return handleKeyword(1.0 / 48.0);
        case CSSValueMedium:
            return handleKeyword(3.0 / 48.0);
        case CSSValueThick:
            return handleKeyword(5.0 / 48.0);
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return 0_css_px;
        }
    }

    return toStyleFromCSSValue<WebkitTextStrokeWidth::Length>(state, *primitiveValue);
}

} // namespace Style
} // namespace WebCore
