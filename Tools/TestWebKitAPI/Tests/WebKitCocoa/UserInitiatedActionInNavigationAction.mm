/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "config.h"

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "Test.h"
#import <WebKit/WKNavigationActionPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKUserInitiatedAction.h>
#import <wtf/RetainPtr.h>
#import <wtf/mac/AppKitCompatibilityDeclarations.h>

static bool finishedNavigation;

@interface UserInitiatedActionInNavigationActionDelegate : NSObject <WKNavigationDelegate>
@property (nonatomic, copy, nullable) void (^policyForNavigationAction)(WKNavigationAction *, void (^)(WKNavigationActionPolicy));
@end

@implementation UserInitiatedActionInNavigationActionDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    if (_policyForNavigationAction) {
        _policyForNavigationAction(navigationAction, decisionHandler);
        return;
    }

    decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    finishedNavigation = true;
}

@end

class UserInitiatedActionTest : public testing::Test {
public:
    RetainPtr<WKWebView> webView;
    RetainPtr<NSWindow> window;
    RetainPtr<UserInitiatedActionInNavigationActionDelegate> delegate;
    RetainPtr<NSURL> URL;

    virtual void SetUp()
    {
        webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

        window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
        [[window contentView] addSubview:webView.get()];

        delegate = adoptNS([[UserInitiatedActionInNavigationActionDelegate alloc] init]);
        [webView setNavigationDelegate:delegate.get()];

        URL = [[NSBundle mainBundle] URLForResource:@"open-multiple-external-url" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    }

    NSURL *URLWithFragment(NSString *fragment)
    {
        NSURLComponents *URLComponents = [[NSURLComponents alloc] initWithURL:URL.get() resolvingAgainstBaseURL:NO];
        URLComponents.fragment = fragment;
        return URLComponents.URL;
    }

    void loadTest(NSString *test)
    {
        [webView loadRequest:[NSURLRequest requestWithURL:URLWithFragment(test)]];
        TestWebKitAPI::Util::run(&finishedNavigation);
        finishedNavigation = false;
    }

    void click()
    {
        NSPoint clickPoint = NSMakePoint(100, 100);

        [[webView hitTest:clickPoint] mouseDown:[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
        [[webView hitTest:clickPoint] mouseUp:[NSEvent mouseEventWithType:NSEventTypeLeftMouseUp location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    }
};

TEST_F(UserInitiatedActionTest, MailtoInNormalLoop)
{
    loadTest(@"normalLoop");

    __block _WKUserInitiatedAction *initialUserInitiatedAction = nil;
    __block unsigned count = 0;
    __block bool finishedPolicy = false;
    delegate.get().policyForNavigationAction = ^(WKNavigationAction *navigationAction, void (^decisionHandler)(WKNavigationActionPolicy)) {
        if ([navigationAction.request.URL.scheme isEqualToString:@"about"]) {
            decisionHandler(WKNavigationActionPolicyAllow);
            return;
        }
        
        EXPECT_WK_STREQ(@"mailto", navigationAction.request.URL.scheme);
        EXPECT_FALSE(navigationAction._canHandleRequest);
        
        if (count == 0) {
            EXPECT_TRUE(navigationAction._isUserInitiated);
            EXPECT_NOT_NULL(navigationAction._userInitiatedAction);
            EXPECT_FALSE(navigationAction._userInitiatedAction.consumed);

            initialUserInitiatedAction = navigationAction._userInitiatedAction;

            [navigationAction._userInitiatedAction consume];
        } else {
            EXPECT_TRUE(navigationAction._isUserInitiated);
            EXPECT_EQ(initialUserInitiatedAction, navigationAction._userInitiatedAction);
            EXPECT_TRUE(navigationAction._userInitiatedAction.consumed);
        }

        if (++count == 3)
            finishedPolicy = true;
    
        decisionHandler(WKNavigationActionPolicyCancel);
    };

    click();

    TestWebKitAPI::Util::run(&finishedPolicy);
}

TEST_F(UserInitiatedActionTest, MailtoInLoopAfterTimer)
{
    loadTest(@"loopAfterTimer");

    __block _WKUserInitiatedAction *initialUserInitiatedAction = nil;
    __block unsigned count = 0;
    __block bool finishedPolicy = false;
    delegate.get().policyForNavigationAction = ^(WKNavigationAction *navigationAction, void (^decisionHandler)(WKNavigationActionPolicy)) {
        if ([navigationAction.request.URL.scheme isEqualToString:@"about"]) {
            decisionHandler(WKNavigationActionPolicyAllow);
            return;
        }

        EXPECT_WK_STREQ(@"mailto", navigationAction.request.URL.scheme);
        EXPECT_FALSE(navigationAction._canHandleRequest);
        
        if (count == 0) {
            EXPECT_TRUE(navigationAction._isUserInitiated);
            EXPECT_NOT_NULL(navigationAction._userInitiatedAction);
            EXPECT_FALSE(navigationAction._userInitiatedAction.consumed);

            initialUserInitiatedAction = navigationAction._userInitiatedAction;

            [navigationAction._userInitiatedAction consume];
        } else {
            EXPECT_TRUE(navigationAction._isUserInitiated);
            EXPECT_EQ(initialUserInitiatedAction, navigationAction._userInitiatedAction);
            EXPECT_TRUE(navigationAction._userInitiatedAction.consumed);
        }

        if (++count == 3)
            finishedPolicy = true;
    
        decisionHandler(WKNavigationActionPolicyCancel);
    };

    click();

    TestWebKitAPI::Util::run(&finishedPolicy);
}

TEST_F(UserInitiatedActionTest, MailtoInLoopAfterPostMessage)
{
    loadTest(@"loopAfterPostMessage");

    __block _WKUserInitiatedAction *initialUserInitiatedAction = nil;
    __block unsigned count = 0;
    __block bool finishedPolicy = false;
    delegate.get().policyForNavigationAction = ^(WKNavigationAction *navigationAction, void (^decisionHandler)(WKNavigationActionPolicy)) {
        if ([navigationAction.request.URL.scheme isEqualToString:@"about"]) {
            decisionHandler(WKNavigationActionPolicyAllow);
            return;
        }

        EXPECT_WK_STREQ(@"mailto", navigationAction.request.URL.scheme);
        EXPECT_FALSE(navigationAction._canHandleRequest);
        
        if (count == 0) {
            EXPECT_TRUE(navigationAction._isUserInitiated);
            EXPECT_NOT_NULL(navigationAction._userInitiatedAction);
            EXPECT_FALSE(navigationAction._userInitiatedAction.consumed);

            initialUserInitiatedAction = navigationAction._userInitiatedAction;

            [navigationAction._userInitiatedAction consume];
        } else {
            EXPECT_TRUE(navigationAction._isUserInitiated);
            EXPECT_EQ(initialUserInitiatedAction, navigationAction._userInitiatedAction);
            EXPECT_TRUE(navigationAction._userInitiatedAction.consumed);
        }

        if (++count == 3)
            finishedPolicy = true;
    
        decisionHandler(WKNavigationActionPolicyCancel);
    };

    click();

    TestWebKitAPI::Util::run(&finishedPolicy);
}

TEST_F(UserInitiatedActionTest, MailtoInLoopAfterLongTimer)
{
    loadTest(@"loopAfterLongTimer");

    __block unsigned count = 0;
    __block bool finishedPolicy = false;
    delegate.get().policyForNavigationAction = ^(WKNavigationAction *navigationAction, void (^decisionHandler)(WKNavigationActionPolicy)) {
        if ([navigationAction.request.URL.scheme isEqualToString:@"about"]) {
            decisionHandler(WKNavigationActionPolicyAllow);
            return;
        }

        EXPECT_WK_STREQ(@"mailto", navigationAction.request.URL.scheme);
        EXPECT_FALSE(navigationAction._canHandleRequest);
        
        EXPECT_FALSE(navigationAction._isUserInitiated);
        EXPECT_NULL(navigationAction._userInitiatedAction);

        if (++count == 3)
            finishedPolicy = true;
    
        decisionHandler(WKNavigationActionPolicyCancel);
    };

    click();

    TestWebKitAPI::Util::run(&finishedPolicy);
}

#endif
