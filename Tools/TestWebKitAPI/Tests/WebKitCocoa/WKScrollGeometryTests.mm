/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(SWIFTUI)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestCocoa.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <wtf/RetainPtr.h>

#if PLATFORM(MAC)
#import <pal/spi/mac/NSScrollerImpSPI.h>
#endif

@interface WKWebView (ScrollGeometryTesting)
- (void)_setNeedsScrollGeometryUpdates:(BOOL)needsScrollGeometryUpdates;
@end

@interface TestScrollGeometryDelegate : NSObject <WKUIDelegate>

- (void)_webView:(WKWebView *)webView geometryDidChange:(WKScrollGeometry *)geometry;

- (CGSize)currentContentSize;

@end

@implementation TestScrollGeometryDelegate {
    RetainPtr<WKScrollGeometry> _currentGeometry;
}

- (void)_webView:(WKWebView *)webView geometryDidChange:(WKScrollGeometry *)geometry
{
    _currentGeometry = geometry;
}

- (CGSize)currentContentSize
{
    return [_currentGeometry contentSize];
}

@end

static void runContentSizeTest(NSString *html, CGSize expectedSize, BOOL needsScrollGeometryUpdates = YES)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView _setNeedsScrollGeometryUpdates:needsScrollGeometryUpdates];

    RetainPtr delegate = adoptNS([[TestScrollGeometryDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<!DOCTYPE html>%@", html]];

    [webView waitForNextPresentationUpdate];

    EXPECT_EQ([delegate currentContentSize], expectedSize);

    [webView synchronouslyLoadHTMLString:html];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ([delegate currentContentSize], expectedSize);
}

TEST(WKScrollGeometry, NoScrollGeometryUpdates)
{
    runContentSizeTest(@""
        "<html>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<head>"
        "<style>"
        "    div { background: red; height: 10000px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div>Test</div>"
        "</body>"
        "</html>", CGSizeZero, NO);
}

TEST(WKScrollGeometry, ContentSizeTallerThanWebView)
{
    CGFloat expectedWidth = 800;
#if PLATFORM(MAC)
    RetainPtr scroller = [NSScrollerImp scrollerImpWithStyle:NSScroller.preferredScrollerStyle controlSize:NSControlSizeRegular horizontal:NO replacingScrollerImp:nil];
    expectedWidth -= [scroller trackBoxWidth];
#endif

    runContentSizeTest(@""
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<style>"
        "    div { background: red; height: 10000px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div>Test</div>"
        "</body>"
        "</html>", CGSizeMake(expectedWidth - 8, 10016));
}

TEST(WKScrollGeometry, FixedSizeDiv)
{
    runContentSizeTest(@""
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<style>"
        "    div { background: red; width: 200px; height: 200px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div></div>"
        "</body>"
        "</html>", CGSizeMake(208, 216));
}

TEST(WKScrollGeometry, AbsolutePositionedRootElement)
{
#if PLATFORM(IOS_FAMILY)
    CGFloat expectedHeight = 136;
#else
    CGFloat expectedHeight = 134;
#endif

    runContentSizeTest(@""
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<style>"
        "    html { position: absolute; top: 100px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div>Test</div>"
        "</body>"
        "</html>", CGSizeMake(34, expectedHeight));
}

TEST(WKScrollGeometry, SimpleText)
{
#if PLATFORM(IOS_FAMILY)
    CGFloat expectedHeight = 36;
#else
    CGFloat expectedHeight = 34;
#endif

    runContentSizeTest(@""
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "</head>"
        "<body>"
        "<div>Test</div>"
        "</body>"
        "</html>", CGSizeMake(792, expectedHeight));
}

TEST(WKScrollGeometry, FixedHeightRootElementWithTallAbsolutePositionedDiv)
{
    runContentSizeTest(@""
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<style>"
        "    html { height: 200px; }"
        "    div { background: red; width: 20px; height: 10000px; position: absolute; }"
        "</style>"
        "</head>"
        "<body>"
        "<div>Test</div>"
        "</body>"
        "</html>", CGSizeMake(34, 10008));
}

TEST(WKScrollGeometry, ClippedRootElementWithFixedHeightBodyAndTallDiv)
{
    runContentSizeTest(@""
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<style>"
        "    html { overflow: clip; }"
        "    body { height: 200px; }"
        "    div { background: red; width: 20px; height: 10000px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div>Test</div>"
        "</body>"
        "</html>", CGSizeMake(34, 600));
}

TEST(WKScrollGeometry, NestedDivsWithAbsolutePositioning)
{
    runContentSizeTest(@""
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<style>"
        "    #outer { position: absolute; top: 100px; }"
        "    #inner { background: red; width: 200px; height: 200px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div id='outer'><div id='inner'></div></div>"
        "</body>"
        "</html>", CGSizeMake(208, 300));
}

TEST(WKScrollGeometry, NestedDivsWithMargin)
{
    runContentSizeTest(@""
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<style>"
        "    #outer { background: red; width: 200px; height: 200px; }"
        "    #inner { background: blue; width: 200px; height: 200px; margin: 8px 8px 8px 8px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div id='outer'><div id='inner'></div></div>"
        "</body>"
        "</html>", CGSizeMake(216, 216));
}

TEST(WKScrollGeometry, NestedDivsWithAbsolutePositioningAndClipOnOutermostDiv)
{
    runContentSizeTest(@""
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<style>"
        "    html { height: 200px; overflow: clip; }"
        "    #outer { background: red; width: 20px; height: 300px; position: absolute; overflow: clip; }"
        "    #inner { background: blue; height: 10000px; width: 20px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div id='outer'>Outer<div id='inner'>Inner</div></div>"
        "</body>"
        "</html>", CGSizeMake(28, 308));
}

TEST(WKScrollGeometry, NestedDivsWithAbsolutePositioningAndClipOnInnermostDiv)
{
    runContentSizeTest(@""
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<style>"
        "    #outer { background: red; width: 400px; height: 400px; overflow: clip; }"
        "    #inner { background: blue; width: 500px; height: 600px; }"
        "    #inner-inner { background: green; width: 700px; height: 600px; position: absolute; top: 5px; left: 100px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div id='outer'>Test<div id='inner'>B<div id='inner-inner'>C</div></div></div>"
        "</body>"
        "</html>", CGSizeMake(800, 605));
}

TEST(WKScrollGeometry, NestedDivsRelativePosition)
{
    runContentSizeTest(@""
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<style>"
        "    #outer { background: red; width: 400px; height: 400px; position: relative; }"
        "    #inner { background: blue; width: 500px; height: 600px; }"
        "    #inner-inner { background: green; width: 700px; height: 600px; position: absolute; top: 100px; left: 100px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div id='outer'>Test<div id='inner'>B<div id='inner-inner'>C</div></div></div>"
        "</body>"
        "</html>", CGSizeMake(808, 708));
}

TEST(WKScrollGeometry, NestedDivsRelativePositionAndClip)
{
    runContentSizeTest(@""
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<style>"
        "    #outer { background: red; width: 400px; height: 400px; overflow: clip; position: relative; }"
        "    #inner { background: blue; width: 500px; height: 600px; }"
        "    #inner-inner { background: green; width: 700px; height: 600px; position: absolute; top: 100px; left: 100px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div id='outer'>Test<div id='inner'>B<div id='inner-inner'>C</div></div></div>"
        "</body>"
        "</html>", CGSizeMake(408, 408));
}

TEST(WKScrollGeometry, NestedDivsOverflowYClip)
{
    runContentSizeTest(@""
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<style>"
        "    #outer { background: red; width: 400px; height: 400px; overflow-y: clip; }"
        "    #inner { background: blue; width: 500px; height: 600px; }"
        "    #inner-inner { background: green; width: 700px; height: 600px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div id='outer'>Test<div id='inner'>B<div id='inner-inner'>C</div></div></div>"
        "</body>"
        "</html>", CGSizeMake(708, 416));
}

TEST(WKScrollGeometry, NestedDivsOverflowYScroll)
{
    runContentSizeTest(@""
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<style>"
        "    #outer { width: 200px; height: 200px; overflow-y: scroll; }"
        "    #inner { background: red; height: 10000px; width: 400px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div id='outer'><div id='inner'></div></div>"
        "</body>"
        "</html>", CGSizeMake(208, 216));
}

TEST(WKScrollGeometry, FixedSizeBody)
{
    runContentSizeTest(@""
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<style>"
        "    body { width: 400px; height: 400px; }"
        "    #outer { background: red; width: 200px; height: 200px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div id='outer'><div id='inner'>Test</div></div>"
        "</body>"
        "</html>", CGSizeMake(408, 416));
}

TEST(WKScrollGeometry, MarginBottom)
{
    runContentSizeTest(@""
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<style>"
        "    #inner { background: red; width: 200px; height: 200px; margin-bottom: 50px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div id='outer'><div id='inner'>Test</div></div>"
        "</body>"
        "</html>", CGSizeMake(792, 258));
}

TEST(WKScrollGeometry, MarginBottomCollapsePrevented)
{
    runContentSizeTest(@""
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
        "<style>"
        "    #outer { border: 1px solid black; }"
        "    #inner { background: red; width: 200px; height: 200px; margin-bottom: 50px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div id='outer'><div id='inner'>Test</div></div>"
        "</body>"
        "</html>", CGSizeMake(792, 268));
}

#endif // ENABLE(SWIFTUI)
