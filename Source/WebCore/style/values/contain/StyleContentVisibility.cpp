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
#include "StyleContentVisibility.h"

#include "AnimationUtilities.h"

namespace WebCore {
namespace Style {

// MARK: - Blending

auto Blending<ContentVisibility>::blend(const ContentVisibility& a, const ContentVisibility& b, const BlendingContext& context) -> ContentVisibility
{
    // In general, the content-visibility property's animation type is discrete. However, similar to interpolation of
    // visibility, during interpolation between hidden and any other content-visibility value, p values between 0 and 1
    // map to the non-hidden value.
    // https://drafts.csswg.org/css-contain/#content-visibility-animation

    if (a != ContentVisibility::Hidden && b != ContentVisibility::Hidden)
        return context.progress < 0.5 ? a : b;
    if (context.progress <= 0)
        return a;
    if (context.progress >= 1)
        return b;
    return a == ContentVisibility::Hidden ? b : a;
}

} // namespace Style
} // namespace WebCore
