/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "Test.h"
#import <WebCore/WebCoreThread.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebUIKitSupport.h>
#import <WebKit/WebViewPrivate.h>
#import <wtf/RetainPtr.h>

using namespace TestWebKitAPI;

static bool isDone;
static bool didStartQuickLookLoad;
static bool didFinishQuickLookLoad;

static const NSUInteger expectedFileSize = 274143;
static NSString * const expectedFileName = @"pages.pages";
static NSString * const expectedUTI = @"com.apple.iwork.pages.sffpages";
static NSURLRequest * const pagesDocumentRequest = [[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"pages" withExtension:@"pages" subdirectory:@"TestWebKitAPI.resources"]] retain];

@interface QuickLookNavigationDelegate : NSObject <WKNavigationDelegatePrivate>
@end

@implementation QuickLookNavigationDelegate

- (void)_webView:(WKWebView *)webView didStartLoadForQuickLookDocumentInMainFrameWithFileName:(NSString *)fileName uti:(NSString *)uti
{
    EXPECT_WK_STREQ(expectedFileName, fileName);
    EXPECT_WK_STREQ(expectedUTI, uti);
    didStartQuickLookLoad = true;
}

- (void)_webView:(WKWebView *)webView didFinishLoadForQuickLookDocumentInMainFrame:(NSData *)documentData
{
    EXPECT_EQ(expectedFileSize, documentData.length);
    didFinishQuickLookLoad = true;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    isDone = true;
}

@end

@interface QuickLookAsyncNavigationDelegate : NSObject <WKNavigationDelegatePrivate>
@end

@implementation QuickLookAsyncNavigationDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    int64_t deferredWaitTime = 100 * NSEC_PER_MSEC;
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, deferredWaitTime);

    dispatch_after(when, dispatch_get_main_queue(), ^{
        decisionHandler(WKNavigationResponsePolicyAllow);
    });
}

- (void)_webView:(WKWebView *)webView didStartLoadForQuickLookDocumentInMainFrameWithFileName:(NSString *)fileName uti:(NSString *)uti
{
    EXPECT_WK_STREQ(expectedFileName, fileName);
    EXPECT_WK_STREQ(expectedUTI, uti);
    didStartQuickLookLoad = true;
}

- (void)_webView:(WKWebView *)webView didFinishLoadForQuickLookDocumentInMainFrame:(NSData *)documentData
{
    EXPECT_EQ(expectedFileSize, documentData.length);
    didFinishQuickLookLoad = true;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    isDone = true;
}

@end

static void runTest(Class navigationDelegateClass, NSURLRequest *request)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto navigationDelegate = adoptNS([[navigationDelegateClass alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadRequest:request];

    isDone = false;
    Util::run(&isDone);
}

TEST(QuickLook, NavigationDelegate)
{
    runTest([QuickLookNavigationDelegate class], pagesDocumentRequest);
    EXPECT_TRUE(didStartQuickLookLoad);
    EXPECT_TRUE(didFinishQuickLookLoad);
}

TEST(QuickLook, AsyncNavigationDelegate)
{
    runTest([QuickLookAsyncNavigationDelegate class], pagesDocumentRequest);
    EXPECT_TRUE(didStartQuickLookLoad);
    EXPECT_TRUE(didFinishQuickLookLoad);
}

@interface QuickLookDecidePolicyDelegate : NSObject <WKNavigationDelegate>
@end

@implementation QuickLookDecidePolicyDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    decisionHandler(WKNavigationResponsePolicyCancel);
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    isDone = true;
}

- (void)_webViewWebProcessDidCrash:(WKWebView *)webView
{
    RELEASE_ASSERT_NOT_REACHED();
}

@end

@interface QuickLookFrameLoadDelegate : NSObject <WebFrameLoadDelegate>
@end

@implementation QuickLookFrameLoadDelegate

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    isDone = true;
}

@end

TEST(QuickLook, CancelNavigationAfterResponse)
{
    runTest([QuickLookDecidePolicyDelegate class], pagesDocumentRequest);
}

@interface QuickLookPasswordNavigationDelegate : NSObject <WKNavigationDelegatePrivate>
@end

@implementation QuickLookPasswordNavigationDelegate

- (void)_webViewDidRequestPasswordForQuickLookDocument:(WKWebView *)webView
{
    isDone = true;
}

@end

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000

TEST(QuickLook, DidRequestPasswordNavigationDelegate)
{
    NSURLRequest *passwordProtectedDocumentRequest = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"password-protected" withExtension:@"pages" subdirectory:@"TestWebKitAPI.resources"]];
    runTest([QuickLookPasswordNavigationDelegate class], passwordProtectedDocumentRequest);
}

#endif

TEST(QuickLook, LegacyQuickLookContent)
{
    WebKitInitialize();
    WebThreadLock();

    auto webView = adoptNS([[WebView alloc] init]);

    auto frameLoadDelegate = adoptNS([[QuickLookFrameLoadDelegate alloc] init]);
    [webView setFrameLoadDelegate:frameLoadDelegate.get()];

    auto webPreferences = adoptNS([[WebPreferences alloc] initWithIdentifier:@"LegacyQuickLookContent"]);
    [webPreferences setQuickLookDocumentSavingEnabled:YES];
    [webView setPreferences:webPreferences.get()];

    WebFrame *mainFrame = [webView mainFrame];

    isDone = false;
    [mainFrame loadRequest:pagesDocumentRequest];
    Util::run(&isDone);
    WebThreadLock();

    NSDictionary *quickLookContent = mainFrame.dataSource._quickLookContent;
    NSString *filePath = quickLookContent[WebQuickLookFileNameKey];
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:filePath]);
    EXPECT_WK_STREQ(expectedFileName, filePath.lastPathComponent);
    EXPECT_WK_STREQ(expectedUTI, quickLookContent[WebQuickLookUTIKey]);

    NSDictionary *fileAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:filePath error:nil];
    EXPECT_EQ(expectedFileSize, [fileAttributes[NSFileSize] unsignedIntegerValue]);

    isDone = false;
    [mainFrame loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    Util::run(&isDone);
    WebThreadLock();

    EXPECT_NULL(mainFrame.dataSource._quickLookContent);
}

#endif // PLATFORM(IOS_FAMILY)
