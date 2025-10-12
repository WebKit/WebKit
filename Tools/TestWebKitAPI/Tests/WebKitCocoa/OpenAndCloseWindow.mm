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

#import "config.h"

#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKFrameInfo.h>
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKNavigationActionPrivate.h>
#import <WebKit/WKPreferences.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWindowFeaturesPrivate.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <wtf/RetainPtr.h>
#import <wtf/darwin/DispatchExtras.h>

@class OpenAndCloseWindowUIDelegate;
@class OpenAndCloseWindowUIDelegateAsync;
@class CheckWindowFeaturesUIDelegate;

static RetainPtr<WKWebView> openedWebView;
static RetainPtr<WKWindowFeatures> openWindowFeatures;
static RetainPtr<OpenAndCloseWindowUIDelegate> sharedUIDelegate;
static RetainPtr<OpenAndCloseWindowUIDelegateAsync> sharedUIDelegateAsync;
static RetainPtr<CheckWindowFeaturesUIDelegate> sharedCheckWindowFeaturesUIDelegate;

static void resetToConsistentState()
{
    isDone = false;
    openedWebView = nil;
    sharedUIDelegate = nil;
    sharedUIDelegateAsync = nil;
    sharedCheckWindowFeaturesUIDelegate = nil;
}

@interface OpenAndCloseWindowUIDelegate : NSObject <WKUIDelegate>
@property (nonatomic, assign) WKWebView *expectedClosingView;
@end

@implementation OpenAndCloseWindowUIDelegate

- (void)webViewDidClose:(WKWebView *)webView
{
    EXPECT_EQ(_expectedClosingView, webView);
    isDone = true;
}

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    openedWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [openedWebView setUIDelegate:sharedUIDelegate.get()];
    _expectedClosingView = openedWebView.get();
    return openedWebView.get();
}

@end

TEST(WebKit, OpenAndCloseWindow)
{
    resetToConsistentState();

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    sharedUIDelegate = adoptNS([[OpenAndCloseWindowUIDelegate alloc] init]);
    [webView setUIDelegate:sharedUIDelegate.get()];

    [webView configuration].preferences.javaScriptCanOpenWindowsAutomatically = YES;

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"open-and-close-window" withExtension:@"html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&isDone);
}

@interface OpenAndCloseWindowUIDelegateAsync : OpenAndCloseWindowUIDelegate
@property (nonatomic) BOOL shouldCallback;
@property (nonatomic, assign) id savedCompletionHandler;
@property (nonatomic) BOOL shouldCallbackWithNil;
@end

@implementation OpenAndCloseWindowUIDelegateAsync

- (void)dealloc
{
    [_savedCompletionHandler release];
    [super dealloc];
}

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    ASSERT_NOT_REACHED();
    return nil;
}

- (void)_webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures completionHandler:(void (^)(WKWebView *webView))completionHandler
{
    if (_shouldCallback) {
        if (!_shouldCallbackWithNil) {
            dispatch_async(mainDispatchQueueSingleton(), ^{
                openedWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
                [openedWebView setUIDelegate:sharedUIDelegateAsync.get()];
                self.expectedClosingView = openedWebView.get();
                completionHandler(openedWebView.get());
            });
        } else {
            dispatch_async(mainDispatchQueueSingleton(), ^{
                self.expectedClosingView = webView;
                completionHandler(nil);
            });
        }
        return;
    }

    _savedCompletionHandler = [completionHandler copy];
    isDone = true;
}

@end

