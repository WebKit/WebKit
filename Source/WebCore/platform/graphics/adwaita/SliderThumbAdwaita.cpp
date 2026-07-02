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
#include "SliderThumbAdwaita.h"

#include "GraphicsContextStateSaver.h"

#if USE(THEME_ADWAITA)

namespace WebCore {
using namespace WebCore::Adwaita;

SliderThumbAdwaita::SliderThumbAdwaita(ControlPart& part, ControlFactoryAdwaita& controlFactory)
    : ControlAdwaita(part, controlFactory)
{
}

void SliderThumbAdwaita::draw(GraphicsContext& graphicsContext, const FloatRoundedRect& borderRect, float /*deviceScaleFactor*/, const ControlStyle& style)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);

    SRGBA<uint8_t> sliderThumbBackgroundColor;
    SRGBA<uint8_t> sliderThumbBackgroundHoveredColor;
    SRGBA<uint8_t> sliderThumbBackgroundDisabledColor;
    SRGBA<uint8_t> sliderThumbBorderColor;

    if (style.states.contains(ControlStyle::State::DarkAppearance)) {
        sliderThumbBackgroundColor = sliderThumbBackgroundColorDark;
        sliderThumbBackgroundHoveredColor = sliderThumbBackgroundHoveredColorDark;
        sliderThumbBackgroundDisabledColor = sliderThumbBackgroundDisabledColorDark;
        sliderThumbBorderColor = sliderThumbBorderColorDark;
    } else {
        sliderThumbBackgroundColor = sliderThumbBackgroundColorLight;
        sliderThumbBackgroundHoveredColor = sliderThumbBackgroundHoveredColorLight;
        sliderThumbBackgroundDisabledColor = sliderThumbBackgroundDisabledColorLight;
        sliderThumbBorderColor = sliderThumbBorderColorLight;
    }

    FloatRect fieldRect = borderRect.rect();
    Path path;
    path.addEllipseInRect(fieldRect);
    fieldRect.inflate(-sliderThumbBorderSize);
    path.addEllipseInRect(fieldRect);
    graphicsContext.setFillRule(WindRule::EvenOdd);
    if (style.states.contains(ControlStyle::State::Enabled) && style.states.contains(ControlStyle::State::Pressed))
        graphicsContext.setFillColor(accentColor(style));
    else
        graphicsContext.setFillColor(sliderThumbBorderColor);
    graphicsContext.fillPath(path);
    path.clear();

    path.addEllipseInRect(fieldRect);
    graphicsContext.setFillRule(WindRule::NonZero);
    if (!style.states.contains(ControlStyle::State::Enabled))
        graphicsContext.setFillColor(sliderThumbBackgroundDisabledColor);
    else if (style.states.contains(ControlStyle::State::Hovered))
        graphicsContext.setFillColor(sliderThumbBackgroundHoveredColor);
    else
        graphicsContext.setFillColor(sliderThumbBackgroundColor);
    graphicsContext.fillPath(path);
}

} // namespace WebCore

#endif // USE(THEME_ADWAITA)
