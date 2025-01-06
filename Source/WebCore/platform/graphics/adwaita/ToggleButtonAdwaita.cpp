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
#include "ToggleButtonAdwaita.h"

#if USE(THEME_ADWAITA)

#include "ColorBlending.h"
#include "GraphicsContextStateSaver.h"

namespace WebCore {
using namespace WebCore::Adwaita;

ToggleButtonAdwaita::ToggleButtonAdwaita(ControlPart& part, ControlFactoryAdwaita& controlFactory)
    : ControlAdwaita(part, controlFactory)
{
}

void ToggleButtonAdwaita::draw(GraphicsContext& graphicsContext, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    if (m_owningPart.type() == StyleAppearance::Checkbox)
        drawCheckbox(graphicsContext, borderRect, deviceScaleFactor, style);
    else
        drawRadio(graphicsContext, borderRect, deviceScaleFactor, style);
}

void ToggleButtonAdwaita::drawCheckbox(GraphicsContext& graphicsContext, const FloatRoundedRect& borderRect, float /*deviceScaleFactor*/, const ControlStyle& style)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);

    FloatRect fieldRect = borderRect.rect();
    if (fieldRect.width() != fieldRect.height()) {
        auto buttonSize = std::min(fieldRect.width(), fieldRect.height());
        fieldRect.setSize({ buttonSize, buttonSize });
        if (fieldRect.width() != borderRect.rect().width())
            fieldRect.move((borderRect.rect().width() - fieldRect.width()) / 2.0, 0);
        else
            fieldRect.move(0, (borderRect.rect().height() - fieldRect.height()) / 2.0);
    }

    SRGBA<uint8_t> toggleBorderColor;
    SRGBA<uint8_t> toggleBorderHoverColor;

    if (style.states.contains(ControlStyle::State::DarkAppearance)) {
        toggleBorderColor = toggleBorderColorDark;
        toggleBorderHoverColor = toggleBorderHoveredColorDark;
    } else {
        toggleBorderColor = toggleBorderColorLight;
        toggleBorderHoverColor = toggleBorderHoveredColorLight;
    }

    Color accentColor = this->accentColor(style);
    Color foregroundColor = accentColor.luminance() > 0.5 ? Color(SRGBA<uint8_t> { 0, 0, 0, 204 }) : Color::white;
    Color accentHoverColor = blendSourceOver(accentColor, foregroundColor.colorWithAlphaMultipliedBy(0.1));

    if (!style.states.contains(ControlStyle::State::Enabled))
        graphicsContext.beginTransparencyLayer(disabledOpacity);

    FloatSize corner(2, 2);
    Path path;

    if (style.states.containsAny({ ControlStyle::State::Checked, ControlStyle::State::Indeterminate })) {
        path.addRoundedRect(fieldRect, corner);
        graphicsContext.setFillRule(WindRule::NonZero);
        if (style.states.contains(ControlStyle::State::Hovered) && style.states.contains(ControlStyle::State::Enabled))
            graphicsContext.setFillColor(accentHoverColor);
        else
            graphicsContext.setFillColor(accentColor);
        graphicsContext.fillPath(path);
        path.clear();

        GraphicsContextStateSaver checkedStateSaver(graphicsContext);
        graphicsContext.translate(fieldRect.x(), fieldRect.y());
        graphicsContext.scale(FloatSize::narrowPrecision(fieldRect.width() / toggleSize, fieldRect.height() / toggleSize));
        if (style.states.contains(ControlStyle::State::Indeterminate))
            path.addRoundedRect(FloatRect(2, 5, 10, 4), corner);
        else {
            path.moveTo({ 2.43, 6.57 });
            path.addLineTo({ 7.5, 11.63 });
            path.addLineTo({ 14, 5 });
            path.addLineTo({ 14, 1 });
            path.addLineTo({ 7.5, 7.38 });
            path.addLineTo({ 4.56, 4.44 });
            path.closeSubpath();
        }

        graphicsContext.setFillColor(foregroundColor);
        graphicsContext.fillPath(path);
    } else {
        path.addRoundedRect(fieldRect, corner);
        if (style.states.contains(ControlStyle::State::Hovered) && style.states.contains(ControlStyle::State::Enabled))
            graphicsContext.setFillColor(toggleBorderHoverColor);
        else
            graphicsContext.setFillColor(toggleBorderColor);
        graphicsContext.fillPath(path);
        path.clear();

        fieldRect.inflate(-toggleBorderSize);
        corner.expand(-buttonBorderSize, -buttonBorderSize);
        path.addRoundedRect(fieldRect, corner);
        graphicsContext.setFillColor(foregroundColor);
        graphicsContext.fillPath(path);
    }

    if (style.states.contains(ControlStyle::State::Focused))
        Adwaita::paintFocus(graphicsContext, borderRect.rect(), toggleFocusOffset, focusColor(accentColor));

    if (!style.states.contains(ControlStyle::State::Enabled))
        graphicsContext.endTransparencyLayer();
}

