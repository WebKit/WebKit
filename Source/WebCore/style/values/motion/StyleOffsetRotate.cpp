/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "StyleOffsetRotate.h"

#include "CSSKeywordValue.h"
#include "CSSOffsetRotateValue.h"
#include "StyleBuilderState.h"
#include "StyleKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<OffsetRotate>::operator()(BuilderState& state, const CSSValue& value) -> OffsetRotate
{
    using namespace CSS::Literals;

    if (auto* offsetRotateValue = dynamicDowncast<CSSOffsetRotateValue>(value)) {
        OffsetRotate::Angle angle = 0_css_deg;

        if (auto* angleValue = offsetRotateValue->angle())
            angle = toStyleFromCSSValue<OffsetRotate::Angle>(state, *angleValue);

        if (auto* modifierValue = offsetRotateValue->modifier()) {
            switch (modifierValue->valueID()) {
            case CSSValueAuto:
                return OffsetRotate { CSS::Keyword::Auto { }, angle };
            case CSSValueReverse:
                angle.value += 180.0f;
                return OffsetRotate { CSS::Keyword::Auto { }, angle };
            default:
                state.setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Auto { };
            }
        }

        return OffsetRotate { std::nullopt, angle };
    }

    // Values coming from CSSTypedOM didn't go through the parser and may not have been converted to a CSSOffsetRotateValue.
    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueAuto:
            return OffsetRotate { CSS::Keyword::Auto { }, 0_css_deg };
        case CSSValueReverse:
            return OffsetRotate { CSS::Keyword::Auto { }, 180_css_deg };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Auto { };
        }
    }
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return OffsetRotate { std::nullopt, toStyleFromCSSValue<OffsetRotate::Angle>(state, *primitiveValue) };

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return CSS::Keyword::Auto { };
}

// MARK: - Blending

bool Blending<OffsetRotate>::canBlend(const OffsetRotate& from, const OffsetRotate& to)
{
    return from.hasAuto() == to.hasAuto();
}

auto Blending<OffsetRotate>::blend(const OffsetRotate& from, const OffsetRotate& to, const BlendingContext& context) -> OffsetRotate
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : from;
    }

    ASSERT(canBlend(from, to));
    return OffsetRotate { from.autoKeyword(), WebCore::Style::blend(from.angle(), to.angle(), context) };
}

// MARK: - Evaluation

#if ENABLE(THREADED_ANIMATIONS)

auto Evaluation<OffsetRotate, AcceleratedEffectOffsetRotate>::operator()(const OffsetRotate& value) -> AcceleratedEffectOffsetRotate
{
    return { .hasAuto = value.hasAuto(), .angle = value.angle().value };
}

#endif

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream& ts, const OffsetRotate& rotate)
{
    ts.dumpProperty("angle"_s, rotate.angle());
    ts.dumpProperty("hasAuto"_s, rotate.hasAuto());
    return ts;
}

} // namespace Style
} // namespace WebCore