TEST(WebKit, OpenAndCloseWindowAsync)
{
    resetToConsistentState();

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    sharedUIDelegateAsync = adoptNS([[OpenAndCloseWindowUIDelegateAsync alloc] init]);
    sharedUIDelegateAsync.get().shouldCallback = YES;
    [webView setUIDelegate:sharedUIDelegateAsync.get()];

    [webView configuration].preferences.javaScriptCanOpenWindowsAutomatically = YES;

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"open-and-close-window" withExtension:@"html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(WebKit, OpenAsyncWithNil)
{
    resetToConsistentState();

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    sharedUIDelegateAsync = adoptNS([[OpenAndCloseWindowUIDelegateAsync alloc] init]);
    sharedUIDelegateAsync.get().shouldCallback = YES;
    sharedUIDelegateAsync.get().shouldCallbackWithNil = YES;
    [webView setUIDelegate:sharedUIDelegateAsync.get()];

    [webView configuration].preferences.javaScriptCanOpenWindowsAutomatically = YES;

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"open-and-close-window" withExtension:@"html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&isDone);
}

// https://bugs.webkit.org/show_bug.cgi?id=171083 - Try to figure out why this fails for some configs but not others, and resolve.
//TEST(WebKit, OpenAndCloseWindowAsyncCallbackException)
//{
//    resetToConsistentState();
//
//    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
//
//    sharedUIDelegateAsync = adoptNS([[OpenAndCloseWindowUIDelegateAsync alloc] init]);
//    sharedUIDelegateAsync.get().shouldCallback = NO;
//    [webView setUIDelegate:sharedUIDelegateAsync.get()];
//
//    [webView configuration].preferences.javaScriptCanOpenWindowsAutomatically = YES;
//
//    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"open-and-close-window" withExtension:@"html"]];
//    [webView loadRequest:request];
//
//    TestWebKitAPI::Util::run(&isDone);
//
//    bool caughtException = false;
//    @try {
//        sharedUIDelegateAsync = nil;
//        openedWebView = nil;
//        webView = nil;
//    }
//    @catch (NSException *) {
//        caughtException = true;
//    }
//
//    EXPECT_EQ(caughtException, true);
//}


@interface CheckWindowFeaturesUIDelegate : NSObject <WKUIDelegate>

@property (nonatomic, readonly) NSNumber *menuBarVisibility;
@property (nonatomic, readonly) NSNumber *statusBarVisibility;
@property (nonatomic, readonly) NSNumber *toolbarsVisibility;

@end

@implementation CheckWindowFeaturesUIDelegate

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    openWindowFeatures = windowFeatures;
    isDone = true;

    return nil;
}

@end

