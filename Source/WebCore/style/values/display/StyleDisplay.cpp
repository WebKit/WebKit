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
#include "StyleDisplay.h"

#include "AnimationUtilities.h"

namespace WebCore {
namespace Style {

// MARK: - Blending

auto Blending<Display>::blend(const Display& a, const Display& b, const BlendingContext& context) -> Display
{
    // In general, the display property's animation type is discrete. However, similar to interpolation of
    // visibility, during interpolation between none and any other display value, p values between 0 and 1
    // map to the non-none value. Additionally, the element is inert as long as its display value would
    // compute to none when ignoring the Transitions and Animations cascade origins.
    // https://drafts.csswg.org/css-display/#display-animation

    if (a != Display::None && b != Display::None)
        return context.progress < 0.5 ? a : b;
    if (context.progress <= 0)
        return a;
    if (context.progress >= 1)
        return b;
    return a == Style::Display::None ? b : a;
}

} // namespace Style
} // namespace WebCore
