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
#include "StyleTextUnderlineOffset.h"

#include "AnimationUtilities.h"
#include "RenderStyleInlines.h"
#include "StyleBuilderState.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

float TextUnderlineOffset::resolve(const RenderStyle& style, float autoValue) const
{
    return WTF::switchOn(*this,
        [&](const CSS::Keyword::Auto&) -> float {
            return autoValue;
        },
        [&](const Fixed& fixed) -> float {
            return Style::evaluate<float>(fixed, style.usedZoomForLength());
        },
        [&](const Calc& calc) -> float {
            return Style::evaluate<float>(calc, style.computedFontSize(), style.usedZoomForLength());
        },
        [&](const Percentage& percentage) -> float {
            return Style::evaluate<float>(percentage, style.computedFontSize());
        }
    );
}

// MARK: - Blending

auto Blending<TextUnderlineOffset>::canBlend(const TextUnderlineOffset& a, const TextUnderlineOffset& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
{
    if (a.isAuto() || b.isAuto())
        return false;

    return a.resolve(aStyle) != b.resolve(bStyle);
}

auto Blending<TextUnderlineOffset>::blend(const TextUnderlineOffset& a, const TextUnderlineOffset& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> TextUnderlineOffset
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? b : a;
    }

    return TextUnderlineOffset { TextUnderlineOffset::Fixed { WebCore::blend(a.resolve(aStyle), b.resolve(bStyle), context) } };
}

} // namespace Style
} // namespace WebCore
