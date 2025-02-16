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
#include "SliderTrackAdwaita.h"

#include "GraphicsContextStateSaver.h"

#if USE(THEME_ADWAITA)

namespace WebCore {
using namespace WebCore::Adwaita;

SliderTrackAdwaita::SliderTrackAdwaita(ControlPart& part, ControlFactoryAdwaita& controlFactory)
    : ControlAdwaita(part, controlFactory)
{
}

void SliderTrackAdwaita::draw(GraphicsContext& graphicsContext, const FloatRoundedRect& borderRect, float /*deviceScaleFactor*/, const ControlStyle& style)
{
    auto& sliderTrackPart = owningSliderTrackPart();
    GraphicsContextStateSaver stateSaver(graphicsContext);

    FloatRect rect = borderRect.rect();
    FloatRect fieldRect = rect;
    bool isHorizontal = sliderTrackPart.type() == StyleAppearance::SliderHorizontal;
    if (isHorizontal) {
        fieldRect.move(0, rect.height() / 2 - (sliderTrackSize / 2));
        fieldRect.setHeight(sliderTrackSize);
    } else {
        fieldRect.move(rect.width() / 2 - (sliderTrackSize / 2), 0);
        fieldRect.setWidth(sliderTrackSize);
    }

    SRGBA<uint8_t> sliderTrackBackgroundColor;

    if (style.states.contains(ControlStyle::State::DarkAppearance))
        sliderTrackBackgroundColor = sliderTrackBackgroundColorDark;
    else
        sliderTrackBackgroundColor = sliderTrackBackgroundColorLight;

    if (!style.states.contains(ControlStyle::State::Enabled))
        graphicsContext.beginTransparencyLayer(disabledOpacity);

    FloatSize corner(3, 3);
    Path path;

    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::NonZero);
    graphicsContext.setFillColor(sliderTrackBackgroundColor);
    graphicsContext.fillPath(path);
    path.clear();

    FloatRect rangeRect = fieldRect;
    FloatRoundedRect::Radii corners;
    if (isHorizontal) {
        float offset = rangeRect.width() * sliderTrackPart.thumbPosition();
        if (style.states.contains(ControlStyle::State::InlineFlippedWritingMode)) {
            rangeRect.move(rangeRect.width() - offset, 0);
            rangeRect.setWidth(offset);
            corners.setTopRight(corner);
            corners.setBottomRight(corner);
        } else {
            rangeRect.setWidth(offset);
            corners.setTopLeft(corner);
            corners.setBottomLeft(corner);
        }
    } else {
        float offset = rangeRect.height() * sliderTrackPart.thumbPosition();
        if (style.states.contains(ControlStyle::State::VerticalWritingMode)) {
            rangeRect.setHeight(offset);
            corners.setTopLeft(corner);
            corners.setTopRight(corner);
        } else {
            rangeRect.move(0, rangeRect.height() - offset);
            rangeRect.setHeight(offset);
            corners.setBottomLeft(corner);
            corners.setBottomRight(corner);
        }
    }

    path.addRoundedRect(FloatRoundedRect(rangeRect, corners));
    graphicsContext.setFillRule(WindRule::NonZero);
    graphicsContext.setFillColor(accentColor(style));
    graphicsContext.fillPath(path);

    sliderTrackPart.drawTicks(graphicsContext, borderRect.rect(), style);

    if (style.states.contains(ControlStyle::State::Focused)) {
        // Sliders support accent-color, so we want to color their focus rings too
        Color focusRingColor = accentColor(style).colorWithAlphaMultipliedBy(focusRingOpacity);
        Adwaita::paintFocus(graphicsContext, fieldRect, sliderTrackFocusOffset, focusRingColor, Adwaita::PaintRounded::Yes);
    }

    if (!style.states.contains(ControlStyle::State::Enabled))
        graphicsContext.endTransparencyLayer();
}

} // namespace WebCore

#endif // USE(THEME_ADWAITA)
