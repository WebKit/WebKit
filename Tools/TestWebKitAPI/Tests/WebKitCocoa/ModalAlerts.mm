/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferences.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

@class ModalAlertsUIDelegate;

static RetainPtr<TestWKWebView> openedWebView;
static RetainPtr<ModalAlertsUIDelegate> sharedUIDelegate;

static unsigned dialogsSeen;

// FIXME: Remove this guard once we fix <https://bugs.webkit.org/show_bug.cgi?id=172614>.
#if PLATFORM(MAC)
static const unsigned dialogsExpected = 4;
static const unsigned dialogsBeforeUnloadConfirmPanel = dialogsExpected - 1;
#else
static const unsigned dialogsExpected = 3;
#endif

static void sawDialog()
{
    ++dialogsSeen;

    // FIXME: Remove this guard once we fix <https://bugs.webkit.org/show_bug.cgi?id=172614>.
#if PLATFORM(MAC)
    if (dialogsSeen == dialogsBeforeUnloadConfirmPanel) {
        [openedWebView sendClicksAtPoint:NSMakePoint(5, 5) numberOfClicks:1];
        [openedWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"data:text/html"]]];
        return;
    }
#endif

    if (dialogsSeen == dialogsExpected)
        isDone = true;
}

@interface ModalAlertsUIDelegate : NSObject <WKUIDelegate>
@end

@implementation ModalAlertsUIDelegate

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [openedWebView setUIDelegate:sharedUIDelegate.get()];
    return openedWebView.get();
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    EXPECT_STREQ([[[[frame request] URL] absoluteString] UTF8String], "about:blank");
    EXPECT_STREQ([[[frame securityOrigin] protocol] UTF8String], "file");
    EXPECT_STREQ([[[frame securityOrigin] host] UTF8String], "");
    EXPECT_EQ([[frame securityOrigin] port], 0);

    completionHandler();
    sawDialog();
}

- (void)webView:(WKWebView *)webView runJavaScriptConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL result))completionHandler
{
    EXPECT_STREQ([[[[frame request] URL] absoluteString] UTF8String], "about:blank");
    EXPECT_STREQ([[[frame securityOrigin] protocol] UTF8String], "file");
    EXPECT_STREQ([[[frame securityOrigin] host] UTF8String], "");
    EXPECT_EQ([[frame securityOrigin] port], 0);

    completionHandler(YES);
    sawDialog();
}

- (void)webView:(WKWebView *)webView runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSString *result))completionHandler
{
    EXPECT_STREQ([[[[frame request] URL] absoluteString] UTF8String], "about:blank");
    EXPECT_STREQ([[[frame securityOrigin] protocol] UTF8String], "file");
    EXPECT_STREQ([[[frame securityOrigin] host] UTF8String], "");
    EXPECT_EQ([[frame securityOrigin] port], 0);

    completionHandler(@"");
    sawDialog();
}

// FIXME: Remove this guard once we fix <https://bugs.webkit.org/show_bug.cgi?id=172614>.
#if PLATFORM(MAC)
- (void)_webView:(WKWebView *)webView runBeforeUnloadConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL result))completionHandler
{
    EXPECT_STREQ([[[[frame request] URL] absoluteString] UTF8String], "about:blank");
    EXPECT_STREQ([[[frame securityOrigin] protocol] UTF8String], "file");
    EXPECT_STREQ([[[frame securityOrigin] host] UTF8String], "");
    EXPECT_EQ([[frame securityOrigin] port], 0);

    completionHandler(NO);
    sawDialog();
}
#endif

@end

TEST(WebKit, ModalAlerts)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    sharedUIDelegate = adoptNS([[ModalAlertsUIDelegate alloc] init]);
    [webView setUIDelegate:sharedUIDelegate.get()];

    [webView configuration].preferences.javaScriptCanOpenWindowsAutomatically = YES;

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"modal-alerts-in-new-about-blank-window" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&isDone);
}

#if PLATFORM(MAC)

static bool shouldRejectClosingViaPrompt = false;
static bool didRespondToPrompt = false;

@interface SlowBeforeUnloadPromptUIDelegate : NSObject <WKUIDelegate>
@end

@implementation SlowBeforeUnloadPromptUIDelegate

- (void)_webView:(WKWebView *)webView runBeforeUnloadConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL result))completionHandler
{
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.5 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        completionHandler(shouldRejectClosingViaPrompt ? NO : YES);
        didRespondToPrompt = true;
    });
}

- (void)webViewDidClose:(WKWebView *)webView
{
    [webView _close];
}

@end

