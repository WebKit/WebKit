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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(WKWebView, PageZoom)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<body>TEST</body>" baseURL:nil];

    // On macOS this will be 400, per the size of the WKWebView.
    // On iOS devices it will be a native device width, and we need to fetch that here.
    unsigned beforeClientWidth = [webView waitUntilClientWidthIs:400];

    webView.get().pageZoom = 2.0;

    unsigned afterClientWidth = [webView waitUntilClientWidthIs:beforeClientWidth / 2];
    EXPECT_EQ(beforeClientWidth / 2, afterClientWidth);
}

TEST(WKWebView, PageZoomAfterPDF)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];
    webView.get().pageZoom = 2.0;
    auto beforePageZoom = webView.get().pageZoom;

    NSURLRequest *pdfRequest = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]];
    [webView loadRequest:pdfRequest];
    [webView _test_waitForDidFinishNavigation];

    [webView goBack];
    [webView _test_waitForDidFinishNavigation];

    auto afterPageZoom = webView.get().pageZoom;
    EXPECT_EQ(beforePageZoom, afterPageZoom);
}

#if PLATFORM(MAC)

TEST(WKWebView, MinimumMagnification)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<body>TEST</body>" baseURL:nil];

    EXPECT_EQ([webView minimumMagnification], 1.00);
}

TEST(WKWebView, MinimumMagnificationPDF)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);

    NSURLRequest *pdfRequest = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]];
    [webView loadRequest:pdfRequest];
    [webView _test_waitForDidFinishNavigation];

    // See PDFPluginBase::minScaleFactor()
    EXPECT_LT([webView minimumMagnification], 1.00);
}

#endif

#if PLATFORM(MAC)

TEST(WKWebView, PageZoomPreservedAcrossSessionRestore)
{
    // Load a page and set a non-default zoom factor.
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<body>TEST</body>" baseURL:[NSURL URLWithString:@"https://example.com/"]];
    [webView setPageZoom:1.5];
    EXPECT_EQ(1.5, [webView pageZoom]);

    // Save session state.
    RetainPtr sessionData = [webView _sessionStateData];
    EXPECT_NOT_NULL(sessionData.get());

    // Create a new web view and restore from session state.
    RetainPtr restoredWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [restoredWebView _restoreFromSessionStateData:sessionData.get()];
    [restoredWebView _test_waitForDidFinishNavigation];

    // Verify the zoom factor was restored.
    EXPECT_EQ(1.5, [restoredWebView pageZoom]);
}

TEST(WKWebView, PageZoomDefaultAfterOldSessionRestore)
{
    // Verify that restoring from a session that has no zoom data defaults to 1.0.
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<body>TEST</body>" baseURL:nil];
    EXPECT_EQ(1.0, [webView pageZoom]);
}

#endif // PLATFORM(MAC)

} // namespace TestWebKitAPI
