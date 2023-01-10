/*
 * Copyright (C) 2023 Apple Inc. All Rights Reserved.
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
#import "SliderThumbMac.h"

#if PLATFORM(MAC)

#import "GraphicsContext.h"
#import "LocalCurrentGraphicsContext.h"
#import "LocalDefaultSystemAppearance.h"
#import "SliderThumbPart.h"

namespace WebCore {

SliderThumbMac::SliderThumbMac(SliderThumbPart& owningPart, ControlFactoryMac& controlFactory, NSSliderCell *sliderCell)
    : ControlMac(owningPart, controlFactory)
    , m_sliderCell(sliderCell)
{
    ASSERT(m_owningPart.type() == StyleAppearance::SliderThumbHorizontal || m_owningPart.type() == StyleAppearance::SliderThumbVertical);
}

void SliderThumbMac::updateCellStates(const FloatRect& rect, const ControlStyle& style)
{
    ControlMac::updateCellStates(rect, style);

    updateEnabledState(m_sliderCell.get(), style);
    updateFocusedState(m_sliderCell.get(), style);

    auto *view = m_controlFactory.drawingView(rect, style);

    if (style.states.contains(ControlStyle::State::Pressed))
        [m_sliderCell startTrackingAt:NSPoint() inView:view];
    else
        [m_sliderCell stopTracking:NSPoint() at:NSPoint() inView:view mouseIsUp:YES];
}

FloatRect SliderThumbMac::rectForBounds(const FloatRect& bounds, const ControlStyle& style) const
{
    if (m_owningPart.type() == StyleAppearance::SliderThumbHorizontal)
        return bounds;

    // Make the height of the vertical slider slightly larger so NSSliderCell will draw a vertical slider.
    static constexpr float verticalSliderHeightPadding = 0.1f;
    return { bounds.location(), bounds.size() + FloatSize { 0, verticalSliderHeightPadding * style.zoomFactor } };
}

void SliderThumbMac::draw(GraphicsContext& context, const FloatRect& rect, float deviceScaleFactor, const ControlStyle& style)
{
    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);
    LocalCurrentGraphicsContext localContext(context);
    
    auto logicalRect = rectForBounds(rect, style);

    GraphicsContextStateSaver stateSaver(context);

    if (style.zoomFactor != 1) {
        logicalRect.scale(1 / style.zoomFactor);
        context.scale(style.zoomFactor);
    }

    // Never draw a focus ring for the slider thumb.
    auto styleForDrawing = style;
    styleForDrawing.states.remove(ControlStyle::State::Focused);

    auto *view = m_controlFactory.drawingView(rect, style);

    drawCell(context, logicalRect, deviceScaleFactor, styleForDrawing, m_sliderCell.get(), view, true);
}

} // namespace WebCore

#endif // PLATFORM(MAC)