TEST(WebKit, OpenWindowFeatures)
{
    resetToConsistentState();

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    sharedCheckWindowFeaturesUIDelegate = adoptNS([[CheckWindowFeaturesUIDelegate alloc] init]);
    [webView setUIDelegate:sharedCheckWindowFeaturesUIDelegate.get()];
    [webView configuration].preferences.javaScriptCanOpenWindowsAutomatically = YES;
    constexpr NSString *windowOpenFormatString = @"window.open(\"about:blank\", \"_blank\", \"%@\")";

    [webView evaluateJavaScript:@"window.open(\"about:blank\")" completionHandler:nil];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    EXPECT_FALSE([openWindowFeatures _wantsPopup]);
    EXPECT_FALSE([openWindowFeatures _hasAdditionalFeatures]);
    EXPECT_TRUE([openWindowFeatures menuBarVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures statusBarVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures toolbarsVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures allowsResizing] == nil);
    EXPECT_TRUE([openWindowFeatures _popup] == nil);
    EXPECT_TRUE([openWindowFeatures _locationBarVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures _scrollbarsVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures _fullscreenDisplay] == nil);
    EXPECT_TRUE([openWindowFeatures _dialogDisplay] == nil);
    openWindowFeatures = nullptr;

    NSString *featuresStringOnlyNonPopupSpecifiedAndTrue = @"noopener=true,noreferrer=true";
    [webView evaluateJavaScript:[NSString stringWithFormat:windowOpenFormatString, featuresStringOnlyNonPopupSpecifiedAndTrue] completionHandler:nil];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    EXPECT_FALSE([openWindowFeatures _wantsPopup]);
    EXPECT_FALSE([openWindowFeatures _hasAdditionalFeatures]);
    EXPECT_TRUE([openWindowFeatures menuBarVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures statusBarVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures toolbarsVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures allowsResizing] == nil);
    EXPECT_TRUE([openWindowFeatures _popup] == nil);
    EXPECT_TRUE([openWindowFeatures _locationBarVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures _scrollbarsVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures _fullscreenDisplay] == nil);
    EXPECT_TRUE([openWindowFeatures _dialogDisplay] == nil);
    openWindowFeatures = nullptr;

    NSString *featuresStringOnlyNonPopupSpecifiedAndFalse = @"noopener=false,noreferrer=false";
    [webView evaluateJavaScript:[NSString stringWithFormat:windowOpenFormatString, featuresStringOnlyNonPopupSpecifiedAndFalse] completionHandler:nil];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    EXPECT_FALSE([openWindowFeatures _wantsPopup]);
    EXPECT_FALSE([openWindowFeatures _hasAdditionalFeatures]);
    EXPECT_TRUE([openWindowFeatures menuBarVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures statusBarVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures toolbarsVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures allowsResizing] == nil);
    EXPECT_TRUE([openWindowFeatures _popup] == nil);
    EXPECT_TRUE([openWindowFeatures _locationBarVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures _scrollbarsVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures _fullscreenDisplay] == nil);
    EXPECT_TRUE([openWindowFeatures _dialogDisplay] == nil);
    openWindowFeatures = nullptr;

    NSString *featuresStringAllSpecifiedAndTrue = @"popup=yes,menubar=yes,status=yes,toolbar=yes,resizable=yes,location=yes,scrollbars=yes,fullscreen=yes";
    [webView evaluateJavaScript:[NSString stringWithFormat:windowOpenFormatString, featuresStringAllSpecifiedAndTrue] completionHandler:nil];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    EXPECT_TRUE([openWindowFeatures _wantsPopup]);
    EXPECT_FALSE([openWindowFeatures _hasAdditionalFeatures]);
    EXPECT_TRUE([openWindowFeatures menuBarVisibility].boolValue);
    EXPECT_TRUE([openWindowFeatures statusBarVisibility].boolValue);
    EXPECT_TRUE([openWindowFeatures toolbarsVisibility].boolValue);
    EXPECT_TRUE([openWindowFeatures allowsResizing].boolValue);
    EXPECT_TRUE([openWindowFeatures _popup].boolValue);
    EXPECT_TRUE([openWindowFeatures _locationBarVisibility].boolValue);
    EXPECT_TRUE([openWindowFeatures _scrollbarsVisibility].boolValue);
    EXPECT_TRUE([openWindowFeatures _fullscreenDisplay].boolValue);
    EXPECT_TRUE([openWindowFeatures _dialogDisplay] == nil);
    openWindowFeatures = nullptr;

    NSString *featuresStringAllSpecifiedAndFalse = @"popup=no,menubar=no,status=no,toolbar=no,resizable=no,location=no,scrollbars=no,fullscreen=no";
    [webView evaluateJavaScript:[NSString stringWithFormat:windowOpenFormatString, featuresStringAllSpecifiedAndFalse] completionHandler:nil];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    EXPECT_FALSE([openWindowFeatures _wantsPopup]);
    EXPECT_FALSE([openWindowFeatures _hasAdditionalFeatures]);
    EXPECT_FALSE([openWindowFeatures menuBarVisibility].boolValue);
    EXPECT_FALSE([openWindowFeatures statusBarVisibility ].boolValue);
    EXPECT_FALSE([openWindowFeatures toolbarsVisibility].boolValue);
    EXPECT_FALSE([openWindowFeatures allowsResizing].boolValue);
    EXPECT_FALSE([openWindowFeatures _popup].boolValue);
    EXPECT_FALSE([openWindowFeatures _locationBarVisibility].boolValue);
    EXPECT_FALSE([openWindowFeatures _scrollbarsVisibility].boolValue);
    EXPECT_FALSE([openWindowFeatures _fullscreenDisplay].boolValue);
    EXPECT_FALSE([openWindowFeatures _dialogDisplay].boolValue);
    openWindowFeatures = nullptr;

    NSString *featuresStringAllSpecifiedWithoutValues = @"popup,menubar,status,toolbar,resizable,location,scrollbars,fullscreen";
    [webView evaluateJavaScript:[NSString stringWithFormat:windowOpenFormatString, featuresStringAllSpecifiedWithoutValues] completionHandler:nil];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    EXPECT_TRUE([openWindowFeatures _wantsPopup]);
    EXPECT_FALSE([openWindowFeatures _hasAdditionalFeatures]);
    EXPECT_TRUE([openWindowFeatures menuBarVisibility].boolValue);
    EXPECT_TRUE([openWindowFeatures statusBarVisibility ].boolValue);
    EXPECT_TRUE([openWindowFeatures toolbarsVisibility].boolValue);
    EXPECT_TRUE([openWindowFeatures allowsResizing].boolValue);
    EXPECT_TRUE([openWindowFeatures _popup].boolValue);
    EXPECT_TRUE([openWindowFeatures _locationBarVisibility].boolValue);
    EXPECT_TRUE([openWindowFeatures _scrollbarsVisibility].boolValue);
    EXPECT_TRUE([openWindowFeatures _fullscreenDisplay].boolValue);
    EXPECT_TRUE([openWindowFeatures _dialogDisplay] == nil);
    openWindowFeatures = nullptr;

    NSString *featuresStringWithUnknownFeatures = @"foo=bar";
    [webView evaluateJavaScript:[NSString stringWithFormat:windowOpenFormatString, featuresStringWithUnknownFeatures] completionHandler:nil];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    EXPECT_TRUE([openWindowFeatures _wantsPopup]);
    EXPECT_TRUE([openWindowFeatures _hasAdditionalFeatures]);
    EXPECT_TRUE([openWindowFeatures menuBarVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures statusBarVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures toolbarsVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures allowsResizing] == nil);
    EXPECT_TRUE([openWindowFeatures _popup] == nil);
    EXPECT_TRUE([openWindowFeatures _locationBarVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures _scrollbarsVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures _fullscreenDisplay] == nil);
    EXPECT_TRUE([openWindowFeatures _dialogDisplay] == nil);
    openWindowFeatures = nullptr;

    NSString *featuresStringGarbage = @",,   =   ,   ";
    [webView evaluateJavaScript:[NSString stringWithFormat:windowOpenFormatString, featuresStringGarbage] completionHandler:nil];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    EXPECT_FALSE([openWindowFeatures _wantsPopup]);
    EXPECT_FALSE([openWindowFeatures _hasAdditionalFeatures]);
    EXPECT_TRUE([openWindowFeatures menuBarVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures statusBarVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures toolbarsVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures allowsResizing] == nil);
    EXPECT_TRUE([openWindowFeatures _popup] == nil);
    EXPECT_TRUE([openWindowFeatures _locationBarVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures _scrollbarsVisibility] == nil);
    EXPECT_TRUE([openWindowFeatures _fullscreenDisplay] == nil);
    EXPECT_TRUE([openWindowFeatures _dialogDisplay] == nil);
    openWindowFeatures = nullptr;
}

