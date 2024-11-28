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
#import "ToggleButtonMac.h"

#if PLATFORM(MAC)

#import "ControlFactoryMac.h"
#import "FloatRoundedRect.h"
#import "GraphicsContext.h"
#import "LocalDefaultSystemAppearance.h"
#import "ToggleButtonPart.h"
#import <pal/spi/cocoa/NSButtonCellSPI.h>
#import <wtf/TZoneMallocInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ToggleButtonMac);

ToggleButtonMac::ToggleButtonMac(ToggleButtonPart& owningPart, ControlFactoryMac& controlFactory, NSButtonCell *buttonCell)
    : ButtonControlMac(owningPart, controlFactory, buttonCell)
{
    ASSERT(m_owningPart.type() == StyleAppearance::Checkbox || m_owningPart.type() == StyleAppearance::Radio);
}

IntSize ToggleButtonMac::cellSize(NSControlSize controlSize, const ControlStyle& style) const
{
    static const std::array<IntSize, 4> checkboxSizes =
    {
        IntSize { 14, 14 },
        IntSize { 12, 12 },
        IntSize { 10, 10 },
        IntSize { 16, 16 }
    };
    static const std::array<IntSize, 4> radioSizes =
    {
        IntSize { 16, 16 },
        IntSize { 12, 12 },
        IntSize { 10, 10 },
        IntSize { 0, 0 }
    };
    static const std::array<IntSize, 4> largeRadioSizes =
    {
        IntSize { 14, 14 },
        IntSize { 12, 12 },
        IntSize { 10, 10 },
        IntSize { 16, 16 }
    };

    if (m_owningPart.type() == StyleAppearance::Checkbox)
        return checkboxSizes[controlSize];

    if (style.states.contains(ControlStyle::State::LargeControls))
        return largeRadioSizes[controlSize];

    return radioSizes[controlSize];
}

IntOutsets ToggleButtonMac::cellOutsets(NSControlSize controlSize, const ControlStyle&) const
{
    static const IntOutsets checkboxOutsets[] = {
        // top right bottom left
        { 2, 2, 2, 2 },
        { 2, 1, 2, 1 },
        { 0, 0, 1, 0 },
        { 2, 2, 2, 2 },
    };
    static const IntOutsets radioOutsets[] = {
        // top right bottom left
        { 1, 0, 1, 2 },
        { 1, 1, 2, 1 },
        { 0, 0, 1, 1 },
        { 1, 0, 1, 2 },
    };
    return (m_owningPart.type() == StyleAppearance::Checkbox ? checkboxOutsets : radioOutsets)[controlSize];
}

FloatRect ToggleButtonMac::rectForBounds(const FloatRect& bounds, const ControlStyle& style) const
{
    auto controlSize = [m_buttonCell controlSize];

    FloatSize size = cellSize(controlSize, style);
    size.scale(style.zoomFactor);

    auto outsets = cellOutsets(controlSize, style);

    return inflatedRect(bounds, size, outsets, style);
}

void ToggleButtonMac::draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);

    GraphicsContextStateSaver stateSaver(context);

    auto logicalRect = rectForBounds(borderRect.rect(), style);

    if (style.zoomFactor != 1) {
        logicalRect.scale(1 / style.zoomFactor);
        context.scale(style.zoomFactor);
    }

    if ([m_buttonCell _stateAnimationRunning]) {
        context.translate(logicalRect.location());
        context.scale(FloatSize(1, -1));
        context.translate(0, -logicalRect.height());

        [m_buttonCell _renderCurrentAnimationFrameInContext:context.platformContext() atLocation:NSMakePoint(0, 0)];

        if (![m_buttonCell _stateAnimationRunning] && style.states.contains(ControlStyle::State::Focused))
            drawCell(context, logicalRect, deviceScaleFactor, style, m_buttonCell.get(), false);
    } else
        drawCell(context, logicalRect, deviceScaleFactor, style, m_buttonCell.get(), true);
}

} // namespace WebCore

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // PLATFORM(MAC)
