/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "TestNavigationDelegate.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKResourceLoadDelegate.h>
#import <wtf/RetainPtr.h>

@interface TestResourceLoadDelegate : NSObject <_WKResourceLoadDelegate>

@property (nonatomic, copy) void (^willSendRequest)(WKWebView *, NSURLRequest *);

@end

@implementation TestResourceLoadDelegate

- (void)webView:(WKWebView *)webView willSendRequest:(NSURLRequest *)request
{
    if (_willSendRequest)
        _willSendRequest(webView, request);
}

@end

TEST(ResourceLoadDelegate, Basic)
{
    auto webView = adoptNS([WKWebView new]);

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    __block bool done = false;
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        done = true;
    }];

    __block RetainPtr<NSURLRequest> requestFromDelegate;
    auto resourceLoadDelegate = adoptNS([TestResourceLoadDelegate new]);
    [webView _setResourceLoadDelegate:resourceLoadDelegate.get()];
    [resourceLoadDelegate setWillSendRequest:^(WKWebView *, NSURLRequest *request) {
        requestFromDelegate = request;
    }];

    RetainPtr<NSURLRequest> requestLoaded = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:requestLoaded.get()];
    TestWebKitAPI::Util::run(&done);
    
    EXPECT_WK_STREQ(requestLoaded.get().URL.absoluteString, requestFromDelegate.get().URL.absoluteString);
}
