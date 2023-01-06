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
#import "SearchFieldCancelButtonMac.h"

#if PLATFORM(MAC)

#import "ControlFactoryMac.h"
#import "GraphicsContext.h"
#import "LocalCurrentGraphicsContext.h"
#import "LocalDefaultSystemAppearance.h"
#import "SearchFieldCancelButtonPart.h"

namespace WebCore {

SearchFieldCancelButtonMac::SearchFieldCancelButtonMac(SearchFieldCancelButtonPart& owningPart, ControlFactoryMac& controlFactory, NSSearchFieldCell *searchFieldCell)
    : SearchControlMac(owningPart, controlFactory, searchFieldCell)
{
    ASSERT(searchFieldCell);
}

IntSize SearchFieldCancelButtonMac::cellSize(NSControlSize controlSize, const ControlStyle&) const
{
    static const IntSize sizes[] = {
        { 22, 22 },
        { 19, 19 },
        { 15, 15 },
        { 22, 22 }
    };
    return sizes[controlSize];
}

FloatRect SearchFieldCancelButtonMac::rectForBounds(const FloatRect& bounds, const ControlStyle& style) const
{
    auto sizeBasedOnFontSize = sizeForSystemFont(style);
    auto diff = bounds.size() - FloatSize(sizeBasedOnFontSize);
    if (diff.isZero())
        return bounds;

    // Vertically centered and right aligned.
    auto location = bounds.location() + FloatSize { diff.width(), diff.height() / 2 };
    return { location, sizeBasedOnFontSize };
}

void SearchFieldCancelButtonMac::updateCellStates(const FloatRect& rect, const ControlStyle& style)
{
    bool enabled = style.states.contains(ControlStyle::State::Enabled);
    bool readOnly = style.states.contains(ControlStyle::State::ReadOnly);

    if (!enabled && !readOnly)
        updatePressedState([m_searchFieldCell cancelButtonCell], style);
    else if ([[m_searchFieldCell cancelButtonCell] isHighlighted])
        [[m_searchFieldCell cancelButtonCell] setHighlighted:NO];

    SearchControlMac::updateCellStates(rect, style);
}

void SearchFieldCancelButtonMac::draw(GraphicsContext& context, const FloatRect& rect, float deviceScaleFactor, const ControlStyle& style)
{
    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);

    LocalCurrentGraphicsContext localContext(context);

    GraphicsContextStateSaver stateSaver(context);

    auto logicalRect = rectForBounds(rect, style);
    if (style.zoomFactor != 1) {
        logicalRect.scale(1 / style.zoomFactor);
        context.scale(style.zoomFactor);
    }

    // Never draw a focus ring for the cancel button.
    auto styleForDrawing = style;
    styleForDrawing.states.remove(ControlStyle::State::Focused);

    auto *view = m_controlFactory.drawingView(rect, style);

    drawCell(context, logicalRect, deviceScaleFactor, styleForDrawing, [m_searchFieldCell cancelButtonCell], view, true);
}

} // namespace WebCore

#endif // PLATFORM(MAC)
