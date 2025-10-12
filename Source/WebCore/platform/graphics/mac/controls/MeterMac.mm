/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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
#import "LocalDefaultSystemAppearance.h"
#import "MeterPart.h"
#import <wtf/BlockObjCExceptions.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MeterMac);

MeterMac::MeterMac(MeterPart& owningMeterPart, ControlFactoryMac& controlFactory, NSLevelIndicatorCell* levelIndicatorCell)
    : ControlMac(owningMeterPart, controlFactory)
    , m_levelIndicatorCell(levelIndicatorCell)
{
    ASSERT(m_levelIndicatorCell);
}

MeterMac::~MeterMac() = default;

void MeterMac::updateCellStates(const FloatRect& rect, const ControlStyle& style)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    ControlMac::updateCellStates(rect, style);

    [m_levelIndicatorCell setUserInterfaceLayoutDirection:style.states.contains(ControlStyle::State::InlineFlippedWritingMode) ? NSUserInterfaceLayoutDirectionRightToLeft : NSUserInterfaceLayoutDirectionLeftToRight];

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

FloatSize MeterMac::sizeForBounds(const FloatRect& bounds, const ControlStyle& style) const
{
    auto isVerticalWritingMode = style.states.contains(ControlStyle::State::VerticalWritingMode);

    auto logicalSize = isVerticalWritingMode ? bounds.size().transposedSize() : bounds.size();

    // Makes enough room for cell's intrinsic size.
    NSSize cellSize = [m_levelIndicatorCell cellSizeForBounds:IntRect({ }, IntSize(logicalSize))];
    logicalSize = { std::max<float>(logicalSize.width(), cellSize.width), std::max<float>(logicalSize.height(), cellSize.height) };

    return isVerticalWritingMode ? logicalSize.transposedSize() : logicalSize;
}

void MeterMac::draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);

    GraphicsContextStateSaver stateSaver(context);

    auto rect = borderRect.rect();

    if (style.states.contains(ControlStyle::State::VerticalWritingMode)) {
        rect.setSize(rect.size().transposedSize());

        context.translate(rect.height(), 0);
        context.translate(rect.location());
        context.rotate(piOverTwoFloat);
        context.translate(-rect.location());
    }

    drawCell(context, rect, deviceScaleFactor, style, m_levelIndicatorCell.get());
}

} // namespace WebCore

#endif // PLATFORM(MAC)