void ToggleButtonAdwaita::drawRadio(GraphicsContext& graphicsContext, const FloatRoundedRect& borderRect, float /*deviceScaleFactor*/, const ControlStyle& style)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);
    FloatRect fieldRect = borderRect.rect();
    if (fieldRect.width() != fieldRect.height()) {
        auto buttonSize = std::min(fieldRect.width(), fieldRect.height());
        fieldRect.setSize({ buttonSize, buttonSize });
        if (fieldRect.width() != borderRect.rect().width())
            fieldRect.move((borderRect.rect().width() - fieldRect.width()) / 2.0, 0);
        else
            fieldRect.move(0, (borderRect.rect().height() - fieldRect.height()) / 2.0);
    }

    SRGBA<uint8_t> toggleBorderColor;
    SRGBA<uint8_t> toggleBorderHoverColor;

    if (style.states.contains(ControlStyle::State::DarkAppearance)) {
        toggleBorderColor = toggleBorderColorDark;
        toggleBorderHoverColor = toggleBorderHoveredColorDark;
    } else {
        toggleBorderColor = toggleBorderColorLight;
        toggleBorderHoverColor = toggleBorderHoveredColorLight;
    }

    Color accentColor = this->accentColor(style);
    Color foregroundColor = accentColor.luminance() > 0.5 ? Color(SRGBA<uint8_t> { 0, 0, 0, 204 }) : Color::white;
    Color accentHoverColor = blendSourceOver(accentColor, foregroundColor.colorWithAlphaMultipliedBy(0.1));

    if (!style.states.contains(ControlStyle::State::Enabled))
        graphicsContext.beginTransparencyLayer(disabledOpacity);

    Path path;

    if (style.states.containsAny({ ControlStyle::State::Checked, ControlStyle::State::Indeterminate })) {
        path.addEllipseInRect(fieldRect);
        graphicsContext.setFillRule(WindRule::NonZero);
        if (style.states.contains(ControlStyle::State::Hovered) && style.states.contains(ControlStyle::State::Enabled))
            graphicsContext.setFillColor(accentHoverColor);
        else
            graphicsContext.setFillColor(accentColor);
        graphicsContext.fillPath(path);
        path.clear();

        fieldRect.inflate(-(fieldRect.width() - fieldRect.width() * 0.70));
        path.addEllipseInRect(fieldRect);
        graphicsContext.setFillColor(foregroundColor);
        graphicsContext.fillPath(path);
    } else {
        path.addEllipseInRect(fieldRect);
        if (style.states.contains(ControlStyle::State::Hovered) && style.states.contains(ControlStyle::State::Enabled))
            graphicsContext.setFillColor(toggleBorderHoverColor);
        else
            graphicsContext.setFillColor(toggleBorderColor);
        graphicsContext.fillPath(path);
        path.clear();

        fieldRect.inflate(-toggleBorderSize);
        path.addEllipseInRect(fieldRect);
        graphicsContext.setFillColor(foregroundColor);
        graphicsContext.fillPath(path);
    }

    if (style.states.contains(ControlStyle::State::Focused))
        Adwaita::paintFocus(graphicsContext, borderRect.rect(), toggleFocusOffset, focusColor(accentColor), Adwaita::PaintRounded::Yes);

    if (!style.states.contains(ControlStyle::State::Enabled))
        graphicsContext.endTransparencyLayer();
}

} // namespace WebCore

#endif // USE(THEME_ADWAITA)
