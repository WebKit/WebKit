/*
 * Copyright (C) 2022 Apple Inc. All Rights Reserved.
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
#import "ColorWellMac.h"

#if PLATFORM(MAC) && ENABLE(INPUT_TYPE_COLOR)

#import "ColorWellPart.h"
#import "ControlFactoryMac.h"
#import "LocalCurrentGraphicsContext.h"
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

ColorWellMac::ColorWellMac(ColorWellPart& owningPart, ControlFactoryMac& controlFactory, NSButtonCell *buttonCell)
    : ButtonControlMac(owningPart, controlFactory, buttonCell)
{
}

void ColorWellMac::updateCellStates(const FloatRect& rect, const ControlStyle& style)
{
    ButtonControlMac::updateCellStates(rect, style);
    [m_buttonCell setBezelStyle:NSBezelStyleTexturedSquare];
}

void ColorWellMac::draw(GraphicsContext& context, const FloatRect& rect, float deviceScaleFactor, const ControlStyle& style)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);

    GraphicsContextStateSaver stateSaver(context);
    LocalCurrentGraphicsContext localContext(context);

    auto *view = m_controlFactory.drawingView(rect, style);
    auto *window = [view window];
    auto *previousDefaultButtonCell = [window defaultButtonCell];

    // The ColorWell can't be the window default button cell.
    if ([previousDefaultButtonCell isEqual:m_buttonCell.get()])
        [window setDefaultButtonCell:nil];

    drawCell(context, rect, deviceScaleFactor, style, m_buttonCell.get(), view, true);

    // Restore the window default button cell.
    if (![previousDefaultButtonCell isEqual:m_buttonCell.get()])
        [window setDefaultButtonCell:previousDefaultButtonCell];

    END_BLOCK_OBJC_EXCEPTIONS
}

} // namespace WebCore

#endif // PLATFORM(MAC) && ENABLE(INPUT_TYPE_COLOR)
