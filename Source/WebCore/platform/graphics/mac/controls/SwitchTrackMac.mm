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

namespace WebCore {

SwitchTrackMac::SwitchTrackMac(SwitchTrackPart& part, ControlFactoryMac& controlFactory)
    : ControlMac(part, controlFactory)
{
    ASSERT(part.type() == StyleAppearance::SwitchTrack);
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

static RefPtr<ImageBuffer> trackMaskImage(GraphicsContext& context, FloatSize trackRectSize, float deviceScaleFactor, bool isRTL, NSString * coreUISize)
{
    auto drawingTrackRect = NSMakeRect(0, 0, trackRectSize.width(), trackRectSize.height());
    auto maskImage = context.createImageBuffer(trackRectSize, deviceScaleFactor);
    if (!maskImage)
        return nullptr;

    auto cgContext = maskImage->context().platformContext();

    auto coreUIDirection = (__bridge NSString *)(isRTL ? kCUIUserInterfaceLayoutDirectionRightToLeft : kCUIUserInterfaceLayoutDirectionLeftToRight);

    CGContextStateSaver stateSaver(cgContext);

    [[NSAppearance currentDrawingAppearance] _drawInRect:drawingTrackRect context:cgContext options:@{
        (__bridge NSString *)kCUIWidgetKey: (__bridge NSString *)kCUIWidgetSwitchFillMask,
        (__bridge NSString *)kCUISizeKey: coreUISize,
        (__bridge NSString *)kCUIUserInterfaceLayoutDirectionKey: coreUIDirection,
        (__bridge NSString *)kCUIScaleKey: @(deviceScaleFactor),
    }];

    return maskImage;
}

static RefPtr<ImageBuffer> trackImage(GraphicsContext& context, RefPtr<ImageBuffer> trackMaskImage, FloatSize trackRectSize, float deviceScaleFactor, const ControlStyle& style, bool isOn, bool isRTL, bool isEnabled, bool isPressed, bool isInActiveWindow, bool needsOnOffLabels, NSString * coreUISize)
{
    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);

    auto drawingTrackRect = NSMakeRect(0, 0, trackRectSize.width(), trackRectSize.height());

    auto trackImage = context.createImageBuffer(trackRectSize, deviceScaleFactor);

    if (!trackImage)
        return nullptr;

    auto cgContext = trackImage->context().platformContext();

    auto coreUIValue = @(isOn ? 1 : 0);
    auto coreUIState = (__bridge NSString *)(!isEnabled ? kCUIStateDisabled : isPressed ? kCUIStatePressed : kCUIStateActive);
    auto coreUIPresentation = (__bridge NSString *)(isInActiveWindow ? kCUIPresentationStateActiveKey : kCUIPresentationStateInactive);
    auto coreUIDirection = (__bridge NSString *)(isRTL ? kCUIUserInterfaceLayoutDirectionRightToLeft : kCUIUserInterfaceLayoutDirectionLeftToRight);

    CGContextStateSaver stateSaver(cgContext);

    // FIXME: clipping in context() might not always be accurate for context().platformContext().
    trackImage->context().clipToImageBuffer(*trackMaskImage, drawingTrackRect);

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

    if (needsOnOffLabels) {
        [[NSAppearance currentDrawingAppearance] _drawInRect:drawingTrackRect context:cgContext options:@{
            (__bridge NSString *)kCUIWidgetKey: (__bridge NSString *)kCUIWidgetSwitchOnOffLabel,
            // FIXME: below does not pass kCUIStatePressed like NSCoreUIStateForSwitchState does,
            // as passing that does not appear to work correctly. Might be related to
            // rdar://118563716.
            (__bridge NSString *)kCUIStateKey: (__bridge NSString *)(!isEnabled ? kCUIStateDisabled : kCUIStateActive),
            (__bridge NSString *)kCUIValueKey: coreUIValue,
            (__bridge NSString *)kCUIPresentationStateKey: coreUIPresentation,
            (__bridge NSString *)kCUISizeKey: coreUISize,
            (__bridge NSString *)kCUIUserInterfaceLayoutDirectionKey: coreUIDirection,
            (__bridge NSString *)kCUIScaleKey: @(deviceScaleFactor),
        }];
    }

    return trackImage;
}

void SwitchTrackMac::draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    auto isOn = owningPart().isOn();
    auto isRTL = style.states.contains(ControlStyle::State::RightToLeft);
    auto isEnabled = style.states.contains(ControlStyle::State::Enabled);
    auto isPressed = style.states.contains(ControlStyle::State::Pressed);
    auto isInActiveWindow = style.states.contains(ControlStyle::State::WindowActive);
    auto needsOnOffLabels = userPrefersWithoutColorDifferentiation();
    auto progress = SwitchMacUtilities::easeInOut(owningPart().progress());

    auto borderRectRect = borderRect.rect();
    auto controlSize = controlSizeForSize(borderRectRect.size(), style);
    auto size = cellSize(controlSize, style);
    auto outsets = cellOutsets(controlSize, style);

    size.scale(style.zoomFactor);

    auto trackY = std::max(((borderRectRect.height() - size.height()) / 2.0f), 0.0f);
    auto trackRect = FloatRect { FloatPoint { borderRectRect.x(), borderRectRect.y() + trackY }, size };
    auto inflatedTrackRect = inflatedRect(trackRect, size, outsets, style);

    auto coreUISize = SwitchMacUtilities::coreUISizeForControlSize(controlSize);

    auto maskImage = trackMaskImage(context, inflatedTrackRect.size(), deviceScaleFactor, isRTL, coreUISize);
    if (!maskImage)
        return;

    auto createTrackImage = [&](bool isOn) {
        return trackImage(context, maskImage, inflatedTrackRect.size(), deviceScaleFactor, style, isOn, isRTL, isEnabled, isPressed, isInActiveWindow, needsOnOffLabels, coreUISize);
    };

    GraphicsContextStateSaver stateSaver(context);

    RefPtr<ImageBuffer> trackImage;
    if (progress == 0.0f || progress == 1.0f) {
        trackImage = createTrackImage(progress == 0.0f ? !isOn : isOn);
        if (!trackImage)
            return;
    } else {
        auto fromImage = createTrackImage(!isOn);
        if (!fromImage)
            return;
        auto toImage = createTrackImage(isOn);
        if (!toImage)
            return;
        trackImage = context.createImageBuffer(inflatedTrackRect.size(), deviceScaleFactor);
        if (!trackImage)
            return;
        // This logic is from CrossfadeGeneratedImage.h, but we copy it to avoid some overhead and
        // also because that class is not supposed to be used in GPUP.
        // FIXME: as above, not using context().platformContext() here is likely dubious.
        trackImage->context().setAlpha(1.0f - progress);
        trackImage->context().drawConsumingImageBuffer(WTFMove(fromImage), IntPoint(), ImagePaintingOptions { CompositeOperator::SourceOver });
        trackImage->context().setAlpha(progress);
        trackImage->context().drawConsumingImageBuffer(WTFMove(toImage), IntPoint(), ImagePaintingOptions { CompositeOperator::PlusLighter });
    }
    context.drawConsumingImageBuffer(WTFMove(trackImage), inflatedTrackRect.location());
}

} // namespace WebCore

#endif // PLATFORM(MAC)
