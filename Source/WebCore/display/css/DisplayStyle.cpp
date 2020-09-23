/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DisplayStyle.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "BorderData.h"
#include "RenderStyle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Display {

Style::Style(const RenderStyle& style)
{
    m_backgroundColor = style.visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);

    const auto& borderData = style.border();
    
    auto borderValueWithResolvedColor = [&style](const BorderValue& value, CSSPropertyID colorPropertyID) {
        auto resolvedValue = value;
        resolvedValue.setColor(style.visitedDependentColorWithColorFilter(colorPropertyID));
        return resolvedValue;
    };
    
    m_border.left = borderValueWithResolvedColor(borderData.left(), CSSPropertyBorderLeftColor);
    m_border.right = borderValueWithResolvedColor(borderData.right(), CSSPropertyBorderRightColor);
    m_border.top = borderValueWithResolvedColor(borderData.top(), CSSPropertyBorderTopColor);
    m_border.bottom = borderValueWithResolvedColor(borderData.bottom(), CSSPropertyBorderBottomColor);

    m_border.image = borderData.image();

    if (!style.hasAutoUsedZIndex())
        m_zIndex = style.usedZIndex();

    setIsPositioned(style.position() != PositionType::Static);
    setIsFloating(style.floating() != Float::No);
}

bool Style::hasBackground() const
{
    return m_backgroundColor.isVisible() || hasBackgroundImage();
}

bool Style::hasVisibleBorder() const
{
    bool haveImage = m_border.image.hasImage();
    return m_border.left.isVisible(!haveImage) || m_border.right.isVisible(!haveImage) || m_border.top.isVisible(!haveImage) || m_border.bottom.isVisible(!haveImage);
}

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
