/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "PlatformWebView.h"
#import <WebKit/DOMPrivate.h>
#import <WebKit/WebViewPrivate.h>
#import <wtf/RetainPtr.h>

@interface LimitTitleSizeTest : NSObject <WebFrameLoadDelegate>
@end

static bool waitUntilLongTitleReceived = false;
static bool didFinishLoad = false;

@implementation LimitTitleSizeTest

static size_t maxTitleLength = 4096;

- (void)webView:(WebView *)sender didReceiveTitle:(NSString *)title forFrame:(WebFrame *)frame
{
    if ([title isEqualToString:@"Original Short Title"])
        return;
    
    EXPECT_LE(title.length, maxTitleLength);
    waitUntilLongTitleReceived = true;
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}
@end

TEST(WebKitLegacy, LimitTitleSize)
{
    RetainPtr<WebView> webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 120, 200) frameName:nil groupName:nil]);
    RetainPtr<LimitTitleSizeTest> testController = adoptNS([LimitTitleSizeTest new]);

    webView.get().frameLoadDelegate = testController.get();
    [[webView.get() mainFrame] loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle]
        URLForResource:@"set-long-title" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    TestWebKitAPI::Util::run(&didFinishLoad);
}

@interface LimitTitleSizeTestObserver : NSObject
@end

@implementation LimitTitleSizeTestObserver

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void*)context
{
    if ([[object title] isEqualToString:@"Original Short Title"])
        return;
    EXPECT_LE([object title].length, maxTitleLength);
    waitUntilLongTitleReceived = true;
}

@end

TEST(WebKit, LimitTitleSize)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"set-long-title" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    
    auto observer = adoptNS([LimitTitleSizeTestObserver new]);
    [webView addObserver:observer.get() forKeyPath:@"title" options:NSKeyValueObservingOptionNew context:nil];
    
    TestWebKitAPI::Util::run(&waitUntilLongTitleReceived);
    [webView removeObserver:observer.get() forKeyPath:@"title"];
}
