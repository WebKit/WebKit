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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

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

TEST(ScrollingCoordinatorTests, ScrollingTreeAfterDetachReattach)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 500, 500) configuration:configuration addToWindow:YES]);
    
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
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
