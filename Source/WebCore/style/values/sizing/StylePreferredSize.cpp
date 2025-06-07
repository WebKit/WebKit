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
#include "StylePreferredSize.h"

#include "StyleBuilderConverter.h"
#include "StyleBuilderState.h"
#include "StyleFlexBasis.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

FlexBasis PreferredSize::asFlexBasis() const
{
    return FlexBasis { m_value };
}

// MARK: - Conversion

PreferredSize preferredSizeFromCSSValue(const CSSValue& value, BuilderState& state)
{
    return PreferredSize { BuilderConverter::convertLengthSizing(state, value) };
}

// MARK: - Blending

auto Blending<PreferredSize>::canBlend(const PreferredSize& a, const PreferredSize& b) -> bool
{
    return WebCore::canInterpolateLengths(a.m_value, b.m_value, true);
}

auto Blending<PreferredSize>::requiresInterpolationForAccumulativeIteration(const PreferredSize& a, const PreferredSize& b) -> bool
{
    return WebCore::lengthsRequireInterpolationForAccumulativeIteration(a.m_value, b.m_value);
}

auto Blending<PreferredSize>::blend(const PreferredSize& a, const PreferredSize& b, const BlendingContext& context) -> PreferredSize
{
    return PreferredSize { WebCore::blend(a.m_value, b.m_value, context, ValueRange::NonNegative) };
}

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream& ts, const PreferredSize& value)
{
    return ts << value.m_value;
}

} // namespace Style
} // namespace WebCore
