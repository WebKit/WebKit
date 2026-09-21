/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/CSSKeywordColor.h>
#include <WebCore/CSSValueKeywords.h>
#include <WebCore/Color.h>
#include <wtf/Forward.h>

namespace WebCore {

namespace CSS {
struct SerializationContext;
}

namespace Style {

class ResolvedColors;

// This is not named `AccentColor`, as `Style::AccentColor` already exists.
struct CurrentAccentColor {
    constexpr bool operator==(const CurrentAccentColor&) const = default;
};

inline WebCore::Color resolveColor(const CurrentAccentColor&, const ResolvedColors&)
{
    // Get the default accent color, which is a constant regardless of StyleColorOptions.
    // Hence the StyleColorOptions can be a default empty one.
    return CSS::colorFromKeyword(CSSValueAccentcolor, { });
}

constexpr bool containsCurrentColor(const CurrentAccentColor&)
{
    // FIXME: will be true once we incorporate the current accent color.
    return false;
}

void serializationForCSSTokenization(StringBuilder&, const CSS::SerializationContext&, const CurrentAccentColor&);
WTF::String serializationForCSSTokenization(const CSS::SerializationContext&, const CurrentAccentColor&);

WTF::TextStream& operator<<(WTF::TextStream&, const CurrentAccentColor&);

} // namespace Style
} // namespace WebCore
