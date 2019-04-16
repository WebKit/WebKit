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

#import "PlatformUtilities.h"
#import <WebKit/WKPreferences.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWindowFeaturesPrivate.h>
#import <wtf/RetainPtr.h>

@class OpenAndCloseWindowUIDelegate;
@class OpenAndCloseWindowUIDelegateAsync;
@class CheckWindowFeaturesUIDelegate;

static bool isDone;
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

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"open-and-close-window" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
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
            dispatch_async(dispatch_get_main_queue(), ^ {
                openedWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
                [openedWebView setUIDelegate:sharedUIDelegateAsync.get()];
                self.expectedClosingView = openedWebView.get();
                completionHandler(openedWebView.get());
            });
        } else {
            dispatch_async(dispatch_get_main_queue(), ^ {
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

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"open-and-close-window" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
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

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"open-and-close-window" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
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
//    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"open-and-close-window" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
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

@property (nullable, nonatomic, readonly) NSNumber *menuBarVisibility;
@property (nullable, nonatomic, readonly) NSNumber *statusBarVisibility;
@property (nullable, nonatomic, readonly) NSNumber *toolbarsVisibility;

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
    NSString *windowOpenFormatString = @"window.open(\"about:blank\", \"_blank\", \"%@\")";

    [webView evaluateJavaScript:@"window.open(\"about:blank\")" completionHandler:nil];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

//  https://bugs.webkit.org/show_bug.cgi?id=174271 - WebCore currently doesn't distinguish between unspecified (nil) and false
//  for the following window features.
//  EXPECT_TRUE([openWindowFeatures menuBarVisibility] == nil);
//  EXPECT_TRUE([openWindowFeatures statusBarVisibility] == nil);
//  EXPECT_TRUE([openWindowFeatures toolbarsVisibility] == nil);
//  EXPECT_TRUE([openWindowFeatures allowsResizing] == nil);
//  EXPECT_TRUE([openWindowFeatures _locationBarVisibility] == nil);
//  EXPECT_TRUE([openWindowFeatures _scrollbarsVisibility] == nil);
//  EXPECT_TRUE([openWindowFeatures _fullscreenDisplay] == nil);
//  EXPECT_TRUE([openWindowFeatures _dialogDisplay] == nil);
    openWindowFeatures = nullptr;

    NSString *featuresStringAllSpecifiedAndTrue = @"menubar=yes,status=yes,toolbar=yes,resizable=yes,location=yes,scrollbars=yes,fullscreen=yes";
    [webView evaluateJavaScript:[NSString stringWithFormat:windowOpenFormatString, featuresStringAllSpecifiedAndTrue] completionHandler:nil];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    EXPECT_TRUE([openWindowFeatures menuBarVisibility].boolValue);
    EXPECT_TRUE([openWindowFeatures statusBarVisibility].boolValue);
    EXPECT_TRUE([openWindowFeatures toolbarsVisibility].boolValue);
    EXPECT_TRUE([openWindowFeatures allowsResizing].boolValue);
    EXPECT_TRUE([openWindowFeatures _locationBarVisibility].boolValue);
    EXPECT_TRUE([openWindowFeatures _scrollbarsVisibility].boolValue);
    EXPECT_TRUE([openWindowFeatures _fullscreenDisplay].boolValue);
    openWindowFeatures = nullptr;

    NSString *featuresStringAllSpecifiedAndFalse = @"menubar=no,status=no,toolbar=no,resizable=no,location=no,scrollbars=no,fullscreen=no";
    [webView evaluateJavaScript:[NSString stringWithFormat:windowOpenFormatString, featuresStringAllSpecifiedAndFalse] completionHandler:nil];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    EXPECT_FALSE([openWindowFeatures menuBarVisibility].boolValue);
    EXPECT_FALSE([openWindowFeatures statusBarVisibility ].boolValue);
    EXPECT_FALSE([openWindowFeatures toolbarsVisibility].boolValue);
//  https://bugs.webkit.org/show_bug.cgi?id=174388 - This property doesn't accurately reflect the parameters passed by the webpage.
//  EXPECT_FALSE([openWindowFeatures allowsResizing].boolValue);
    EXPECT_FALSE([openWindowFeatures _locationBarVisibility].boolValue);
    EXPECT_FALSE([openWindowFeatures _scrollbarsVisibility].boolValue);
    EXPECT_FALSE([openWindowFeatures _fullscreenDisplay].boolValue);
    openWindowFeatures = nullptr;
}
