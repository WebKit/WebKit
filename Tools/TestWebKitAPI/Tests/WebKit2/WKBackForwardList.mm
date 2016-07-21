/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "Test.h"

#import <WebKit/WKBackForwardListPrivate.h>
#import <WebKit/WKNavigationDelegate.h>
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKSessionState.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED

static bool isDone;

@interface WKBackForwardListTestNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation WKBackForwardListTestNavigationDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    isDone = true;
}

@end

static NSString *loadableURL1 = @"data:text/html,no%20error%20A";
static NSString *loadableURL2 = @"data:text/html,no%20error%20B";
static NSString *loadableURL3 = @"data:text/html,no%20error%20C";

TEST(WKBackForwardList, RemoveCurrentItem)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto controller = adoptNS([[WKBackForwardListTestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:controller.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL1]]];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL2]]];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL3]]];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ((NSUInteger)2, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, loadableURL3.UTF8String);

    _WKSessionState *sessionState = [webView _sessionStateWithFilter:^BOOL(WKBackForwardListItem *item)
    {
        return [item.URL isEqual:[NSURL URLWithString:loadableURL2]];
    }];

    [webView _restoreSessionState:sessionState andNavigate:NO];

    WKBackForwardList *newList = [webView backForwardList];

    EXPECT_EQ((NSUInteger)0, newList.backList.count);
    EXPECT_EQ((NSUInteger)0, newList.forwardList.count);
    EXPECT_STREQ([[newList.currentItem URL] absoluteString].UTF8String, loadableURL2.UTF8String);
}

#endif