@interface OpenWindowThenDocumentOpenUIDelegate : NSObject <WKUIDelegate>
@end

@implementation OpenWindowThenDocumentOpenUIDelegate

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    openedWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [openedWebView setUIDelegate:sharedUIDelegate.get()];
    return openedWebView.get();
}

@end

TEST(WebKit, OpenWindowThenDocumentOpen)
{
    resetToConsistentState();

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto uiDelegate = adoptNS([[OpenWindowThenDocumentOpenUIDelegate alloc] init]);
    [webView setUIDelegate:uiDelegate.get()];
    [webView configuration].preferences.javaScriptCanOpenWindowsAutomatically = YES;

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"open-window-then-write-to-it" withExtension:@"html"]];
    [webView loadRequest:request];

    while (!openedWebView)
        TestWebKitAPI::Util::runFor(0.1_s);

    // Both WebViews should have the same URL because of document.open().
    while (![[[openedWebView URL] absoluteString] isEqualToString:[[webView URL] absoluteString]])
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_TRUE([[[openedWebView _mainFrameURL] absoluteString] isEqualToString:[[webView URL] absoluteString]]);
}

TEST(WebKit, OpenFileURLWithHost)
{
    resetToConsistentState();

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto uiDelegate = adoptNS([[OpenWindowThenDocumentOpenUIDelegate alloc] init]);
    [webView setUIDelegate:uiDelegate.get()];
    [webView configuration].preferences.javaScriptCanOpenWindowsAutomatically = YES;

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"open-window-with-file-url-with-host" withExtension:@"html"]];
    [webView loadRequest:request];

    while (![[[webView URL] absoluteString] hasSuffix:@"#test"])
        TestWebKitAPI::Util::spinRunLoop();

    while (![[[webView URL] absoluteString] hasPrefix:@"file:///"])
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(WebKit, TryClose)
{
    auto webView = adoptNS([TestWKWebView new]);
    [webView synchronouslyLoadHTMLString:@"load something"];
    EXPECT_TRUE([webView _tryClose]);
    [webView synchronouslyLoadHTMLString:@"<body onunload='runScriptThatDoesNotNeedToExist()'/>"];
    EXPECT_FALSE([webView _tryClose]);
}