TEST(WebKit, SlowBeforeUnloadPromptReject)
{
    auto slowBeforeUnloadPromptUIDelegate = adoptNS([[SlowBeforeUnloadPromptUIDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setUIDelegate:slowBeforeUnloadPromptUIDelegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"beforeunload"];

    TestWebKitAPI::Util::spinRunLoop(10);

    // Need a user gesture on the page before being allowed to show the beforeunload prompt.
    [webView sendClicksAtPoint:NSMakePoint(5, 5) numberOfClicks:1];

    TestWebKitAPI::Util::spinRunLoop(10);

    shouldRejectClosingViaPrompt = true;
    [webView _tryClose];

    TestWebKitAPI::Util::run(&didRespondToPrompt);
    EXPECT_FALSE([webView _isClosed]);

    TestWebKitAPI::Util::sleep(0.1);
    EXPECT_FALSE([webView _isClosed]);
}

TEST(WebKit, SlowBeforeUnloadPromptAllow)
{
    auto slowBeforeUnloadPromptUIDelegate = adoptNS([[SlowBeforeUnloadPromptUIDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setUIDelegate:slowBeforeUnloadPromptUIDelegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"beforeunload"];

    TestWebKitAPI::Util::spinRunLoop(10);

    // Need a user gesture on the page before being allowed to show the beforeunload prompt.
    [webView sendClicksAtPoint:NSMakePoint(5, 5) numberOfClicks:1];

    TestWebKitAPI::Util::spinRunLoop(10);

    shouldRejectClosingViaPrompt = false;
    [webView _tryClose];

    TestWebKitAPI::Util::run(&didRespondToPrompt);

    while (![webView _isClosed])
        TestWebKitAPI::Util::sleep(0.1);
}

TEST(WebKit, BeforeUnloadPromptRejectOnReload)
{
    auto slowBeforeUnloadPromptUIDelegate = adoptNS([[SlowBeforeUnloadPromptUIDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setUIDelegate:slowBeforeUnloadPromptUIDelegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"beforeunload"];

    TestWebKitAPI::Util::spinRunLoop(10);

    // Need a user gesture on the page before being allowed to show the beforeunload prompt.
    [webView sendClicksAtPoint:NSMakePoint(5, 5) numberOfClicks:1];

    TestWebKitAPI::Util::spinRunLoop(10);

    shouldRejectClosingViaPrompt = true;
    [webView reload];

    TestWebKitAPI::Util::run(&didRespondToPrompt);

    EXPECT_FALSE([webView _isClosed]);

    TestWebKitAPI::Util::sleep(0.1);
    EXPECT_FALSE([webView _isClosed]);
}

TEST(WebKit, BeforeUnloadPromptAllowOnReload)
{
    auto slowBeforeUnloadPromptUIDelegate = adoptNS([[SlowBeforeUnloadPromptUIDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setUIDelegate:slowBeforeUnloadPromptUIDelegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"beforeunload"];

    TestWebKitAPI::Util::spinRunLoop(10);

    // Need a user gesture on the page before being allowed to show the beforeunload prompt.
    [webView sendClicksAtPoint:NSMakePoint(5, 5) numberOfClicks:1];

    TestWebKitAPI::Util::spinRunLoop(10);

    shouldRejectClosingViaPrompt = false;
    [webView reload];

    TestWebKitAPI::Util::run(&didRespondToPrompt);

    EXPECT_FALSE([webView _isClosed]);

    TestWebKitAPI::Util::sleep(0.1);
    EXPECT_FALSE([webView _isClosed]);
}

static bool webViewDidCloseCalled = false;
static unsigned viewDidCloseCallCount = 0;

@interface SlowBeforeUnloadHandlerUIDelegate : NSObject <WKUIDelegate>
@end

@implementation SlowBeforeUnloadHandlerUIDelegate

- (void)_webView:(WKWebView *)webView runBeforeUnloadConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL result))completionHandler
{
    completionHandler(YES);
}

- (void)webViewDidClose:(WKWebView *)webView
{
    ++viewDidCloseCallCount;
    webViewDidCloseCalled = true;
}

@end

TEST(WebKit, SlowBeforeUnloadHandlerSingleClosePageCall)
{
    auto slowBeforeUnloadHandlerUIDelegate = adoptNS([[SlowBeforeUnloadHandlerUIDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setUIDelegate:slowBeforeUnloadHandlerUIDelegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"beforeunload-slow"];

    TestWebKitAPI::Util::spinRunLoop(10);

    // Need a user gesture on the page before being allowed to show the beforeunload prompt.
    [webView sendClicksAtPoint:NSMakePoint(5, 5) numberOfClicks:1];

    TestWebKitAPI::Util::spinRunLoop(100);

    [webView _tryClose];

    TestWebKitAPI::Util::run(&webViewDidCloseCalled);
    EXPECT_EQ(1U, viewDidCloseCallCount);
    EXPECT_FALSE([webView _isClosed]);

    TestWebKitAPI::Util::sleep(0.2);

    EXPECT_EQ(1U, viewDidCloseCallCount);
    EXPECT_FALSE([webView _isClosed]);
}

#endif
