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
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import <WebKit/_WKFrameTreeNode.h>
#import <wtf/Function.h>
#import <wtf/text/MakeString.h>
#import <pal/mac/DataDetectorsSoftLink.h>

namespace TestWebKitAPI {

RetainPtr<DDHighlightRef> createDataDetectorHighlight(CGRect textRect, CGRect visibleRect, DataDetectorHighlightStyle style)
{
    DDHighlightStyle ddStyle { style == DataDetectorHighlightStyle::Bubble
        ? static_cast<DDHighlightStyle>(DDHighlightStyleBubbleStandard) | static_cast<DDHighlightStyle>(DDHighlightStyleStandardIconArrow)
        : static_cast<DDHighlightStyle>(DDHighlightStyleBubbleNone) | static_cast<DDHighlightStyle>(DDHighlightStyleStandardIconArrow) | static_cast<DDHighlightStyle>(DDHighlightStyleButtonShowAlways) };
    return adoptCF(PAL::softLink_DataDetectors_DDHighlightCreateWithRectsInVisibleRectWithStyleScaleAndDirection(nullptr, &textRect, 1, visibleRect, ddStyle, YES, NSWritingDirectionNatural, NO, YES, 0));
}

bool pointIsOnDataDetectorHighlight(DDHighlightRef highlight, CGPoint point)
{
    Boolean isOverButton { NO };
    return PAL::softLink_DataDetectors_DDHighlightPointIsOnHighlight(highlight, point, &isOverButton);
}

DataDetectorHighlightGeometry dataDetectorHighlightGeometry(CGRect textRect, CGRect visibleRect, DataDetectorHighlightStyle style)
{
    RetainPtr highlight { createDataDetectorHighlight(textRect, visibleRect, style) };
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

testing::AssertionResult waitUntilWindowClipRectWidthIs(TestWKWebView *webView, WKFrameInfo *frame, int width)
{
    int lastWidth { 0 };
    if (Util::waitFor([&] {
        lastWidth = [[webView objectByEvaluatingJavaScript:@"Math.round(internals.windowClipRect().width)" inFrame:frame] intValue];
        return lastWidth == width;
    }))
        return testing::AssertionSuccess();
    return testing::AssertionFailure() << "The frame's window clip rect is " << lastWidth << "px wide, not " << width << "px.";
}

String DataDetectorInSubframeTest::iframe(int left, const String& url)
{
    return makeString("<iframe style='position:absolute; left:"_s, left, "px; top:"_s, iframeTop, "px; width:"_s, iframeWidth, "px; height:"_s, iframeHeight,
        "px; border:"_s, iframeBorder, "px solid black; padding:"_s, iframePadding, "px' src='"_s, url, "'></iframe>"_s);
}

String DataDetectorInSubframeTest::mainHTML(const String& iframes)
{
    return makeString("<body style='margin:0'>"_s, iframes, "<div style='height:2000px'></div></body>"_s);
}

String DataDetectorInSubframeTest::nestedFrameHTML(const String& url, int width, int height)
{
    return makeString("<body style='margin:0'><iframe style='position:absolute; left:"_s, nestedFrameLeft, "px; top:"_s, nestedFrameTop, "px; width:"_s, width,
        "px; height:"_s, height, "px; border:0' src='"_s, url, "'></iframe></body>"_s);
}

HTTPResponse DataDetectorInSubframeTest::html(const String& body)
{
    return HTTPResponse { { { "Content-Type"_s, "text/html"_s } }, body };
}

void DataDetectorInSubframeTest::setUpWebView(std::initializer_list<std::pair<String, HTTPResponse>> routes, NOESCAPE const Function<void(WKWebViewConfiguration *)>& configure)
{
    server = makeUnique<HTTPServer>(routes, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration { [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES] };
    [configuration setWebsiteDataStore:[server->httpsProxyConfiguration() websiteDataStore]];
    configure(configuration.get());
    GetParam().applyTo(configuration.get());
    adjustConfiguration(configuration.get());

    navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 1000, 600) configuration:configuration.get() addToWindow:YES]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [[webView window] makeKeyAndOrderFront:nil];
    [[webView window] orderFrontRegardless];
}

void DataDetectorInSubframeTest::load(NSString *path)
{
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[@"https://example.com" stringByAppendingString:path]]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
}

RetainPtr<WKFrameInfo> DataDetectorInSubframeTest::nestedFrame()
{
    RetainPtr<WKFrameInfo> frame;
    EXPECT_TRUE(Util::waitFor([&] {
        frame = [webView mainFrame].childFrames.firstObject.childFrames.firstObject.info;
        return !!frame;
    }));
    return frame;
}

CGRect DataDetectorInSubframeTest::rectInWebView(NSArray *rectInFrame, CGPoint frameContentOrigin)
{
    return CGRectMake(frameContentOrigin.x + [[rectInFrame objectAtIndex:0] doubleValue], frameContentOrigin.y + [[rectInFrame objectAtIndex:1] doubleValue],
        [[rectInFrame objectAtIndex:2] doubleValue], [[rectInFrame objectAtIndex:3] doubleValue]);
}

CGPoint DataDetectorInSubframeTest::pointInSecondFrame(CGPoint pointInFirstFrame)
{
    return CGPointMake(pointInFirstFrame.x - CGRectGetMinX(iframeContentBox) + CGRectGetMinX(secondIframeContentBox), pointInFirstFrame.y);
}

CGPoint DataDetectorInSubframeTest::center(CGRect rect)
{
    return CGPointMake(CGRectGetMidX(rect), CGRectGetMidY(rect));
}

CGRect DataDetectorInSubframeTest::rootVisibleRect(CGRect contentBoxIfRoot)
{
    return GetParam().isCrossOriginWithSiteIsolation() ? CGRectIntersection(contentBoxIfRoot, [webView bounds]) : [webView bounds];
}

void DataDetectorInSubframeTest::moveMouse(CGPoint pointInWebView)
{
    [webView mouseMoveToPoint:[webView convertPoint:pointInWebView toView:nil] withFlags:0];
    [webView waitForPendingMouseEvents];
}

bool DataDetectorInSubframeTest::scrollTo(WKFrameInfo *frame, int scrollY)
{
    bool scrolled { Util::waitFor([&] {
        [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"window.scrollTo(0, %d)", scrollY] inFrame:frame];
        return [[webView objectByEvaluatingJavaScript:@"window.scrollY" inFrame:frame] intValue] == scrollY;
    }) };
    [webView waitForNextPresentationUpdate];
    return scrolled;
}

std::string DataDetectorInSubframeTest::testNameGenerator(testing::TestParamInfo<SiteIsolationParams> info)
{
    return info.param.testName();
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) && ENABLE(DATA_DETECTION)
