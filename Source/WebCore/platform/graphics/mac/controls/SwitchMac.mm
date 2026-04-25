/*
 * Copyright (C) 2023-2026 Apple Inc. All rights reserved.
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
#import "SwitchMac.h"

#if PLATFORM(MAC)

#import "ColorCocoa.h"
#import "ControlFactoryMac.h"
#import "FloatRoundedRect.h"
#import "GraphicsContext.h"
#import "ImageBuffer.h"
#import "LocalCurrentGraphicsContext.h"
#import "LocalDefaultSystemAppearance.h"
#import "SwitchMacUtilities.h"
#import <pal/spi/mac/CoreUISPI.h>
#import <pal/spi/mac/NSAppearanceSPI.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SwitchMac);

SwitchMac::SwitchMac(SwitchPart& part, ControlFactoryMac& controlFactory)
    : ControlMac(part, controlFactory)
{
    ASSERT(part.type() == StyleAppearance::Switch);
}

IntSize SwitchMac::cellSize(NSControlSize controlSize, const ControlStyle&) const
{
    return SwitchMacUtilities::cellSize(controlSize);
}

IntOutsets SwitchMac::cellOutsets(NSControlSize controlSize, const ControlStyle&) const
{
    return SwitchMacUtilities::cellOutsets(controlSize);
}

FloatRect SwitchMac::rectForBounds(const FloatRect& bounds, const ControlStyle&) const
{
    return SwitchMacUtilities::rectForBounds(bounds);
}

static RefPtr<ImageBuffer> createTrackStateImage(GraphicsContext& context, RefPtr<ImageBuffer> trackMaskImage, FloatSize trackRectSize, float deviceScaleFactor, const ControlStyle& style, bool isOn, bool isInlineFlipped, bool isVertical, bool isEnabled, bool isPressed, bool isInActiveWindow, bool needsOnOffLabels, NSString *coreUISize)
{
    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);

    auto drawingTrackRect = NSMakeRect(0, 0, trackRectSize.width(), trackRectSize.height());

    auto trackImage = context.createImageBuffer(trackRectSize, deviceScaleFactor);

    if (!trackImage)
        return nullptr;

    RetainPtr cgContext = trackImage->context().platformContext();

    auto coreUIValue = @(isOn ? 1 : 0);
    auto coreUIState = (__bridge NSString *)(!isEnabled ? kCUIStateDisabled : isPressed ? kCUIStatePressed : kCUIStateActive);
    auto coreUIPresentation = (__bridge NSString *)(isInActiveWindow ? kCUIPresentationStateActiveKey : kCUIPresentationStateInactive);
    auto coreUIDirection = (__bridge NSString *)(isInlineFlipped ? kCUIUserInterfaceLayoutDirectionRightToLeft : kCUIUserInterfaceLayoutDirectionLeftToRight);

    CGContextStateSaver stateSaver(cgContext.get());

    // FIXME: clipping in context() might not always be accurate for context().platformContext().
    trackImage->context().clipToImageBuffer(*trackMaskImage, drawingTrackRect);

    [[NSAppearance currentDrawingAppearance] _drawInRect:drawingTrackRect context:cgContext.get() options:@{
        (__bridge NSString *)kCUIWidgetKey: (__bridge NSString *)kCUIWidgetSwitchFill,
        (__bridge NSString *)kCUIStateKey: coreUIState,
        (__bridge NSString *)kCUIValueKey: coreUIValue,
        (__bridge NSString *)kCUIPresentationStateKey: coreUIPresentation,
        (__bridge NSString *)kCUISizeKey: coreUISize,
        (__bridge NSString *)kCUIUserInterfaceLayoutDirectionKey: coreUIDirection,
        (__bridge NSString *)kCUIScaleKey: @(deviceScaleFactor),
    }];

    [[NSAppearance currentDrawingAppearance] _drawInRect:drawingTrackRect context:cgContext.get() options:@{
        (__bridge NSString *)kCUIWidgetKey: (__bridge NSString *)kCUIWidgetSwitchBorder,
        (__bridge NSString *)kCUISizeKey: coreUISize,
        (__bridge NSString *)kCUIUserInterfaceLayoutDirectionKey: coreUIDirection,
        (__bridge NSString *)kCUIScaleKey: @(deviceScaleFactor),
    }];

    if (needsOnOffLabels) {
        // This ensures the on label continues to appear upright.
        if (isVertical && isOn) {
            auto isRegularSize = coreUISize == (__bridge NSString *)kCUISizeRegular;
            if (isInlineFlipped) {
                auto thumbLogicalLeftAxis = trackRectSize.width() - trackRectSize.height();
                auto y = -thumbLogicalLeftAxis;
                trackImage->context().translate(thumbLogicalLeftAxis, y);
            }
            if (!isInlineFlipped && isRegularSize)
                trackImage->context().translate(0.0f, 1.f);
            SwitchMacUtilities::rotateContextForVerticalWritingMode(trackImage->context(), drawingTrackRect);
        }

        [[NSAppearance currentDrawingAppearance] _drawInRect:drawingTrackRect context:cgContext.get() options:@{
            (__bridge NSString *)kCUIWidgetKey: (__bridge NSString *)kCUIWidgetSwitchOnOffLabel,
            // FIXME: Below does not pass kCUIStatePressed like NSCoreUIStateForSwitchState does,
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

void SwitchMac::drawTrack(GraphicsContext& context, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    Ref owningPart = this->owningPart();
    GraphicsContextStateSaver stateSaver(context);

    auto isOn = owningPart->isOn();
    auto isInlineFlipped = style.states.contains(ControlStyle::State::InlineFlippedWritingMode);
    auto isVertical = style.states.contains(ControlStyle::State::VerticalWritingMode);
    auto isEnabled = style.states.contains(ControlStyle::State::Enabled);
    auto isPressed = style.states.contains(ControlStyle::State::Pressed);
    auto isInActiveWindow = style.states.contains(ControlStyle::State::WindowActive);
    auto isFocused = style.states.contains(ControlStyle::State::Focused);
    auto needsOnOffLabels = userPrefersWithoutColorDifferentiation();
    auto progress = SwitchMacUtilities::easeInOut(owningPart->progress());

    auto logicalBounds = SwitchMacUtilities::rectWithTransposedSize(borderRect.rect(), isVertical);
    auto controlSize = controlSizeForSize(logicalBounds.size(), style);
    auto size = SwitchMacUtilities::visualCellSize(cellSize(controlSize, style), style);
    auto outsets = SwitchMacUtilities::visualCellOutsets(controlSize, isVertical);

    auto trackRect = SwitchMacUtilities::trackRectForBounds(logicalBounds, size);
    auto inflatedTrackRect = inflatedRect(trackRect, size, outsets, style);
    if (isVertical)
        inflatedTrackRect.setSize(inflatedTrackRect.size().transposedSize());

    if (style.zoomFactor != 1) {
        inflatedTrackRect.scale(1 / style.zoomFactor);
        trackRect.scale(1 / style.zoomFactor);
        context.scale(style.zoomFactor);
    }

    auto coreUISize = SwitchMacUtilities::coreUISizeForControlSize(controlSize);

    auto maskImage = SwitchMacUtilities::trackMaskImage(context, inflatedTrackRect.size(), deviceScaleFactor, isInlineFlipped, coreUISize);
    if (!maskImage)
        return;

    auto createTrackImage = [&](bool isOn) {
        return createTrackStateImage(context, maskImage, inflatedTrackRect.size(), deviceScaleFactor, style, isOn, isInlineFlipped, isVertical, isEnabled, isPressed, isInActiveWindow, needsOnOffLabels, coreUISize);
    };

    RefPtr<ImageBuffer> trackImageBuffer;
    if (progress == 0.0f || progress == 1.0f) {
        trackImageBuffer = createTrackImage(progress == 0.0f ? !isOn : isOn);
        if (!trackImageBuffer)
            return;
    } else {
        auto fromImage = createTrackImage(!isOn);
        if (!fromImage)
            return;
        auto toImage = createTrackImage(isOn);
        if (!toImage)
            return;
        trackImageBuffer = context.createImageBuffer(inflatedTrackRect.size(), deviceScaleFactor);
        if (!trackImageBuffer)
            return;
        // This logic is from CrossfadeGeneratedImage.h, but we copy it to avoid some overhead and
        // also because that class is not supposed to be used in GPUP.
        // FIXME: As above, not using context().platformContext() here is likely dubious.
        trackImageBuffer->context().setAlpha(1.0f - progress);
        trackImageBuffer->context().drawConsumingImageBuffer(WTF::move(fromImage), IntPoint(), ImagePaintingOptions { CompositeOperator::SourceOver });
        trackImageBuffer->context().setAlpha(progress);
        trackImageBuffer->context().drawConsumingImageBuffer(WTF::move(toImage), IntPoint(), ImagePaintingOptions { CompositeOperator::PlusLighter });
    }

    {
        GraphicsContextStateSaver rotationStateSaver(context);
        if (isVertical)
            SwitchMacUtilities::rotateContextForVerticalWritingMode(context, inflatedTrackRect);
        context.drawConsumingImageBuffer(WTF::move(trackImageBuffer), inflatedTrackRect.location());
    }

    if (isFocused) {
        auto color = colorFromCocoaColor([NSColor keyboardFocusIndicatorColor]).opaqueColor();
        context.drawFocusRing(Vector { trackRect }, 0, color, style.zoomFactor);
    }
}

void SwitchMac::drawThumb(GraphicsContext& context, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    Ref owningPart = this->owningPart();
    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);

    GraphicsContextStateSaver stateSaver(context);

    auto isOn = owningPart->isOn();
    auto isInlineFlipped = style.states.contains(ControlStyle::State::InlineFlippedWritingMode);
    auto isVertical = style.states.contains(ControlStyle::State::VerticalWritingMode);
    auto isEnabled = style.states.contains(ControlStyle::State::Enabled);
    auto isPressed = style.states.contains(ControlStyle::State::Pressed);
    auto progress = SwitchMacUtilities::easeInOut(owningPart->progress());

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

    auto drawingThumbIsLogicallyLeft = (!isInlineFlipped && !isOn) || (isInlineFlipped && isOn);
    auto drawingThumbLogicalXAxis = inflatedTrackRect.width() - inflatedThumbRect.width();
    auto drawingThumbLogicalXAxisProgress = drawingThumbLogicalXAxis * progress;
    auto drawingThumbLogicalX = drawingThumbIsLogicallyLeft ? drawingThumbLogicalXAxis - drawingThumbLogicalXAxisProgress : drawingThumbLogicalXAxisProgress;
    auto drawingThumbRect = NSMakeRect(drawingThumbLogicalX, 0, inflatedThumbRect.width(), inflatedThumbRect.height());

    auto coreUISize = SwitchMacUtilities::coreUISizeForControlSize(controlSize);

    auto maskImage = SwitchMacUtilities::trackMaskImage(context, inflatedTrackRect.size(), deviceScaleFactor, isInlineFlipped, coreUISize);
    if (!maskImage)
        return;

    auto thumbImage = context.createImageBuffer(inflatedTrackRect.size(), deviceScaleFactor);
    if (!thumbImage)
        return;

    auto cgContext = thumbImage->context().platformContext();

    {
        CGContextStateSaver stateSaverTrack(cgContext);

        // FIXME: clipping in context() might not always be accurate for context().platformContext().
        thumbImage->context().clipToImageBuffer(*maskImage, NSMakeRect(0, 0, inflatedTrackRect.width(), inflatedTrackRect.height()));

        [[NSAppearance currentDrawingAppearance] _drawInRect:drawingThumbRect context:cgContext options:@{
            (__bridge NSString *)kCUIWidgetKey: (__bridge NSString *)kCUIWidgetSwitchKnob,
            (__bridge NSString *)kCUIStateKey: (__bridge NSString *)(!isEnabled ? kCUIStateDisabled : isPressed ? kCUIStatePressed : kCUIStateActive),
            (__bridge NSString *)kCUISizeKey: SwitchMacUtilities::coreUISizeForControlSize(controlSize),
            (__bridge NSString *)kCUIUserInterfaceLayoutDirectionKey: (__bridge NSString *)(isInlineFlipped ? kCUIUserInterfaceLayoutDirectionRightToLeft : kCUIUserInterfaceLayoutDirectionLeftToRight),
            (__bridge NSString *)kCUIScaleKey: @(deviceScaleFactor),
        }];
    }

    if (isVertical)
        SwitchMacUtilities::rotateContextForVerticalWritingMode(context, inflatedTrackRect);

    context.drawConsumingImageBuffer(WTF::move(thumbImage), inflatedTrackRect.location());
}

void SwitchMac::draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    drawTrack(context, borderRect, deviceScaleFactor, style);
    drawThumb(context, borderRect, deviceScaleFactor, style);
}

} // namespace WebCore

#endif // PLATFORM(MAC)