TEST(WebKit, TryWindowOpenJavascriptURLInIframeSingleWindowApp)
{
    TestWebKitAPI::HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto webView = adoptNS([TestWKWebView new]);
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];
    webView.get().navigationDelegate = navigationDelegate.get();

    __block bool calledCreateWebViewWithConfiguration { false };
    uiDelegate.get().createWebViewWithConfiguration = ^WKWebView *(WKWebViewConfiguration *, WKNavigationAction *navigationAction, WKWindowFeatures *) {
        EXPECT_NULL([webView loadRequest:[navigationAction request]]);
        calledCreateWebViewWithConfiguration = true;
        return nil;
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [webView objectByEvaluatingJavaScript:@"window.open(\"javascript:alert('test');\")" inFrame:[webView firstChildFrame]];
    TestWebKitAPI::Util::run(&calledCreateWebViewWithConfiguration);
}

static void runHasOpenerTest(NSString *js, bool expectsOpener)
{
    RetainPtr webView = adoptNS([TestWKWebView new]);
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];

    __block RetainPtr<TestWKWebView> openedWebView;
    __block bool decidedPolicyInPopup = false;
    __block bool popupHasOpenerInCreateWebView = false;
    __block bool popupHasOpenerInDecidePolicyForNavigationAction = false;

    __block auto popupNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    popupNavigationDelegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        popupHasOpenerInDecidePolicyForNavigationAction = action._hasOpener;
        decisionHandler(WKNavigationActionPolicyCancel);
        decidedPolicyInPopup = true;
    };
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        popupHasOpenerInCreateWebView = action._hasOpener;
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
        [openedWebView setNavigationDelegate:popupNavigationDelegate.get()];
        return openedWebView.get();
    };

    openedWebView = nullptr;
    popupHasOpenerInCreateWebView = false;
    popupHasOpenerInDecidePolicyForNavigationAction = false;
    decidedPolicyInPopup = false;
    [webView synchronouslyLoadHTMLString:js];
    [webView evaluateJavaScript:@"runTest()" completionHandler:^(id result, NSError *error) { }];
    TestWebKitAPI::Util::run(&decidedPolicyInPopup);
    EXPECT_EQ(popupHasOpenerInCreateWebView, expectsOpener);
    EXPECT_EQ(popupHasOpenerInDecidePolicyForNavigationAction, expectsOpener);
}

