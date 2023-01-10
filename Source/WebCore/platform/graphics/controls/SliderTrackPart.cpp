/*
 * Copyright (C) 2023 Apple Inc.  All rights reserved.
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
#include "SliderTrackPart.h"

#include "ControlFactory.h"
#include "GraphicsContext.h"

namespace WebCore {

Ref<SliderTrackPart> SliderTrackPart::create(StyleAppearance type, const IntSize& thumbSize, const IntRect& trackBounds, Vector<double>&& tickRatios)
{
    return adoptRef(*new SliderTrackPart(type, thumbSize, trackBounds, WTFMove(tickRatios)));
}

SliderTrackPart::SliderTrackPart(StyleAppearance type, const IntSize& thumbSize, const IntRect& trackBounds, Vector<double>&& tickRatios)
    : ControlPart(type)
    , m_thumbSize(thumbSize)
    , m_trackBounds(trackBounds)
    , m_tickRatios(WTFMove(tickRatios))
{
    ASSERT(type == StyleAppearance::SliderHorizontal || type == StyleAppearance::SliderVertical);
}

#if ENABLE(DATALIST_ELEMENT)
void SliderTrackPart::drawTicks(GraphicsContext& context, const FloatRect& rect, const ControlStyle& style) const
{
    if (m_tickRatios.isEmpty())
        return;

    static constexpr IntSize sliderTickSize = { 1, 3 };
    static constexpr int sliderTickOffsetFromTrackCenter = -9;

    bool isHorizontal = type() == StyleAppearance::SliderHorizontal;

    auto trackBounds = m_trackBounds;
    trackBounds.moveBy(IntPoint(rect.location()));

    auto tickSize = isHorizontal? sliderTickSize : sliderTickSize.transposedSize();
    tickSize.scale(style.zoomFactor);

    FloatPoint tickLocation;
    float tickRegionMargin = 0;
    float tickRegionWidth = 0;
    float offsetFromTrackCenter = sliderTickOffsetFromTrackCenter * style.zoomFactor;

    if (isHorizontal) {
        tickLocation = { 0, rect.center().y() + offsetFromTrackCenter };
        tickRegionMargin = trackBounds.x() + (m_thumbSize.width() - tickSize.width()) / 2.0;
        tickRegionWidth = trackBounds.width() - m_thumbSize.width();
    } else {
        tickLocation = { rect.center().x() + offsetFromTrackCenter, 0 };
        tickRegionMargin = trackBounds.y() + (m_thumbSize.height() - tickSize.height()) / 2.0;
        tickRegionWidth = trackBounds.height() - m_thumbSize.height();
    }

    auto tickRect = FloatRect { tickLocation, tickSize };

    GraphicsContextStateSaver stateSaver(context);
    context.setFillColor(style.textColor);

    bool isVerticalWritingMode = style.states.contains(ControlStyle::State::VerticalWritingMode);
    bool isRightToLeft = style.states.contains(ControlStyle::State::RightToLeft);
    bool isReversedInlineDirection = (!isHorizontal && !isVerticalWritingMode) || isRightToLeft;

    for (auto tickRatio : m_tickRatios) {
        double value = isReversedInlineDirection ? 1.0 - tickRatio : tickRatio;
        double tickPosition = round(tickRegionMargin + tickRegionWidth * value);
        if (isHorizontal)
            tickRect.setX(tickPosition);
        else
            tickRect.setY(tickPosition);
        context.fillRect(tickRect);
    }
}
#endif

std::unique_ptr<PlatformControl> SliderTrackPart::createPlatformControl()
{
    return controlFactory().createPlatformSliderTrack(*this);
}

} // namespace WebCore
