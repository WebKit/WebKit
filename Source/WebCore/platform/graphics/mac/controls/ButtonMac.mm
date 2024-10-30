/*
 * Copyright (C) 2022-2023 Apple Inc. All Rights Reserved.
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
#import "ButtonMac.h"

#if PLATFORM(MAC)

#import "ButtonPart.h"
#import "ControlFactoryMac.h"
#import "GraphicsContext.h"
#import "LocalDefaultSystemAppearance.h"
#import <wtf/TZoneMallocInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ButtonMac);

ButtonMac::ButtonMac(ButtonPart& owningPart, ControlFactoryMac& controlFactory, NSButtonCell *buttonCell)
    : ButtonControlMac(owningPart, controlFactory, buttonCell)
{
    ASSERT(m_owningPart.type() == StyleAppearance::Button
        || m_owningPart.type() == StyleAppearance::DefaultButton
        || m_owningPart.type() == StyleAppearance::PushButton
        || m_owningPart.type() == StyleAppearance::SquareButton);
}

IntSize ButtonMac::cellSize(NSControlSize controlSize, const ControlStyle&) const
{
    // Buttons really only constrain height. They respect width.
    static const IntSize cellSizes[] = {
        { 0, 20 },
        { 0, 16 },
        { 0, 13 },
        { 0, 28 }
    };
    return cellSizes[controlSize];
}

IntOutsets ButtonMac::cellOutsets(NSControlSize controlSize, const ControlStyle&) const
{
    // FIXME: Determine these values programmatically.
    // https://bugs.webkit.org/show_bug.cgi?id=251066
    static const IntOutsets cellOutsets[] = {
        // top right bottom left
        { 5, 7, 7, 7 },
        { 4, 6, 7, 6 },
        { 1, 2, 2, 2 },
        { 6, 6, 6, 6 },
    };
    return cellOutsets[controlSize];
}

NSBezelStyle ButtonMac::bezelStyle(const FloatRect& rect, const ControlStyle& style) const
{
    if (m_owningPart.type() == StyleAppearance::SquareButton)
        return NSBezelStyleShadowlessSquare;

    auto controlSize = style.states.contains(ControlStyle::State::LargeControls) ? NSControlSizeLarge : NSControlSizeRegular;
    auto size = cellSize(controlSize, style);

    if (rect.height() > size.height() * style.zoomFactor)
        return NSBezelStyleShadowlessSquare;

    return NSBezelStyleRounded;
}

void ButtonMac::updateCellStates(const FloatRect& rect, const ControlStyle& style)
{
    ButtonControlMac::updateCellStates(rect, style);
    [m_buttonCell setBezelStyle:bezelStyle(rect, style)];
}

FloatRect ButtonMac::rectForBounds(const FloatRect& bounds, const ControlStyle& style) const
{
    if ([m_buttonCell bezelStyle] != NSBezelStyleRounded)
        return bounds;

    auto controlSize = [m_buttonCell controlSize];

    // Explicitly use `FloatSize` to support non-integral sizes following zoom.
    FloatSize size = cellSize(controlSize, style);
    size.scale(style.zoomFactor);
    size.setWidth(bounds.width());

    auto rect = bounds;
    auto delta = rect.height() - size.height();
    if (delta > 0)
        rect.inflateY(-delta / 2);

    auto outsets = cellOutsets(controlSize, style);
    return inflatedRect(rect, size, outsets, style);
}

void ButtonMac::draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);

    GraphicsContextStateSaver stateSaver(context);

    auto inflatedRect = rectForBounds(borderRect.rect(), style);

    if (style.zoomFactor != 1) {
        inflatedRect.scale(1 / style.zoomFactor);
        context.scale(style.zoomFactor);
    }

    drawCell(context, inflatedRect, deviceScaleFactor, style, m_buttonCell.get(), true);
}

} // namespace WebCore

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // PLATFORM(MAC)
