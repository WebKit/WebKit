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
#include "StyleMinimumSize.h"

#include "StyleBuilderConverter.h"
#include "StyleBuilderState.h"
#include "StylePreferredSize.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

MinimumSize::MinimumSize(PreferredSize&& other)
    : m_value(WTFMove(other.m_value))
{
}

MinimumSize::MinimumSize(const PreferredSize& other)
    : m_value(other.m_value)
{
}

// MARK: - Conversion

MinimumSize minimumSizeFromCSSValue(const CSSValue& value, BuilderState& state)
{
    return MinimumSize { BuilderConverter::convertLengthSizing(state, value) };
}

// MARK: - Blending

auto Blending<MinimumSize>::canBlend(const MinimumSize& a, const MinimumSize& b) -> bool
{
    return WebCore::canInterpolateLengths(a.m_value, b.m_value, true);
}

auto Blending<MinimumSize>::requiresInterpolationForAccumulativeIteration(const MinimumSize& a, const MinimumSize& b) -> bool
{
    return WebCore::lengthsRequireInterpolationForAccumulativeIteration(a.m_value, b.m_value);
}

auto Blending<MinimumSize>::blend(const MinimumSize& a, const MinimumSize& b, const BlendingContext& context) -> MinimumSize
{
    return MinimumSize { WebCore::blend(a.m_value, b.m_value, context, ValueRange::NonNegative) };
}

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream& ts, const MinimumSize& value)
{
    return ts << value.m_value;
}

} // namespace Style
} // namespace WebCore
