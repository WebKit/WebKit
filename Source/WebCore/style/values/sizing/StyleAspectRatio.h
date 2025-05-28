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

#pragma once

#include "RenderStyleConstants.h"
#include "StyleRatio.h"

namespace WebCore {
namespace Style {

// <'aspect-ratio'> = auto || <ratio>
// https://drafts.csswg.org/css-sizing-4/#propdef-aspect-ratio
struct AspectRatio {
    AspectRatioType type;
    Style::Ratio ratio;

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        switch (type) {
        case AspectRatioType::Auto:
            return functor(CSS::Keyword::Auto { });
        case AspectRatioType::AutoZero:
        case AspectRatioType::Ratio:
            return functor(ratio);
        case AspectRatioType::AutoAndRatio:
            return functor(SpaceSeparatedTuple { CSS::Keyword::Auto { }, ratio });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const AspectRatio&) const = default;
};

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::AspectRatio> = true;
