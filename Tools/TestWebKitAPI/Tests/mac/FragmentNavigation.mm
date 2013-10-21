/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#import "PlatformUtilities.h"

#import <wtf/RetainPtr.h>

static bool testFinished;

@interface WebKit1FragmentNavigationTestDelegate : NSObject {
    unsigned _stage;
}

+ (WebKit1FragmentNavigationTestDelegate *)shared;

@end

@implementation WebKit1FragmentNavigationTestDelegate

+ (WebKit1FragmentNavigationTestDelegate *)shared
{
    static WebKit1FragmentNavigationTestDelegate *sharedTestDelegate = [[WebKit1FragmentNavigationTestDelegate alloc] init];
    return sharedTestDelegate;
}

-(void)webView:(WebView *)sender runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame
{
    // Any alerts mean that the hashchange event was dispatched. This means that the test failed.
    EXPECT_TRUE(false);
}

- (void)webView:(WebView *)webView decidePolicyForNavigationAction:(NSDictionary *)actionInformation request:(NSURLRequest *)request frame:(WebFrame *)frame decisionListener:(id<WebPolicyDecisionListener>)listener
{
    NSString *fragment = request.URL.fragment;
    if (!fragment) {
        [listener use];
        return;
    }

    [listener ignore];

    dispatch_async(dispatch_get_main_queue(), ^{
        [self _runNextTestWithWebView:webView];
    });
}

- (void)webView:(WebView *)webView didFinishLoadForFrame:(WebFrame *)frame
{
    if (frame != webView.mainFrame)
        return;

    EXPECT_FALSE(frame.dataSource.request.URL.fragment);

    [self _runNextTestWithWebView:webView];
}

- (void)_runNextTestWithWebView:(WebView *)webView
{
    switch (_stage++) {
    case 0:
    case 1: {
        // Normal fragment load, twice.
        NSURL *fragmentURL = [NSURL URLWithString:[NSString stringWithFormat:@"%@#fragment", webView.mainFrame.dataSource.request.URL.absoluteString]];
        [webView.mainFrame loadRequest:[NSURLRequest requestWithURL:fragmentURL]];
        break;
    }

    case 2:
    case 3: {
        // Setting location.hash explicitly, twice.
        [webView stringByEvaluatingJavaScriptFromString:@"location.hash='#hash'"];
        break;
    }

    case 4:
        // We're done.
        testFinished = true;
        break;

    default:
        EXPECT_TRUE(false);
        break;
    }
}

@end

namespace TestWebKitAPI {

TEST(WebKit1, FragmentNavigation)
{
    @autoreleasepool {
        RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);

        [webView setPolicyDelegate:[WebKit1FragmentNavigationTestDelegate shared]];
        [webView setFrameLoadDelegate:[WebKit1FragmentNavigationTestDelegate shared]];
        [webView setUIDelegate:[WebKit1FragmentNavigationTestDelegate shared]];
        [[webView mainFrame] loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"FragmentNavigation" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

        Util::run(&testFinished);
    }
}

} // namespace TestWebKitAPI