TEST(WEBKIT, NavigationActionHasOpener1)
{
    runHasOpenerTest(@"<a id='testLink' href='https://www.apple.com' target='foo'>Link</a><script>function runTest() { document.getElementById('testLink').click(); }</script>", true);
}

TEST(WEBKIT, NavigationActionHasOpener2)
{
    runHasOpenerTest(@"<a id='testLink' href='https://www.apple.com' target='foo' rel='noopener'>Link</a><script>function runTest() { document.getElementById('testLink').click(); }</script>", false);
}

TEST(WEBKIT, NavigationActionHasOpener3)
{
    runHasOpenerTest(@"<a id='testLink' href='https://www.apple.com' target='foo' rel='noreferrer'>Link</a><script>function runTest() { document.getElementById('testLink').click(); }</script>", false);
}

TEST(WEBKIT, NavigationActionHasOpener4)
{
    runHasOpenerTest(@"<a id='testLink' href='https://www.apple.com' target='_blank'>Link</a><script>function runTest() { document.getElementById('testLink').click(); }</script>", false);
}

TEST(WEBKIT, NavigationActionHasOpener5)
{
    runHasOpenerTest(@"<a id='testLink' href='https://www.apple.com' target='_blank' rel='opener'>Link</a><script>function runTest() { document.getElementById('testLink').click(); }</script>", true);
}

TEST(WEBKIT, NavigationActionHasOpener6)
{
    runHasOpenerTest(@"<script>function runTest() { open('https://www.apple.com'); }</script>", true);
}

TEST(WEBKIT, NavigationActionHasOpener7)
{
    runHasOpenerTest(@"<script>function runTest() { open('https://www.apple.com', 'foo'); }</script>", true);
}

TEST(WEBKIT, NavigationActionHasOpener8)
{
    runHasOpenerTest(@"<script>function runTest() { open('https://www.apple.com', 'foo', 'noopener'); }</script>", false);
}

TEST(WEBKIT, NavigationActionHasOpener9)
{
    runHasOpenerTest(@"<script>function runTest() { open('https://www.apple.com', 'foo', 'noreferrer'); }</script>", false);
}

TEST(WEBKIT, NavigationActionHasOpener10)
{
    runHasOpenerTest(@"<script>function runTest() { open('https://www.apple.com', '_blank'); }</script>", true);
}

TEST(WEBKIT, NavigationActionHasOpener11)
{
    runHasOpenerTest(@"<form action='https://www.apple.com' target='foo'><input id='testButton' type='submit'></form><script>function runTest() { document.getElementById('testButton').click(); }</script>", true);
}

TEST(WEBKIT, NavigationActionHasOpener12)
{
    runHasOpenerTest(@"<form action='https://www.apple.com' target='foo' rel='noopener'><input id='testButton' type='submit'></form><script>function runTest() { document.getElementById('testButton').click(); }</script>", false);
}

TEST(WEBKIT, NavigationActionHasOpener13)
{
    runHasOpenerTest(@"<form action='https://www.apple.com' target='foo' rel='noreferrer'><input id='testButton' type='submit'></form><script>function runTest() { document.getElementById('testButton').click(); }</script>", false);
}

TEST(WEBKIT, NavigationActionHasOpener14)
{
    runHasOpenerTest(@"<form action='https://www.apple.com' target='_blank'><input id='testButton' type='submit'></form><script>function runTest() { document.getElementById('testButton').click(); }</script>", false);
}

TEST(WEBKIT, NavigationActionHasOpener15)
{
    runHasOpenerTest(@"<form action='https://www.apple.com' target='_blank' rel='opener'><input id='testButton' type='submit'></form><script>function runTest() { document.getElementById('testButton').click(); }</script>", true);
}
