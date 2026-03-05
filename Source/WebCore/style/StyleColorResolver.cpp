/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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
#include "StyleColorResolver.h"

#include "RenderStyle+GettersInlines.h"

namespace WebCore {
namespace Style {

WebCore::Color ColorResolver::colorApplyingColorFilter(const WebCore::Color& color) const
{
    auto transformedColor = color;
    m_style->appleColorFilter().transformColor(transformedColor);
    return transformedColor;
}

WebCore::Color ColorResolver::colorResolvingCurrentColor(const Style::Color& color) const
{
    return color.resolveColor(m_style->color());
}

WebCore::Color ColorResolver::colorResolvingCurrentColorApplyingColorFilter(const Style::Color& color) const
{
    return colorApplyingColorFilter(colorResolvingCurrentColor(color));
}

WebCore::Color ColorResolver::visitedLinkColorResolvingCurrentColor(const Style::Color& color) const
{
    return color.resolveColor(m_style->visitedLinkColor());
}

WebCore::Color ColorResolver::visitedLinkColorResolvingCurrentColorApplyingColorFilter(const Style::Color& color) const
{
    return colorApplyingColorFilter(visitedLinkColorResolvingCurrentColor(color));
}

bool ColorResolver::visitedDependentShouldReturnUnvisitedLinkColor(OptionSet<PaintBehavior> paintBehavior) const
{
    if (m_style->insideLink() != InsideLink::InsideVisited)
        return true;

    if (paintBehavior.contains(PaintBehavior::DontShowVisitedLinks))
        return true;

    if (m_style->isInSubtreeWithBlendMode())
        return true;

    return false;
}

} // namespace Style
} // namespace WebCore
