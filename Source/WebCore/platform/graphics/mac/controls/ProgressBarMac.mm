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
#import "ProgressBarMac.h"

#if PLATFORM(MAC)

#import "GraphicsContext.h"
#import "ImageBuffer.h"
#import "LocalDefaultSystemAppearance.h"
#import "ProgressBarPart.h"
#import <pal/spi/mac/CoreUISPI.h>
#import <pal/spi/mac/NSAppearanceSPI.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ProgressBarMac);

ProgressBarMac::ProgressBarMac(ProgressBarPart& owningPart, ControlFactoryMac& controlFactory)
    : ControlMac(owningPart, controlFactory)
{
}

IntSize ProgressBarMac::cellSize(NSControlSize controlSize, const ControlStyle&) const
{
    static const std::array<IntSize, 4> sizes =
    {
        IntSize(0, 20),
        IntSize(0, 12),
        IntSize(0, 12),
        IntSize(0, 20)
    };
    return sizes[controlSize];
}

IntOutsets ProgressBarMac::cellOutsets(NSControlSize controlSize, const ControlStyle&) const
{
    static const IntOutsets cellOutsets[] = {
        // top right bottom left
        { 0, 0, 1, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 1, 0 },
    };
    return cellOutsets[controlSize];
}

FloatRect ProgressBarMac::rectForBounds(const FloatRect& bounds, const ControlStyle& style) const
{
    auto isVerticalWritingMode = style.states.contains(ControlStyle::State::VerticalWritingMode);

    auto logicalBounds = bounds;
    if (isVerticalWritingMode)
        logicalBounds.setSize(bounds.size().transposedSize());

    int minimumProgressBarBlockSize = sizeForSystemFont(style).height();
    if (logicalBounds.height() > minimumProgressBarBlockSize)
        return bounds;

    auto controlSize = controlSizeForFont(style);
    auto size = cellSize(controlSize, style);
    auto outsets = cellOutsets(controlSize, style);

    size.scale(style.zoomFactor);

    if (isVerticalWritingMode) {
        outsets = { outsets.left(), outsets.top(), outsets.right(), outsets.bottom() };
        size.setWidth(size.height());
        size.setHeight(bounds.height());
    } else
        size.setWidth(bounds.width());

    // Make enough room for the shadow.
    return inflatedRect(bounds, size, outsets, style);
}

void ProgressBarMac::draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    LocalDefaultSystemAppearance localAppearance(style.states.contains(ControlStyle::State::DarkAppearance), style.accentColor);

    auto isVerticalWritingMode = style.states.contains(ControlStyle::State::VerticalWritingMode);

    auto inflatedRect = rectForBounds(borderRect.rect(), style);
    if (isVerticalWritingMode)
        inflatedRect.setSize(inflatedRect.size().transposedSize());

    auto imageBuffer = context.createImageBuffer(inflatedRect.size(), deviceScaleFactor);
    if (!imageBuffer)
        return;

    CGContextRef cgContext = imageBuffer->context().platformContext();

    auto& progressBarPart = owningProgressBarPart();
    auto controlSize = controlSizeForFont(style);
    bool isIndeterminate = progressBarPart.position() < 0;
    bool isActive = style.states.contains(ControlStyle::State::WindowActive);

    auto coreUISizeForProgressBarSize = [](NSControlSize size) -> CFStringRef {
        switch (size) {
        case NSControlSizeMini:
        case NSControlSizeSmall:
            return kCUISizeSmall;
        case NSControlSizeRegular:
        case NSControlSizeLarge:
            return kCUISizeRegular;
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    };

    [[NSAppearance currentDrawingAppearance] _drawInRect:NSMakeRect(0, 0, inflatedRect.width(), inflatedRect.height()) context:cgContext options:@{
        (__bridge NSString *)kCUIWidgetKey: (__bridge NSString *)(isIndeterminate ? kCUIWidgetProgressIndeterminateBar : kCUIWidgetProgressBar),
        (__bridge NSString *)kCUIValueKey: @(isIndeterminate ? 1 : std::min(nextafter(1.0, -1), progressBarPart.position())),
        (__bridge NSString *)kCUISizeKey: (__bridge NSString *)coreUISizeForProgressBarSize(controlSize),
        (__bridge NSString *)kCUIUserInterfaceLayoutDirectionKey: (__bridge NSString *)kCUIUserInterfaceLayoutDirectionLeftToRight,
        (__bridge NSString *)kCUIScaleKey: @(deviceScaleFactor),
        (__bridge NSString *)kCUIPresentationStateKey: (__bridge NSString *)(isActive ? kCUIPresentationStateActiveKey : kCUIPresentationStateInactive),
        (__bridge NSString *)kCUIOrientationKey: (__bridge NSString *)kCUIOrientHorizontal,
        (__bridge NSString *)kCUIAnimationStartTimeKey: @(progressBarPart.animationStartTime().seconds()),
        (__bridge NSString *)kCUIAnimationTimeKey: @(MonotonicTime::now().secondsSinceEpoch().seconds())
    }];

    GraphicsContextStateSaver stateSaver(context);

    if (isVerticalWritingMode) {
        context.translate(inflatedRect.height(), 0);
        context.translate(inflatedRect.location());
        context.rotate(piOverTwoFloat);
        context.translate(-inflatedRect.location());
    }

    if (style.states.contains(ControlStyle::State::RightToLeft)) {
        context.translate(2 * inflatedRect.x() + inflatedRect.width(), 0);
        context.scale(FloatSize(-1, 1));
    }

    context.drawConsumingImageBuffer(WTFMove(imageBuffer), inflatedRect.location());
}

} // namespace WebCore

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // PLATFORM(MAC)
