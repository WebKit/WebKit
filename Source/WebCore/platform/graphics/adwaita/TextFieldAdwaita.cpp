/*
 * Copyright (C) 2015, 2020 Igalia S.L.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Sony Interactive Entertainment Inc.
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
#include "TextFieldAdwaita.h"

#include "GraphicsContextStateSaver.h"

#if USE(THEME_ADWAITA)

namespace WebCore {
using namespace WebCore::Adwaita;

TextFieldAdwaita::TextFieldAdwaita(ControlPart& part, ControlFactoryAdwaita& controlFactory)
    : ControlAdwaita(part, controlFactory)
{
}

void TextFieldAdwaita::draw(GraphicsContext& graphicsContext, const FloatRoundedRect& borderRect, float /*deviceScaleFactor*/, const ControlStyle& style)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);

    SRGBA<uint8_t> textFieldBackgroundColor;
    SRGBA<uint8_t> textFieldBorderColor;

    if (style.states.contains(ControlStyle::State::DarkAppearance)) {
        textFieldBackgroundColor = textFieldBackgroundColorDark;
        textFieldBorderColor= textFieldBorderColorDark;
    } else {
        textFieldBackgroundColor = textFieldBackgroundColorLight;
        textFieldBorderColor = textFieldBorderColorLight;
    }

    bool enabled = style.states.contains(ControlStyle::State::Enabled) && !style.states.contains(ControlStyle::State::ReadOnly);
    int borderSize = textFieldBorderSize;
    if (style.states.contains(ControlStyle::State::Focused))
        borderSize *= 2;

    if (!enabled)
        graphicsContext.beginTransparencyLayer(disabledOpacity);

    FloatRect fieldRect = borderRect.rect();
    FloatSize corner(5, 5);
    Path path;
    path.addRoundedRect(fieldRect, corner);
    fieldRect.inflate(-borderSize);
    corner.expand(-borderSize, -borderSize);
    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::EvenOdd);
    if (enabled && style.states.contains(ControlStyle::State::Focused))
        graphicsContext.setFillColor(systemFocusRingColor());
    else
        graphicsContext.setFillColor(textFieldBorderColor);
    graphicsContext.fillPath(path);
    path.clear();

    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::NonZero);
    graphicsContext.setFillColor(textFieldBackgroundColor);
    graphicsContext.fillPath(path);

    if (style.states.contains(ControlStyle::State::ListButton)) {
        auto zoomedArrowSize = menuListButtonArrowSize * style.zoomFactor;
        FloatRect arrowRect = borderRect.rect();
        if (style.states.contains(ControlStyle::State::InlineFlippedWritingMode))
            arrowRect.move(textFieldBorderSize * 2, 0);
        else
            arrowRect.move(arrowRect.width() - (zoomedArrowSize + textFieldBorderSize * 2), 0);
        arrowRect.setWidth(zoomedArrowSize);
        bool useDarkAppearance = style.states.contains(ControlStyle::State::DarkAppearance);
        Adwaita::paintArrow(graphicsContext, arrowRect, Adwaita::ArrowDirection::Down, useDarkAppearance);
    }

    if (!enabled)
        graphicsContext.endTransparencyLayer();
}

} // namespace WebCore

#endif // USE(THEME_ADWAITA)
