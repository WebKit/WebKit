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
#include "StyleAccentColor.h"

#include "AnimationUtilities.h"

namespace WebCore {
namespace Style {

const Color& AccentColor::colorOrCurrentColor() const
{
    if (m_value)
        return *m_value;
    return Color::currentColor();
}

// MARK: - Blending

auto Blending<AccentColor>::equals(const AccentColor& a, const AccentColor& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
{
    bool aAuto = a.isAuto();
    bool bAuto = b.isAuto();

    if (aAuto || bAuto)
        return aAuto == bAuto;

    return Style::equalsForBlending(*a.tryColor(), *b.tryColor(), aStyle, bStyle);
}

auto Blending<AccentColor>::canBlend(const AccentColor& a, const AccentColor& b) -> bool
{
    return !a.isAuto() && !b.isAuto() && Style::canBlend(*a.tryColor(), *b.tryColor());
}

auto Blending<AccentColor>::blend(const AccentColor& a, const AccentColor& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> AccentColor
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    ASSERT(canBlend(a, b));
    return Style::blend(*a.tryColor(), *b.tryColor(), aStyle, bStyle, context);
}

} // namespace Style
} // namespace WebCore
