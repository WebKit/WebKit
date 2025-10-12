/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "MenuListButtonMac.h"

#if PLATFORM(MAC)

#import "ColorSpaceCG.h"
#import "FloatRoundedRect.h"
#import "GraphicsContext.h"
#import "MenuListButtonPart.h"
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MenuListButtonMac);

MenuListButtonMac::MenuListButtonMac(MenuListButtonPart& owningPart, ControlFactoryMac& controlFactory)
    : ControlMac(owningPart, controlFactory)
{
}

MenuListButtonMac::~MenuListButtonMac() = default;

void MenuListButtonMac::draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float, const ControlStyle& style)
{
    auto bounds = borderRect.rect();
    bounds.contract(style.borderWidth);

    if (bounds.isEmpty())
        return;

    // SF symbols 6: chevron.up.chevron.down / semibold / 20pt.
    // FIXME: This is duplicated with RenderThemeCocoa.mm.
    Path glyphPath;
    glyphPath.moveTo({ 7.05356f, 0.0f });
    glyphPath.addBezierCurveTo({ 6.6506f, 0.0f }, { 6.31419f, 0.159764f }, { 5.98845f, 0.476187f });
    glyphPath.addLineTo({ 0.449547f, 5.79359f });
    glyphPath.addBezierCurveTo({ 0.188162f, 6.04876f }, { 0.0f, 6.37185f }, { 0.0f, 6.75753f });
    glyphPath.addBezierCurveTo({ 0.0f, 7.5262f }, { 0.619975f, 8.0783f }, { 1.31101f, 8.0783f });
    glyphPath.addBezierCurveTo({ 1.64075f, 8.0783f }, { 1.99003f, 7.97934f }, { 2.27895f, 7.69662f });
    glyphPath.addLineTo({ 7.05356f, 3.02633f });
    glyphPath.addLineTo({ 11.8215f, 7.69662f });
    glyphPath.addBezierCurveTo({ 12.1171f, 7.97268f }, { 12.4597f, 8.0783f }, { 12.7895f, 8.0783f });
    glyphPath.addBezierCurveTo({ 13.4805f, 8.0783f }, { 14.1005f, 7.5262f }, { 14.1005f, 6.75753f });
    glyphPath.addBezierCurveTo({ 14.1005f, 6.37185f }, { 13.9159f, 6.04876f }, { 13.6509f, 5.79359f });
    glyphPath.addLineTo({ 8.11201f, 0.476187f });
    glyphPath.addBezierCurveTo({ 7.78627f, 0.159764f }, { 7.44987f, 0.0f }, { 7.05356f, 0.0f });
    glyphPath.moveTo({ 7.05356f, 20.1625f });
    glyphPath.addBezierCurveTo({ 7.44987f, 20.1625f }, { 7.78627f, 19.9961f }, { 8.11201f, 19.6833f });
    glyphPath.addLineTo({ 13.6509f, 14.3659f });
    glyphPath.addBezierCurveTo({ 13.9159f, 14.1071f }, { 14.1005f, 13.7907f }, { 14.1005f, 13.3984f });
    glyphPath.addBezierCurveTo({ 14.1005f, 12.6297f }, { 13.4805f, 12.0811f }, { 12.7895f, 12.0811f });
    glyphPath.addBezierCurveTo({ 12.4597f, 12.0811f }, { 12.1171f, 12.1899f }, { 11.8215f, 12.4659f });
    glyphPath.addLineTo({ 7.05356f, 17.1362f });
    glyphPath.addLineTo({ 2.27895f, 12.4659f });
    glyphPath.addBezierCurveTo({ 1.99003f, 12.1801f }, { 1.64075f, 12.0811f }, { 1.31101f, 12.0811f });
    glyphPath.addBezierCurveTo({ 0.619975f, 12.0811f }, { 0.0f, 12.6297f }, { 0.0f, 13.3984f });
    glyphPath.addBezierCurveTo({ 0.0f, 13.7907f }, { 0.188162f, 14.1071f }, { 0.449547f, 14.3659f });
    glyphPath.addLineTo({ 5.98845f, 19.6833f });
    glyphPath.addBezierCurveTo({ 6.31419f, 19.9961f }, { 6.6506f, 20.1625f }, { 7.05356f, 20.1625f });

    constexpr float baseGlyphWidth = 14.4618f;
    constexpr float baseGlyphHeight = 20.1723f;
    const auto glyphScale = 0.55f * style.fontSize / baseGlyphWidth;
    FloatSize glyphSize { baseGlyphWidth * glyphScale, baseGlyphHeight * glyphScale };

    bool isVerticalWritingMode = style.states.contains(ControlStyle::State::VerticalWritingMode);
    auto logicalBounds = isVerticalWritingMode ? bounds.transposedRect() : bounds;

    bool isInlineFlipped = style.states.contains(ControlStyle::State::InlineFlippedWritingMode);
    float endEdge = [&] {
        static constexpr int arrowPaddingAfter = 6;
        if (isInlineFlipped)
            return logicalBounds.x() + arrowPaddingAfter * style.zoomFactor;
        return logicalBounds.maxX() - arrowPaddingAfter * style.zoomFactor - glyphSize.width();
    }();

    FloatPoint glyphOrigin { endEdge, logicalBounds.y() + (logicalBounds.height() - glyphSize.height()) / 2 };

    if (isVerticalWritingMode)
        glyphOrigin = glyphOrigin.transposedPoint();

    AffineTransform transform;
    transform.translate(glyphOrigin);
    transform.scale(glyphScale);
    glyphPath.transform(transform);

    GraphicsContextStateSaver stateSaver(context);
    context.setFillColor(style.textColor);
    context.setStrokeStyle(StrokeStyle::NoStroke);
    context.fillPath(glyphPath);
}

} // namespace WebCore

#endif // PLATFORM(MAC)
