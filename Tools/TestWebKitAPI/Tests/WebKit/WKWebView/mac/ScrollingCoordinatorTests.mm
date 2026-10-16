/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

@interface WKWebView (Internal)

- (void)_setContentOffsetX:(NSNumber *)x y:(NSNumber *)y animated:(BOOL)animated;

@end

namespace TestWebKitAPI {

static float waitForScrollEventAndReturnScrollY(WKWebView* webView, const std::function<void(WKWebView*)>& scrollTrigger)
{
    bool receivedScrollEvent = false;
    RetainPtr<id> evalResult;
    RetainPtr<NSError> strongError;
    
    NSString *scriptString = @"return new Promise((resolve) => {" \
        "   let iframe = document.getElementsByTagName('iframe')[0];" \
        "   iframe.contentWindow.addEventListener(\"scroll\", (event) => resolve(iframe.contentWindow.scrollY)); " \
        "})";

    [webView callAsyncJavaScript:scriptString arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
        evalResult = result;
        strongError = error;
        receivedScrollEvent = true;
    }];

    scrollTrigger(webView);

    TestWebKitAPI::Util::run(&receivedScrollEvent);

    return [evalResult floatValue];
}

// Remove "last committed scroll position" for the iframe's node, which we expect to be different.
static NSString *scrollingTreeElidingLastCommittedScrollPosition(NSString *scrollingTree)
{
    NSMutableArray *lines = [[[scrollingTree componentsSeparatedByCharactersInSet:[NSCharacterSet newlineCharacterSet]] mutableCopy] autorelease];
    
    NSIndexSet* lastCommittedLineIndices = [lines indexesOfObjectsPassingTest:^BOOL(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        return [obj hasPrefix:@"        (last committed scroll position"];
    }];
    [lines removeObjectsAtIndexes:lastCommittedLineIndices];
    return [lines componentsJoinedByString:@"\n"];
}

// FIXME when webkit.org/b/308786 is resolved.
TEST(ScrollingCoordinatorTests, DISABLED_ScrollingTreeAfterDetachReattach)
{
    [[NSUserDefaults standardUserDefaults] setObject:@"Scrolling" forKey:@"WebCoreLogging"];
    [[NSUserDefaults standardUserDefaults] setObject:@"Scrolling" forKey:@"WebKit2Logging"];

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 500, 500) configuration:configuration addToWindow:YES]);
    
    NSString *documentString = @"<style>body { height: 5000px; }</style>" \
        "<iframe srcdoc=\"<style>body { height: 5000px; }</style><div style='position:fixed; width: 100px; height: 50px; background: blue'></div>\"></iframe>";
    
    [webView synchronouslyLoadHTMLString:documentString];
    [webView waitForNextPresentationUpdate];

    CGPoint eventLocationInWindow = [webView convertPoint:CGPointMake(50, 50) toView:nil];

    auto scrollY = waitForScrollEventAndReturnScrollY(webView.get(), [eventLocationInWindow](TestWKWebView *webView) {
        [webView wheelEventAtPoint:eventLocationInWindow wheelDelta:CGSizeMake(0, -100)];
    });
    EXPECT_EQ(scrollY, 100);

    // Send a second wheel event to trigger reconcileScrollingState() that does not end up in setScrollingNodeScrollableAreaGeometry() (some compositing code early returns).
    // This would leave the scrolling state tree with a stale m_scrollPosition.
    scrollY = waitForScrollEventAndReturnScrollY(webView.get(), [eventLocationInWindow](TestWKWebView *webView) {
        [webView wheelEventAtPoint:eventLocationInWindow wheelDelta:CGSizeMake(0, -100)];
    });
    EXPECT_EQ(scrollY, 200);

    [webView waitForNextPresentationUpdate];

    NSString *scrollingTreeBefore = scrollingTreeElidingLastCommittedScrollPosition([webView stringByEvaluatingJavaScript:@"internals.scrollingTreeAsText()"]);

    NSWindow *hostWindow = [webView window];
    [webView removeFromSuperview];
    [webView waitForNextPresentationUpdate];
    [[hostWindow contentView] addSubview:webView.get()];
    [webView waitForNextPresentationUpdate];

    NSString *scrollingTreeAfter = scrollingTreeElidingLastCommittedScrollPosition([webView stringByEvaluatingJavaScript:@"internals.scrollingTreeAsText()"]);

    EXPECT_TRUE([scrollingTreeBefore isEqualToString:scrollingTreeAfter]);

    scrollY = waitForScrollEventAndReturnScrollY(webView.get(), [eventLocationInWindow](TestWKWebView *webView) {
        [webView wheelEventAtPoint:eventLocationInWindow wheelDelta:CGSizeMake(0, -101)];
    });
    EXPECT_EQ(scrollY, 301);

    [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"WebCoreLogging"];
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"WebKit2Logging"];
}

