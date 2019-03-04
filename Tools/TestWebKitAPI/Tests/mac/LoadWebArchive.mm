/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "DragAndDropSimulator.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKDragDestinationAction.h>
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

static bool navigationComplete = false;
static bool navigationFail = false;
static String finalURL;
static id<WKNavigationDelegate> gDelegate;
static RetainPtr<WKWebView> newWebView;

@interface TestLoadWebArchiveNavigationDelegate : NSObject <WKNavigationDelegate, WKUIDelegate>
@end

@implementation TestLoadWebArchiveNavigationDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    navigationComplete = true;
    finalURL = navigation._request.URL.lastPathComponent;
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    navigationFail = true;
    finalURL = navigation._request.URL.lastPathComponent;
}

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    newWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
    [newWebView setNavigationDelegate:gDelegate];

    return newWebView.get();
}

@end

namespace TestWebKitAPI {

TEST(LoadWebArchive, FailNavigation1)
{
    // Using `document.location.href = 'helloworld.webarchive';`.
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"load-web-archive-1" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[WKWebView alloc] init]);
    auto delegate = adoptNS([[TestLoadWebArchiveNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    navigationFail = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationFail);

    EXPECT_WK_STREQ(finalURL, "helloworld.webarchive");
}

TEST(LoadWebArchive, FailNavigation2)
{
    // Using `window.open('helloworld.webarchive');`.
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"load-web-archive-2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto delegate = adoptNS([[TestLoadWebArchiveNavigationDelegate alloc] init]);
    gDelegate = delegate.get();

    auto webView = adoptNS([[WKWebView alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    navigationFail = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationFail);

    EXPECT_WK_STREQ(finalURL, "helloworld.webarchive");
}

TEST(LoadWebArchive, ClientNavigationSucceed)
{
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"helloworld" withExtension:@"webarchive" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[WKWebView alloc] init]);
    auto delegate = adoptNS([[TestLoadWebArchiveNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    navigationComplete = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationComplete);

    EXPECT_WK_STREQ(finalURL, "helloworld.webarchive");
}

TEST(LoadWebArchive, ClientNavigationReload)
{
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"helloworld" withExtension:@"webarchive" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[WKWebView alloc] init]);
    auto delegate = adoptNS([[TestLoadWebArchiveNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    navigationComplete = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, "helloworld.webarchive");

    navigationComplete = false;
    [webView reload];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, "");
}

TEST(LoadWebArchive, DragNavigationSucceed)
{
    RetainPtr<NSURL> webArchiveURL = [[NSBundle mainBundle] URLForResource:@"helloworld" withExtension:@"webarchive" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> simpleURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard declareTypes:@[NSFilenamesPboardType] owner:nil];
    [pasteboard setPropertyList:@[webArchiveURL.get().path] forType:NSFilenamesPboardType];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    [simulator setExternalDragPasteboard:pasteboard];
    [simulator setDragDestinationAction:WKDragDestinationActionAny];

    auto webView = [simulator webView];
    auto delegate = adoptNS([[TestLoadWebArchiveNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    navigationComplete = false;
    [webView loadRequest:[NSURLRequest requestWithURL:simpleURL.get()]];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, "simple.html");

    navigationComplete = false;
    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(50, 50)];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, "helloworld.webarchive");
}

TEST(LoadWebArchive, DragNavigationReload)
{
    RetainPtr<NSURL> webArchiveURL = [[NSBundle mainBundle] URLForResource:@"helloworld" withExtension:@"webarchive" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> simpleURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard declareTypes:@[NSFilenamesPboardType] owner:nil];
    [pasteboard setPropertyList:@[webArchiveURL.get().path] forType:NSFilenamesPboardType];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    [simulator setExternalDragPasteboard:pasteboard];
    [simulator setDragDestinationAction:WKDragDestinationActionAny];

    auto webView = [simulator webView];
    auto delegate = adoptNS([[TestLoadWebArchiveNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    navigationComplete = false;
    [webView loadRequest:[NSURLRequest requestWithURL:simpleURL.get()]];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, "simple.html");

    navigationComplete = false;
    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(50, 50)];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, "helloworld.webarchive");

    navigationComplete = false;
    [webView reload];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, "");
}

} // namespace TestWebKitAPI
