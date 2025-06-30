/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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
#include "StyleLineWidth.h"

#include "CSSPrimitiveValue.h"
#include "CSSValuePair.h"
#include "RenderStyleInlines.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// MARK: - Conversion

auto CSSValueConversion<LineWidth>::operator()(BuilderState& state, const CSSValue& value) -> LineWidth
{
    auto* primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return CSS::Keyword::Medium { };

    switch (primitiveValue->valueID()) {
    case CSSValueThin:
        return CSS::Keyword::Thin { };
    case CSSValueMedium:
        return CSS::Keyword::Medium { };
    case CSSValueThick:
        return CSS::Keyword::Thick { };
    case CSSValueInvalid: {
        // Any original result that was >= 1 should not be allowed to fall below 1.
        // This keeps border lines from vanishing.
        auto result = primitiveValue->resolveAsLength<float>(state.cssToLengthConversionData());
        if (state.style().usedZoom() < 1.0f && result < 1.0) {
            auto originalLength = primitiveValue->resolveAsLength<float>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0));
            if (originalLength >= 1.0f)
                return 1_css_px;
        }
        auto minimumLineWidth = 1.0f / state.document().deviceScaleFactor();
        if (result > 0.0f && result < minimumLineWidth)
            return { minimumLineWidth };
        return { floorToDevicePixel(result, state.document().deviceScaleFactor()) };
    }
    default:
        ASSERT_NOT_REACHED();
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Medium { };
    }
}

} // namespace Style
} // namespace WebCore
