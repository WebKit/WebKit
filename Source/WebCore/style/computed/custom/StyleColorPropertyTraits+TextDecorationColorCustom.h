/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/StyleComputedStyleProperties.h>

namespace WebCore {
namespace Style {

inline WebCore::Color ColorPropertyTraits<PropertyNameConstant<CSSPropertyTextDecorationColor>>::colorResolvingCurrentColor(const ComputedStyleProperties& style)
{
    auto& result = style.textDecorationColor();
    if (result.isCurrentColor()) {
        if ((style.hasExplicitlySetStrokeWidth() && style.strokeWidth().isPossiblyPositive()) || style.textStrokeWidth().isPositive()) {
            // Prefer stroke color if possible but not if it's fully transparent.
            if (style.hasExplicitlySetStrokeColor()) {
                auto strokeColor = style.strokeColor().resolveColor(style.color());
                if (strokeColor.isVisible())
                    return strokeColor;
            } else {
                auto strokeColor = style.textStrokeColor().resolveColor(style.color());
                if (strokeColor.isVisible())
                    return strokeColor;
            }
        }
        return style.color();
    }
    return result.resolveColor(style.color());
}

inline WebCore::Color ColorPropertyTraits<PropertyNameConstant<CSSPropertyTextDecorationColor>>::visitedLinkColorResolvingCurrentColor(const ComputedStyleProperties& style)
{
    auto& result = style.visitedLinkTextDecorationColor();
    if (result.isCurrentColor()) {
        if ((style.hasExplicitlySetStrokeWidth() && style.strokeWidth().isPossiblyPositive()) || style.textStrokeWidth().isPositive()) {
            // Prefer stroke color if possible but not if it's fully transparent.
            if (style.hasExplicitlySetStrokeColor()) {
                auto strokeColor = style.visitedLinkStrokeColor().resolveColor(style.visitedLinkColor());
                if (strokeColor.isVisible())
                    return strokeColor;
            } else {
                auto strokeColor = style.visitedLinkTextStrokeColor().resolveColor(style.visitedLinkColor());
                if (strokeColor.isVisible())
                    return strokeColor;
            }
        }
        return style.visitedLinkColor();
    }
    return result.resolveColor(style.visitedLinkColor());
}

} // namespace Style
} // namespace WebCore