static NSString *const latchedWheelGestureMarkup = @""
    "<style>"
    "body { margin: 0; }"
    "#outer { width: 400px; height: 400px; overflow: scroll; }"
    "#inner { width: 300px; height: 300px; overflow: scroll; }"
    ".tall { height: 2000px; }"
    "#cover { position: absolute; top: 0; left: 0; width: 400px; height: 400px; }"
    "</style>"
    "<div id='outer'><div id='inner'><div id='innerContent' class='tall'></div></div><div class='tall'></div></div>"
    "<script>"
    "var lastWheelEventTarget = '';"
    "document.addEventListener('wheel', event => lastWheelEventTarget = event.target.id);"
    "</script>";

// Resolves once the page evaluates the condition to true. The page may handle a wheel event or
// update its scroll position some time after the UI process sends the event.
static void waitForCondition(WKWebView *webView, NSString *condition)
{
    bool done = false;
    RetainPtr script = [NSString stringWithFormat:@"return new Promise(resolve => { let check = () => (%@) ? resolve() : setTimeout(check, 10); check(); })", condition];
    [webView callAsyncJavaScript:script.get() arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&](id, NSError *) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

// EventHandler uses the latched state without a hit test for events that continue a latched gesture
// when it doesn't scroll on the main thread, which is the case with an async scrolling thread. This test
// checks that behavior stays the same for an element that covers the pointer after the gesture began.
TEST(ScrollingCoordinatorTests, LatchedWheelGestureIgnoresElementCoveringPointer)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    enableSiteIsolation(configuration.get());
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadHTMLString:latchedWheelGestureMarkup];
    [webView waitForNextPresentationUpdate];

    // The pointer is over #inner, which is inside #outer.
    CGPoint pointInWindow = [webView convertPoint:CGPointMake(150, 150) toView:nil];

    [webView wheelEventAtPoint:pointInWindow wheelDelta:CGSizeMake(0, -10) phase:kCGScrollPhaseBegan momentumPhase:kCGMomentumScrollPhaseNone];
    [webView wheelEventAtPoint:pointInWindow wheelDelta:CGSizeMake(0, -10) phase:kCGScrollPhaseChanged momentumPhase:kCGMomentumScrollPhaseNone];
    waitForCondition(webView.get(), @"document.getElementById('inner').scrollTop > 0");
    int innerScrollTop = [webView stringByEvaluatingJavaScript:@"document.getElementById('inner').scrollTop"].intValue;
    EXPECT_GT(innerScrollTop, 0);

    // A hit test after layout finds the cover. The latched element and scroller still get the rest
    // of the gesture.
    [webView stringByEvaluatingJavaScript:@""
        "lastWheelEventTarget = '';"
        "let cover = document.createElement('div');"
        "cover.id = 'cover';"
        "document.body.appendChild(cover);"
        "null;"];
    [webView wheelEventAtPoint:pointInWindow wheelDelta:CGSizeMake(0, -10) phase:kCGScrollPhaseChanged momentumPhase:kCGMomentumScrollPhaseNone];
    waitForCondition(webView.get(), [NSString stringWithFormat:@"lastWheelEventTarget !== '' && document.getElementById('inner').scrollTop > %d", innerScrollTop]);
    [webView wheelEventAtPoint:pointInWindow wheelDelta:CGSizeMake(0, 0) phase:kCGScrollPhaseEnded momentumPhase:kCGMomentumScrollPhaseNone];

    EXPECT_WK_STREQ(@"innerContent", [webView stringByEvaluatingJavaScript:@"lastWheelEventTarget"]);
    EXPECT_EQ(0, [webView stringByEvaluatingJavaScript:@"document.getElementById('outer').scrollTop"].intValue);
}

TEST(ScrollingCoordinatorTests, SetContentOffset)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 500, 500) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadTestPageNamed:@"simple-tall"];
    [webView waitForNextPresentationUpdate];

    [webView _setContentOffsetX:@0 y:@200 animated:NO];
    [webView waitForNextPresentationUpdate];

    RetainPtr scrollY = [webView stringByEvaluatingJavaScript:@"window.scrollY"];
    EXPECT_WK_STREQ("200", scrollY.get());

    [webView _setContentOffsetX:@0 y:nil animated:NO];
    [webView waitForNextPresentationUpdate];

    RetainPtr scrollY2 = [webView stringByEvaluatingJavaScript:@"window.scrollY"];
    EXPECT_WK_STREQ("200", scrollY2.get());
}

TEST(ScrollingCoordinatorTests, SetContentOffsetHorizontalRTL)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 500, 500) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadTestPageNamed:@"rtl-sideways-scrolling"];
    [webView waitForNextPresentationUpdate];

    [webView _setContentOffsetX:@0 y:@0 animated:NO];
    [webView waitForNextPresentationUpdate];

    RetainPtr scrollX = [webView stringByEvaluatingJavaScript:@"window.scrollX"];
    EXPECT_WK_STREQ("-1508", scrollX.get());
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
