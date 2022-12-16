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
#import "MeterMac.h"

#if PLATFORM(MAC)

#import "ControlFactoryMac.h"
#import "GraphicsContext.h"
#import "LocalCurrentGraphicsContext.h"
#import "LocalDefaultSystemAppearance.h"
#import "MeterPart.h"
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

MeterMac::MeterMac(const MeterPart& owningMeterPart, ControlFactoryMac& controlFactory, NSLevelIndicatorCell* levelIndicatorCell)
    : ControlMac(owningMeterPart, controlFactory)
    , m_levelIndicatorCell(levelIndicatorCell)
{
    ASSERT(m_levelIndicatorCell);
}

void MeterMac::updateCellStates(const FloatRect& rect, const ControlStyle& style)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    ControlMac::updateCellStates(rect, style);

    [m_levelIndicatorCell setUserInterfaceLayoutDirection:style.states.contains(ControlStyle::State::RightToLeft) ? NSUserInterfaceLayoutDirectionRightToLeft : NSUserInterfaceLayoutDirectionLeftToRight];

    auto& meterPart = owningMeterPart();
    
    // Because NSLevelIndicatorCell does not support optimum-in-the-middle type coloring,
    // we explicitly control the color instead giving low and high value to NSLevelIndicatorCell as is.
    switch (meterPart.gaugeRegion()) {
    case MeterPart::GaugeRegion::Optimum:
        // Make meter the green
        [m_levelIndicatorCell setWarningValue:meterPart.value() + 1];
        [m_levelIndicatorCell setCriticalValue:meterPart.value() + 2];
        break;
    case MeterPart::GaugeRegion::Suboptimal:
        // Make the meter yellow
        [m_levelIndicatorCell setWarningValue:meterPart.value() - 1];
        [m_levelIndicatorCell setCriticalValue:meterPart.value() + 1];
        break;
    case MeterPart::GaugeRegion::EvenLessGood:
        // Make the meter red
        [m_levelIndicatorCell setWarningValue:meterPart.value() - 2];
        [m_levelIndicatorCell setCriticalValue:meterPart.value() - 1];
        break;
    }

    [m_levelIndicatorCell setObjectValue:@(meterPart.value())];
    [m_levelIndicatorCell setMinValue:meterPart.minimum()];
    [m_levelIndicatorCell setMaxValue:meterPart.maximum()];

    END_BLOCK_OBJC_EXCEPTIONS
}

FloatSize MeterMac::sizeForBounds(const FloatRect& bounds) const
{
    // Makes enough room for cell's intrinsic size.
    NSSize cellSize = [m_levelIndicatorCell cellSizeForBounds:IntRect({ }, IntSize(bounds.size()))];
    return { std::max<float>(bounds.width(), cellSize.width), std::max<float>(bounds.height(), cellSize.height) };
}

void MeterMac::draw(GraphicsContext& context, const FloatRect& rect, float deviceScaleFactor, const ControlStyle& style)
{
    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);

    auto *view = m_controlFactory.drawingView(rect, style);

    ControlMac::draw(context, rect, deviceScaleFactor, style, m_levelIndicatorCell.get(), view);
}

} // namespace WebCore

#endif // PLATFORM(MAC)
