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
#import "SwitchMacUtilities.h"

#if PLATFORM(MAC)

#import "GraphicsContext.h"
#import "ImageBuffer.h"
#import "LocalCurrentGraphicsContext.h"
#import <pal/spi/mac/CoreUISPI.h>
#import <pal/spi/mac/NSAppearanceSPI.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore::SwitchMacUtilities {

IntSize cellSize(NSControlSize controlSize)
{
    static const std::array<IntSize, 4> sizes =
    {
        IntSize { 38, 22 },
        IntSize { 32, 18 },
        IntSize { 26, 15 },
        IntSize { 38, 22 }
    };
    return sizes[controlSize];
}

FloatSize visualCellSize(IntSize size, const ControlStyle& style)
{
    if (style.states.contains(ControlStyle::State::VerticalWritingMode))
        size = size.transposedSize();
    size.scale(style.zoomFactor);
    return size;
}

IntOutsets cellOutsets(NSControlSize controlSize)
{
    static const IntOutsets margins[] =
    {
        // top right bottom left
        { 2, 2, 1, 2 },
        { 2, 2, 1, 2 },
        { 1, 1, 0, 1 },
        { 2, 2, 1, 2 },
    };
    return margins[controlSize];
}

IntOutsets visualCellOutsets(NSControlSize controlSize, bool isVertical)
{
    auto outsets = cellOutsets(controlSize);
    if (isVertical)
        outsets = { outsets.left(), outsets.top(), outsets.right(), outsets.bottom() };
    return outsets;
}

FloatRect rectForBounds(const FloatRect& bounds)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return bounds;
}

NSString *coreUISizeForControlSize(const NSControlSize controlSize)
{
    if (controlSize == NSControlSizeMini)
        return (__bridge NSString *)kCUISizeMini;
    if (controlSize == NSControlSizeSmall)
        return (__bridge NSString *)kCUISizeSmall;
    return (__bridge NSString *)kCUISizeRegular;
}

float easeInOut(const float progress)
{
    return -2.0f * pow(progress, 3.0f) + 3.0f * pow(progress, 2.0f);
}

FloatRect rectWithTransposedSize(const FloatRect& rect, bool isVertical)
{
    auto logicalRect = rect;
    if (isVertical)
        logicalRect.setSize(logicalRect.size().transposedSize());
    return logicalRect;
}

FloatRect trackRectForBounds(const FloatRect& bounds, const FloatSize& size)
{
    auto offsetY = std::max(((bounds.height() - size.height()) / 2.0f), 0.0f);
    return { FloatPoint { bounds.x(), bounds.y() + offsetY }, size };
}

void rotateContextForVerticalWritingMode(GraphicsContext& context, const FloatRect& rect)
{
    context.translate(rect.height(), 0);
    context.translate(rect.location());
    context.rotate(piOverTwoFloat);
    context.translate(-rect.location());
}

RefPtr<ImageBuffer> trackMaskImage(GraphicsContext& context, FloatSize trackRectSize, float deviceScaleFactor, bool isRTL, NSString *coreUISize)
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

} // namespace WebCore::SwitchMacUtilities

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // PLATFORM(MAC)
