/*
 * Copyright (C) 2015 Igalia S.L.
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
#include "ThemeWPE.h"

#include "Color.h"
#include "ControlStates.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "LengthSize.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

Theme& Theme::singleton()
{
    static NeverDestroyed<ThemeWPE> theme;
    return theme;
}

LengthSize ThemeWPE::controlSize(ControlPart part, const FontCascade& fontCascade, const LengthSize& zoomedSize, float zoomFactor) const
{
    if (!zoomedSize.width.isIntrinsicOrAuto() && !zoomedSize.height.isIntrinsicOrAuto())
        return Theme::controlSize(part, fontCascade, zoomedSize, zoomFactor);

    switch (part) {
    case CheckboxPart:
    case RadioPart:
        return LengthSize { Length(12, Fixed), Length(12, Fixed) };
    default:
        break;
    }

    return Theme::controlSize(part, fontCascade, zoomedSize, zoomFactor);
}

void ThemeWPE::paint(ControlPart part, ControlStates& states, GraphicsContext& context, const FloatRect& zoomedRect, float zoomFactor, ScrollView*, float, float, bool, bool)
{
    switch (part) {
    case CheckboxPart:
        paintCheckbox(states, context, zoomedRect, zoomFactor);
        break;
    case RadioPart:
        paintRadio(states, context, zoomedRect, zoomFactor);
        break;
    case PushButtonPart:
    case DefaultButtonPart:
    case ButtonPart:
    case SquareButtonPart:
        paintButton(part, states, context, zoomedRect, zoomFactor);
        break;
    default:
        break;
    }
}

void ThemeWPE::paintCheckbox(ControlStates& states, GraphicsContext& context, const FloatRect& zoomedRect, float)
{
    GraphicsContextStateSaver stateSaver(context);

    FloatSize corner(2, 2);
    FloatRoundedRect roundedRect(zoomedRect, corner, corner, corner, corner);
    Path path;
    path.addRoundedRect(roundedRect);

    context.setFillColor(makeRGB(224, 224, 224));
    context.fillPath(path);

    context.setStrokeThickness(1);
    context.setStrokeColor(makeRGB(94, 94, 94));
    context.strokePath(path);

    if (states.states() & ControlStates::CheckedState) {
        auto& rect = roundedRect.rect();

        Path checkerPath;
        checkerPath.moveTo(FloatPoint(rect.x() + 3, rect.maxY() - 3));
        checkerPath.addLineTo(FloatPoint(rect.maxX() - 3, rect.y() + 3));

        context.setStrokeThickness(2);
        context.setStrokeColor(makeRGB(84, 84, 84));
        context.strokePath(checkerPath);
    }
}

void ThemeWPE::paintRadio(ControlStates& states, GraphicsContext& context, const FloatRect& zoomedRect, float)
{
    GraphicsContextStateSaver stateSaver(context);

    Path path;
    path.addEllipse(zoomedRect);

    context.setFillColor(makeRGB(224, 224, 224));
    context.fillPath(path);

    context.setStrokeThickness(1);
    context.setStrokeColor(makeRGB(94, 94, 94));
    context.strokePath(path);

    if (states.states() & ControlStates::CheckedState) {
        FloatRect checkerRect = zoomedRect;
        checkerRect.inflate(-3);

        Path checkerPath;
        checkerPath.addEllipse(checkerRect);

        context.setFillColor(makeRGB(84, 84, 84));
        context.fillPath(checkerPath);
    }
}

void ThemeWPE::paintButton(ControlPart part, ControlStates& states, GraphicsContext& context, const FloatRect& zoomedRect, float)
{
    GraphicsContextStateSaver stateSaver(context);

    float roundness = (part == SquareButtonPart) ? 0 : 2;

    FloatSize corner(roundness, roundness);
    FloatRoundedRect roundedRect(zoomedRect, corner, corner, corner, corner);
    Path path;
    path.addRoundedRect(roundedRect);

    Color fillColor = states.states() & ControlStates::PressedState ? makeRGB(244, 244, 244) : makeRGB(224, 224, 224);
    context.setFillColor(fillColor);
    context.fillPath(path);

    context.setStrokeThickness(1);
    context.setStrokeColor(makeRGB(94, 94, 94));
    context.strokePath(path);
}

} // namespace WebCore
