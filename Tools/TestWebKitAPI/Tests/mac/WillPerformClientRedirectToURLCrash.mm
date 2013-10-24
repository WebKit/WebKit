/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "WTFStringUtilities.h"

#import <wtf/RetainPtr.h>

static bool testFinished;

static NSURL *testURL()
{
    static RetainPtr<NSURL> url = [[NSBundle mainBundle] URLForResource:@"WillPerformClientRedirectToURLCrash" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    return url.get();
}

@interface WebKit1TestDelegate : NSObject

+ (WebKit1TestDelegate *)shared;

@end

@implementation WebKit1TestDelegate

+ (WebKit1TestDelegate *)shared
{
    static WebKit1TestDelegate *sharedTestDelegate = [[WebKit1TestDelegate alloc] init];
    return sharedTestDelegate;
}

// MARK: WebFrameLoadDelegate callbacks

- (void)webView:(WebView *)webView willPerformClientRedirectToURL:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date forFrame:(WebFrame *)frame
{
    // Start a new load; canceling the scheduled redirect. Should not cause a crash.
    NSString *url = [NSString stringWithFormat:@"%@?PASS", testURL()];
    [frame loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:url]]];
}

// MARK: WebUIDelegate callbacks

-(void)webView:(WebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame
{
    EXPECT_EQ(String("PASS"), String(message));
    testFinished = true;
}

@end

namespace TestWebKitAPI {

TEST(WebKit1, WillPerformClientRedirectToURLCrash)
{
    @autoreleasepool {
        RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
        [webView setFrameLoadDelegate: [WebKit1TestDelegate shared]];
        [webView setUIDelegate:[WebKit1TestDelegate shared]];
        [[webView mainFrame] loadRequest:[NSURLRequest requestWithURL:testURL()]];
        Util::run(&testFinished);
    }
}

} // namespace TestWebKitAPI
