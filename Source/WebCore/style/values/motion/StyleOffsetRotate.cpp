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

#include "CSSOffsetRotateValue.h"
#include "StyleBuilderState.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// MARK: - Conversion

auto CSSValueConversion<OffsetRotate>::operator()(BuilderState& state, const CSSValue& value) -> OffsetRotate
{
    RefPtr<const CSSPrimitiveValue> modifierValue;
    RefPtr<const CSSPrimitiveValue> angleValue;

    if (auto* offsetRotateValue = dynamicDowncast<CSSOffsetRotateValue>(value)) {
        modifierValue = offsetRotateValue->modifier();
        angleValue = offsetRotateValue->angle();
    } else if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        // Values coming from CSSTypedOM didn't go through the parser and may not have been converted to a CSSOffsetRotateValue.
        if (primitiveValue->valueID() == CSSValueAuto || primitiveValue->valueID() == CSSValueReverse)
            modifierValue = primitiveValue;
        else if (primitiveValue->isAngle())
            angleValue = primitiveValue;
    }

    std::optional<CSS::Keyword::Auto> autoKeyword;
    auto angleInDegrees = 0.0f;

    if (angleValue)
        angleInDegrees = angleValue->resolveAsAngle<float>(state.cssToLengthConversionData());

    if (modifierValue) {
        switch (modifierValue->valueID()) {
        case CSSValueAuto:
            autoKeyword = CSS::Keyword::Auto { };
            break;
        case CSSValueReverse:
            autoKeyword = CSS::Keyword::Auto { };
            angleInDegrees += 180.0f;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    return OffsetRotate { autoKeyword, OffsetRotate::Angle { angleInDegrees } };
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

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

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
