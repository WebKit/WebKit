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

#include "config.h"
#include "StyleResolvedColors.h"

namespace WebCore {
namespace Style {

ResolvedColors::ResolvedColors(WebCore::Color currentColor, WebCore::Color accentColor)
    : m_currentColor(currentColor)
    , m_accentColor(accentColor)
{
}

ResolvedColors ResolvedColors::fromStyle(const ComputedStyleProperties& style)
{
    auto currentColor = style.color();
    WebCore::Color resolvedAccentColor = [&] () {
        auto accentColorOrDefault = style.accentColor().colorOrDefaultColor();

        // We're sure that style.accentColor() won't have any AccentColor, since those got
        // replaced with the parent's accent color during style building. So just give a null
        // color for AccentColor, it doesn't matter since we won't need to use it.
        return resolveColor(accentColorOrDefault, ResolvedColors { currentColor, { } });
    }();

    return ResolvedColors(style.color(), resolvedAccentColor);
}

ResolvedColors ResolvedColors::fromVisitedLinkStyle(const ComputedStyleProperties& style)
{
    auto currentColor = style.color();
    WebCore::Color resolvedAccentColor = [&] () {
        auto accentColorOrDefault = style.accentColor().colorOrDefaultColor();
        return resolveColor(accentColorOrDefault, ResolvedColors { currentColor, { } });
    }();

    return ResolvedColors(style.visitedLinkColorResolvingCurrentColor(), resolvedAccentColor);
}

} // namespace Style
} // namespace WebCore
