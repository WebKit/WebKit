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
#include "StyleOffsetAnchor.h"

#include "CSSPositionValue.h"
#include "RenderStyleInlines.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<OffsetAnchor>::operator()(BuilderState& state, const CSSValue& value) -> OffsetAnchor
{
    if (value.valueID() == CSSValueAuto)
        return OffsetAnchor { CSS::Keyword::Auto { } };

    RefPtr positionValue = requiredDowncast<CSSPositionValue>(state, value);
    if (!positionValue)
        return RenderStyle::initialOffsetAnchor();
    return OffsetAnchor { toStyle(positionValue->position(), state) };
}

// MARK: - Blending

auto Blending<OffsetAnchor>::canBlend(const OffsetAnchor& a, const OffsetAnchor& b) -> bool
{
    return WTF::holdsAlternative<Position>(a) && WTF::holdsAlternative<Position>(b);
}

auto Blending<OffsetAnchor>::requiresInterpolationForAccumulativeIteration(const OffsetAnchor& a, const OffsetAnchor& b) -> bool
{
    ASSERT(canBlend(a, b));
    return Style::requiresInterpolationForAccumulativeIteration(Position { a.value }, Position { b.value });
}

auto Blending<OffsetAnchor>::blend(const OffsetAnchor& a, const OffsetAnchor& b, const BlendingContext& context) -> OffsetAnchor
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? b : a;
    }

    ASSERT(canBlend(a, b));
    return OffsetAnchor { Style::blend(Position { a.value }, Position { b.value }, context) };
}

// MARK: - Platform

auto ToPlatform<OffsetAnchor>::operator()(const OffsetAnchor& value) -> WebCore::LengthPoint
{
    return value.value;
}

} // namespace Style
} // namespace WebCore
