/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "Helpers/mac/DataDetectorHighlightTestUtilities.h"

#if PLATFORM(MAC) && ENABLE(DATA_DETECTION)

#import "Helpers/Utilities.h"
#import <wtf/Function.h>
#import <pal/mac/DataDetectorsSoftLink.h>

namespace TestWebKitAPI {

RetainPtr<DDHighlightRef> createDataDetectorHighlight(CGRect textRect, CGRect visibleRect)
{
    return adoptCF(PAL::softLink_DataDetectors_DDHighlightCreateWithRectsInVisibleRectWithStyleScaleAndDirection(nullptr, &textRect, 1, visibleRect, static_cast<DDHighlightStyle>(DDHighlightStyleBubbleStandard) | static_cast<DDHighlightStyle>(DDHighlightStyleStandardIconArrow), YES, NSWritingDirectionNatural, NO, YES, 0));
}

bool pointIsOnDataDetectorHighlight(DDHighlightRef highlight, CGPoint point)
{
    Boolean isOverButton { NO };
    return PAL::softLink_DataDetectors_DDHighlightPointIsOnHighlight(highlight, point, &isOverButton);
}

DataDetectorHighlightGeometry dataDetectorHighlightGeometry(CGRect textRect, CGRect visibleRect)
{
    RetainPtr highlight { createDataDetectorHighlight(textRect, visibleRect) };
    CGRect boundingRect { PAL::softLink_DataDetectors_DDHighlightGetBoundingRect(highlight.get()) };
    CGRect integralBounds { CGRectIntegral(boundingRect) };
    CGRect buttonRect { CGRectNull };
    for (auto y = CGRectGetMinY(integralBounds); y < CGRectGetMaxY(integralBounds); ++y) {
        for (auto x = CGRectGetMinX(integralBounds); x < CGRectGetMaxX(integralBounds); ++x) {
            Boolean isOverButton { NO };
            if (PAL::softLink_DataDetectors_DDHighlightPointIsOnHighlight(highlight.get(), CGPointMake(x, y), &isOverButton) && isOverButton)
                buttonRect = CGRectUnion(buttonRect, CGRectMake(x, y, 1, 1));
        }
    }
    EXPECT_FALSE(CGRectIsNull(buttonRect));
    return { boundingRect, buttonRect, CGPointMake(std::floor(CGRectGetMidX(buttonRect)), std::floor(CGRectGetMidY(buttonRect))) };
}

bool rectsAreNear(CGRect a, CGRect b, CGFloat tolerance)
{
    return std::abs(CGRectGetMinX(a) - CGRectGetMinX(b)) <= tolerance
        && std::abs(CGRectGetMinY(a) - CGRectGetMinY(b)) <= tolerance
        && std::abs(CGRectGetMaxX(a) - CGRectGetMaxX(b)) <= tolerance
        && std::abs(CGRectGetMaxY(a) - CGRectGetMaxY(b)) <= tolerance;
}

Vector<std::pair<RetainPtr<CALayer>, CGRect>> visibleLayersAndFrames(CALayer *rootLayer)
{
    Vector<std::pair<RetainPtr<CALayer>, CGRect>> layers;
    Function<void(CALayer *)> collect { [&](CALayer *layer) {
        if (layer.hidden || !layer.opacity)
            return;
        layers.append({ layer, [layer convertRect:layer.bounds toLayer:rootLayer] });
        for (CALayer *sublayer in layer.sublayers)
            collect(sublayer);
    } };
    collect(rootLayer);
    return layers;
}

RetainPtr<CALayer> visibleLayerNear(CALayer *rootLayer, CGRect expectedFrame, CGFloat tolerance)
{
    for (auto& [layer, frame] : visibleLayersAndFrames(rootLayer)) {
        if (rectsAreNear(frame, expectedFrame, tolerance))
            return layer;
    }
    return nil;
}

testing::AssertionResult hasVisibleLayerNear(CALayer *rootLayer, CGRect expectedFrame, CGFloat tolerance)
{
    auto layers { visibleLayersAndFrames(rootLayer) };
    if (layers.containsIf([&](auto& layerAndFrame) { return rectsAreNear(layerAndFrame.second, expectedFrame, tolerance); }))
        return testing::AssertionSuccess();

    auto isSimilar = [](CGFloat size, CGFloat expectedSize) {
        return size >= expectedSize / 2 && size <= expectedSize * 2;
    };
    auto failure { testing::AssertionFailure() << "No visible layer at " << NSStringFromRect(expectedFrame).UTF8String << ". Visible layers of a similar size: [" };
    for (auto& [layer, frame] : layers) {
        if (isSimilar(CGRectGetWidth(frame), CGRectGetWidth(expectedFrame)) && isSimilar(CGRectGetHeight(frame), CGRectGetHeight(expectedFrame)))
            failure << ' ' << NSStringFromRect(frame).UTF8String;
    }
    return failure << " ]";
}

testing::AssertionResult hoverUntilHighlightIsShown(TestWKWebView *webView, CGRect textRect, CGRect highlightBoundingRect, CGFloat tolerance)
{
    unsigned moveCount { 0 };
    testing::AssertionResult isShown { testing::AssertionFailure() };
    if (Util::waitFor([&] {
        // Every move must change the position: the PDF plugin only looks for data detectors when the mouse moves.
        CGPoint point { CGRectGetMidX(textRect) + (++moveCount % 2), CGRectGetMidY(textRect) };
        [webView mouseMoveToPoint:[webView convertPoint:point toView:nil] withFlags:0];
        [webView waitForPendingMouseEvents];
        [webView waitForNextPresentationUpdate];
        isShown = hasVisibleLayerNear([webView layer], highlightBoundingRect, tolerance);
        return static_cast<bool>(isShown);
    }))
        return testing::AssertionSuccess();
    return isShown;
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) && ENABLE(DATA_DETECTION)
