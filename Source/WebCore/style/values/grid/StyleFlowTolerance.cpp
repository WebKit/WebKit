/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "StyleFlowTolerance.h"

#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// MARK: - Conversion

auto CSSValueConversion<FlowTolerance>::operator()(BuilderState& state, const CSSValue& value) -> FlowTolerance
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueNormal:
            return CSS::Keyword::Normal { };
        case CSSValueInfinite:
            return CSS::Keyword::Infinite { };
        default:
            break;
        }

        // If not a keyword, it must be a <length-percentage [0,∞]>
        return toStyleFromCSSValue<LengthPercentage<CSS::NonnegativeUnzoomed>>(state, *primitiveValue);
    }

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return CSS::Keyword::Normal { };
}

// MARK: - Blending

auto Blending<FlowTolerance>::canBlend(const FlowTolerance& a, const FlowTolerance& b) -> bool
{
    // Can only blend if both are length-percentage values
    return !a.isNormal() && !a.isInfinite() && !b.isNormal() && !b.isInfinite();
}

auto Blending<FlowTolerance>::blend(const FlowTolerance& a, const FlowTolerance& b, const BlendingContext& context) -> FlowTolerance
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    ASSERT(!a.isNormal());
    ASSERT(!a.isInfinite());
    ASSERT(!b.isNormal());
    ASSERT(!b.isInfinite());

    // Both must be length-percentage values
    return a.switchOn(
        [&](const LengthPercentage<CSS::NonnegativeUnzoomed>& aValue) {
            return b.switchOn(
                [&](const LengthPercentage<CSS::NonnegativeUnzoomed>& bValue) -> FlowTolerance {
                    return WebCore::Style::blend(aValue, bValue, context);
                },
                [&](auto) -> FlowTolerance {
                    ASSERT_NOT_REACHED();
                    return a;
                }
            );
        },
        [&](auto) -> FlowTolerance {
            ASSERT_NOT_REACHED();
            return a;
        }
    );
}

} // namespace Style
} // namespace WebCore
