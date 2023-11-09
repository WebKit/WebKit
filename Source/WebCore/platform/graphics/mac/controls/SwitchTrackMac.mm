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
#import "SwitchTrackMac.h"

#if PLATFORM(MAC)

#import "SwitchMacUtilities.h"
#import "SwitchTrackAppearance.h"

namespace WebCore {

SwitchTrackMac::SwitchTrackMac(ControlPart& part, ControlFactoryMac& controlFactory)
    : ControlMac(part, controlFactory)
{
    ASSERT(m_owningPart.type() == StyleAppearance::SwitchTrack);
}

IntSize SwitchTrackMac::cellSize(NSControlSize controlSize, const ControlStyle&) const
{
    return SwitchMacUtilities::cellSize(controlSize);
}

IntOutsets SwitchTrackMac::cellOutsets(NSControlSize controlSize, const ControlStyle&) const
{
    return SwitchMacUtilities::cellOutsets(controlSize);
}

FloatRect SwitchTrackMac::rectForBounds(const FloatRect& bounds, const ControlStyle&) const
{
    return SwitchMacUtilities::rectForBounds(bounds);
}

void SwitchTrackMac::draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);

    bool isOn = style.states.contains(ControlStyle::State::Checked);
    bool isRTL = style.states.contains(ControlStyle::State::RightToLeft);
    bool isEnabled = style.states.contains(ControlStyle::State::Enabled);
    bool isPressed = style.states.contains(ControlStyle::State::Pressed);
    bool isInActiveWindow = style.states.contains(ControlStyle::State::WindowActive);

    auto borderRectRect = borderRect.rect();
    auto controlSize = controlSizeForSize(borderRectRect.size(), style);
    auto size = cellSize(controlSize, style);
    auto outsets = cellOutsets(controlSize, style);

    size.scale(style.zoomFactor);

    auto trackY = std::max(((borderRectRect.height() - size.height()) / 2.0f), 0.0f);
    auto trackRect = FloatRect { FloatPoint { borderRectRect.x(), borderRectRect.y() + trackY }, size };
    auto inflatedTrackRect = inflatedRect(trackRect, size, outsets, style);
    auto drawingTrackRect = NSMakeRect(0, 0, inflatedTrackRect.width(), inflatedTrackRect.height());

    auto trackBuffer = context.createImageBuffer(inflatedTrackRect.size(), deviceScaleFactor);
    auto trackMaskBuffer = context.createImageBuffer(inflatedTrackRect.size(), deviceScaleFactor);

    if (!trackBuffer || !trackMaskBuffer)
        return;

    auto cgContext = trackBuffer->context().platformContext();
    auto cgContextForMask = trackMaskBuffer->context().platformContext();

    auto coreUIState = (__bridge NSString *)(!isEnabled ? kCUIStateDisabled : isPressed ? kCUIStatePressed : kCUIStateActive);
    auto coreUIValue = @(isOn ? 1 : 0);
    auto coreUIPresentation = (__bridge NSString *)(isInActiveWindow ? kCUIPresentationStateActiveKey : kCUIPresentationStateInactive);
    auto coreUISize = SwitchMacUtilities::coreUISizeForControlSize(controlSize);
    auto coreUIDirection = (__bridge NSString *)(isRTL ? kCUIUserInterfaceLayoutDirectionRightToLeft : kCUIUserInterfaceLayoutDirectionLeftToRight);

    GraphicsContextStateSaver stateSaver(context);

    [[NSAppearance currentDrawingAppearance] _drawInRect:drawingTrackRect context:cgContextForMask options:@{
        (__bridge NSString *)kCUIWidgetKey: (__bridge NSString *)kCUIWidgetSwitchFillMask,
        (__bridge NSString *)kCUISizeKey: coreUISize,
        (__bridge NSString *)kCUIUserInterfaceLayoutDirectionKey: coreUIDirection,
        (__bridge NSString *)kCUIScaleKey: @(deviceScaleFactor),
    }];

    trackBuffer->context().clipToImageBuffer(*trackMaskBuffer, drawingTrackRect);

    [[NSAppearance currentDrawingAppearance] _drawInRect:drawingTrackRect context:cgContext options:@{
        (__bridge NSString *)kCUIWidgetKey: (__bridge NSString *)kCUIWidgetSwitchFill,
        (__bridge NSString *)kCUIStateKey: coreUIState,
        (__bridge NSString *)kCUIValueKey: coreUIValue,
        (__bridge NSString *)kCUIPresentationStateKey: coreUIPresentation,
        (__bridge NSString *)kCUISizeKey: coreUISize,
        (__bridge NSString *)kCUIUserInterfaceLayoutDirectionKey: coreUIDirection,
        (__bridge NSString *)kCUIScaleKey: @(deviceScaleFactor),
    }];

    [[NSAppearance currentDrawingAppearance] _drawInRect:drawingTrackRect context:cgContext options:@{
        (__bridge NSString *)kCUIWidgetKey: (__bridge NSString *)kCUIWidgetSwitchBorder,
        (__bridge NSString *)kCUISizeKey: coreUISize,
        (__bridge NSString *)kCUIUserInterfaceLayoutDirectionKey: coreUIDirection,
        (__bridge NSString *)kCUIScaleKey: @(deviceScaleFactor),
    }];

    if (userPrefersWithoutColorDifferentiation()) {
        [[NSAppearance currentDrawingAppearance] _drawInRect:drawingTrackRect context:cgContext options:@{
            (__bridge NSString *)kCUIWidgetKey: (__bridge NSString *)kCUIWidgetSwitchOnOffLabel,
            // FIXME: below does not pass kCUIStatePressed like NSCoreUIStateForSwitchState does,
            // as passing that does not appear to work correctly.
            (__bridge NSString *)kCUIStateKey: (__bridge NSString *)(!isEnabled ? kCUIStateDisabled : kCUIStateActive),
            (__bridge NSString *)kCUIValueKey: coreUIValue,
            (__bridge NSString *)kCUIPresentationStateKey: coreUIPresentation,
            (__bridge NSString *)kCUISizeKey: coreUISize,
            (__bridge NSString *)kCUIUserInterfaceLayoutDirectionKey: coreUIDirection,
            (__bridge NSString *)kCUIScaleKey: @(deviceScaleFactor),
        }];
    }

    context.drawConsumingImageBuffer(WTFMove(trackBuffer), inflatedTrackRect.location());
}

} // namespace WebCore

#endif // PLATFORM(MAC)
