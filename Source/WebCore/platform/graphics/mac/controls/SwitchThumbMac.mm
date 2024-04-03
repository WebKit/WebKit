/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "SwitchThumbMac.h"

#if PLATFORM(MAC)

#import "ControlFactoryMac.h"
#import "FloatRoundedRect.h"
#import "GraphicsContext.h"
#import "LocalCurrentGraphicsContext.h"
#import "LocalDefaultSystemAppearance.h"
#import "SwitchMacUtilities.h"
#import <pal/spi/mac/CoreUISPI.h>
#import <pal/spi/mac/NSAppearanceSPI.h>

namespace WebCore {

SwitchThumbMac::SwitchThumbMac(SwitchThumbPart& part, ControlFactoryMac& controlFactory)
    : ControlMac(part, controlFactory)
{
    ASSERT(part.type() == StyleAppearance::SwitchThumb);
}

IntSize SwitchThumbMac::cellSize(NSControlSize controlSize, const ControlStyle&) const
{
    return SwitchMacUtilities::cellSize(controlSize);
}

IntOutsets SwitchThumbMac::cellOutsets(NSControlSize controlSize, const ControlStyle&) const
{
    return SwitchMacUtilities::cellOutsets(controlSize);
}

FloatRect SwitchThumbMac::rectForBounds(const FloatRect& bounds, const ControlStyle&) const
{
    return SwitchMacUtilities::rectForBounds(bounds);
}

void SwitchThumbMac::draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);

    GraphicsContextStateSaver stateSaver(context);

    auto isOn = owningPart().isOn();
    auto isRTL = style.states.contains(ControlStyle::State::RightToLeft);
    auto isVertical = style.states.contains(ControlStyle::State::VerticalWritingMode);
    auto isEnabled = style.states.contains(ControlStyle::State::Enabled);
    auto isPressed = style.states.contains(ControlStyle::State::Pressed);
    auto progress = SwitchMacUtilities::easeInOut(owningPart().progress());

    auto logicalBounds = SwitchMacUtilities::rectWithTransposedSize(borderRect.rect(), isVertical);
    auto controlSize = controlSizeForSize(logicalBounds.size(), style);
    auto logicalTrackSize = cellSize(controlSize, style);
    auto logicalThumbSize = IntSize { logicalTrackSize.height(), logicalTrackSize.height() };
    auto trackSize = SwitchMacUtilities::visualCellSize(logicalTrackSize, style);
    auto thumbSize = SwitchMacUtilities::visualCellSize(logicalThumbSize, style);
    auto outsets = SwitchMacUtilities::visualCellOutsets(controlSize, isVertical);

    auto trackRect = SwitchMacUtilities::trackRectForBounds(logicalBounds, trackSize);
    auto thumbRect = SwitchMacUtilities::trackRectForBounds(logicalBounds, thumbSize);

    auto inflatedTrackRect = inflatedRect(trackRect, trackSize, outsets, style);
    auto inflatedThumbRect = inflatedRect(thumbRect, thumbSize, outsets, style);
    if (isVertical) {
        inflatedTrackRect.setSize(inflatedTrackRect.size().transposedSize());
        inflatedThumbRect.setSize(inflatedThumbRect.size().transposedSize());
    }

    if (style.zoomFactor != 1) {
        inflatedTrackRect.scale(1 / style.zoomFactor);
        inflatedThumbRect.scale(1 / style.zoomFactor);
        context.scale(style.zoomFactor);
    }

    auto drawingThumbIsLogicallyLeft = (!isRTL && !isOn) || (isRTL && isOn);
    auto drawingThumbLogicalXAxis = inflatedTrackRect.width() - inflatedThumbRect.width();
    auto drawingThumbLogicalXAxisProgress = drawingThumbLogicalXAxis * progress;
    auto drawingThumbLogicalX = drawingThumbIsLogicallyLeft ? drawingThumbLogicalXAxis - drawingThumbLogicalXAxisProgress : drawingThumbLogicalXAxisProgress;
    auto drawingThumbRect = NSMakeRect(drawingThumbLogicalX, 0, inflatedThumbRect.width(), inflatedThumbRect.height());

    auto coreUISize = SwitchMacUtilities::coreUISizeForControlSize(controlSize);

    auto maskImage = SwitchMacUtilities::trackMaskImage(context, inflatedTrackRect.size(), deviceScaleFactor, isRTL, coreUISize);
    if (!maskImage)
        return;

    auto trackImage = context.createImageBuffer(inflatedTrackRect.size(), deviceScaleFactor);
    if (!trackImage)
        return;

    auto cgContext = trackImage->context().platformContext();

    {
        CGContextStateSaver stateSaverTrack(cgContext);

        // FIXME: clipping in context() might not always be accurate for context().platformContext().
        trackImage->context().clipToImageBuffer(*maskImage, NSMakeRect(0, 0, inflatedTrackRect.width(), inflatedTrackRect.height()));

        [[NSAppearance currentDrawingAppearance] _drawInRect:drawingThumbRect context:cgContext options:@{
            (__bridge NSString *)kCUIWidgetKey: (__bridge NSString *)kCUIWidgetSwitchKnob,
            (__bridge NSString *)kCUIStateKey: (__bridge NSString *)(!isEnabled ? kCUIStateDisabled : isPressed ? kCUIStatePressed : kCUIStateActive),
            (__bridge NSString *)kCUISizeKey: SwitchMacUtilities::coreUISizeForControlSize(controlSize),
            (__bridge NSString *)kCUIUserInterfaceLayoutDirectionKey: (__bridge NSString *)(isRTL ? kCUIUserInterfaceLayoutDirectionRightToLeft : kCUIUserInterfaceLayoutDirectionLeftToRight),
            (__bridge NSString *)kCUIScaleKey: @(deviceScaleFactor),
        }];
    }

    if (isVertical)
        SwitchMacUtilities::rotateContextForVerticalWritingMode(context, inflatedTrackRect);

    context.drawConsumingImageBuffer(WTFMove(trackImage), inflatedTrackRect.location());
}

} // namespace WebCore

#endif // PLATFORM(MAC)
