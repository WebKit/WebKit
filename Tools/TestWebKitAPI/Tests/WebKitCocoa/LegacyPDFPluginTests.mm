/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(LEGACY_PDFKIT_PLUGIN)

#import "PDFTestHelpers.h"
#import "PlatformUtilities.h"
#import "TestCocoa.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import <Carbon/Carbon.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(LegacyPDF, PrintSize)
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    RetainPtr schemeHandler = adoptNS([TestURLSchemeHandler new]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"test"];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr delegate = adoptNS([PDFPrintUIDelegate new]);
    [webView setUIDelegate:delegate.get()];

    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto url = task.request.URL;
        NSData *data;
        NSString *mimeType;
        if ([url.path isEqualToString:@"/main.html"]) {
            mimeType = @"text/html";
            const char* html = "<br/><iframe src='test.pdf' id='pdfframe'></iframe>";
            data = [NSData dataWithBytes:html length:strlen(html)];
        } else if ([url.path isEqualToString:@"/test.pdf"]) {
            mimeType = @"application/pdf";
            data = testPDFData().get();
        } else {
            EXPECT_WK_STREQ(url.path, "/test_print.pdf");
            mimeType = @"application/pdf";
            data = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"test_print" withExtension:@"pdf"]];
        }
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:url MIMEType:mimeType expectedContentLength:data.length textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:data];
        [task didFinish];
    }];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///test_print.pdf"]]];
    auto size = [delegate waitForPageSize];
    EXPECT_EQ(size.height, 792.0);
    EXPECT_EQ(size.width, 612.0);

    __block bool receivedSize = false;
    [webView _getPDFFirstPageSizeInFrame:[webView _mainFrame] completionHandler:^(CGSize requestedSize) {
        EXPECT_EQ(requestedSize.height, 792.0);
        EXPECT_EQ(requestedSize.width, 612.0);
        receivedSize = true;
    }];
    TestWebKitAPI::Util::run(&receivedSize);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///main.html"]]];
    [webView _test_waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"window.print()" completionHandler:nil];
    auto mainFrameSize = [delegate waitForPageSize];
    EXPECT_EQ(mainFrameSize.height, 0.0);
    EXPECT_EQ(mainFrameSize.width, 0.0);

    receivedSize = false;
    [webView _getPDFFirstPageSizeInFrame:[webView _mainFrame] completionHandler:^(CGSize requestedSize) {
        EXPECT_EQ(requestedSize.height, 0.0);
        EXPECT_EQ(requestedSize.width, 0.0);
        receivedSize = true;
    }];
    TestWebKitAPI::Util::run(&receivedSize);

    [webView evaluateJavaScript:@"pdfframe.contentWindow.print()" completionHandler:nil];
    auto pdfFrameSize = [delegate waitForPageSize];
    EXPECT_NEAR(pdfFrameSize.height, 28.799999, .00001);
    EXPECT_NEAR(pdfFrameSize.width, 129.600006, .00001);

    receivedSize = false;
    [webView _getPDFFirstPageSizeInFrame:[delegate lastPrintedFrame] completionHandler:^(CGSize requestedSize) {
        EXPECT_NEAR(requestedSize.height, 28.799999, .00001);
        EXPECT_NEAR(requestedSize.width, 129.600006, .00001);
        receivedSize = true;
    }];
    TestWebKitAPI::Util::run(&receivedSize);
}

TEST(LegacyPDF, SetPageZoomFactorDoesNotBailIncorrectly)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:adoptNS([WKWebViewConfiguration new]).get()]);
    [webView loadData:testPDFData().get() MIMEType:@"application/pdf" characterEncodingName:@"" baseURL:[NSURL URLWithString:@"https://www.apple.com/testPath"]];
    [webView _test_waitForDidFinishNavigation];

    double scaleBeforeZooming = [webView _pageZoomFactor];

    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    [[webView _pdfHUDs].anyObject performSelector:NSSelectorFromString(@"_performActionForControl:") withObject:@"plus.magnifyingglass"];
    [webView waitForNextPresentationUpdate];

    double scaleAfterZooming = [webView _pageZoomFactor];
    EXPECT_GT(scaleAfterZooming, scaleBeforeZooming);

    [webView _setPageZoomFactor:1];
    [webView waitForNextPresentationUpdate];

    double scaleAfterResetting = [webView _pageZoomFactor];
    EXPECT_LT(scaleAfterResetting, scaleAfterZooming);
    EXPECT_EQ(scaleAfterResetting, 1.0);
}

static void checkFrame(NSRect frame, CGFloat x, CGFloat y, CGFloat width, CGFloat height)
{
    EXPECT_EQ(frame.origin.x, x);
    EXPECT_EQ(frame.origin.y, y);
    EXPECT_EQ(frame.size.width, width);
    EXPECT_EQ(frame.size.height, height);
}

TEST(LegacyPDF, PDFHUDMainResourcePDF)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:adoptNS([WKWebViewConfiguration new]).get()]);
    [webView loadData:testPDFData().get() MIMEType:@"application/pdf" characterEncodingName:@"" baseURL:[NSURL URLWithString:@"https://www.apple.com/testPath"]];
    EXPECT_EQ([webView _pdfHUDs].count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 0, 0, 800, 600);

    RetainPtr delegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:delegate.get()];
    __block bool saveRequestReceived = false;
    [delegate setSaveDataToFile:^(WKWebView *webViewFromDelegate, NSData *data, NSString *suggestedFilename, NSString *mimeType, NSURL *originatingURL) {
        EXPECT_EQ(webView.get(), webViewFromDelegate);
        EXPECT_TRUE([data isEqualToData:testPDFData().get()]);
        EXPECT_WK_STREQ(suggestedFilename, "testPath.pdf");
        EXPECT_WK_STREQ(mimeType, "application/pdf");
        saveRequestReceived = true;
    }];
    [[webView _pdfHUDs].anyObject performSelector:NSSelectorFromString(@"_performActionForControl:") withObject:@"arrow.down.circle"];
    TestWebKitAPI::Util::run(&saveRequestReceived);

    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    [webView _killWebContentProcess];
    while ([webView _pdfHUDs].count)
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(LegacyPDF, PDFHUDNestedIFrames)
{
    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        RetainPtr htmlResponse = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            const char* html = "<iframe src='frame.html' id='parentframe'></iframe>";
            [task didReceiveResponse:htmlResponse.get()];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        } else if ([task.request.URL.path isEqualToString:@"/frame.html"]) {
            const char* html = "<iframe src='test.pdf'></iframe>";
            [task didReceiveResponse:htmlResponse.get()];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        } else {
            EXPECT_WK_STREQ(task.request.URL.path, "/test.pdf");
            RetainPtr data = testPDFData();
            RetainPtr response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"application/pdf" expectedContentLength:[data length] textEncodingName:nil]);
            [task didReceiveResponse:response.get()];
            [task didReceiveData:data.get()];
            [task didFinish];
        }
    }];

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test"];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///main.html"]]];
    EXPECT_EQ([webView _pdfHUDs].count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 20, 20, 300, 150);

    [webView evaluateJavaScript:@"document.body.removeChild(parentframe)" completionHandler:nil];
    while ([webView _pdfHUDs].count)
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(LegacyPDF, PDFHUDIFrame3DTransform)
{
    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        RetainPtr htmlResponse = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            const char* html = "<iframe src='test.pdf' height=500 width=500 style='transform:rotateY(235deg);'></iframe>";
            [task didReceiveResponse:htmlResponse.get()];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        } else {
            EXPECT_WK_STREQ(task.request.URL.path, "/test.pdf");
            RetainPtr data = testPDFData();
            RetainPtr response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"application/pdf" expectedContentLength:[data length] textEncodingName:nil]);
            [task didReceiveResponse:response.get()];
            [task didReceiveData:data.get()];
            [task didFinish];
        }
    }];

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test"];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///main.html"]]];
    EXPECT_EQ([webView _pdfHUDs].count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 403, 10, 500, 500);
}

TEST(LegacyPDF, PDFHUDMultipleIFrames)
{
    RetainPtr handler = adoptNS([TestURLSchemeHandler new]);
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        RetainPtr htmlResponse = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            const char* html = "<iframe src='test.pdf' height=100 width=150></iframe><iframe src='test.pdf' height=123 width=134></iframe>";
            [task didReceiveResponse:htmlResponse.get()];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        } else {
            EXPECT_WK_STREQ(task.request.URL.path, "/test.pdf");
            RetainPtr data = testPDFData();
            RetainPtr response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"application/pdf" expectedContentLength:[data length] textEncodingName:nil]);
            [task didReceiveResponse:response.get()];
            [task didReceiveData:data.get()];
            [task didFinish];
        }
    }];

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test"];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///main.html"]]];
    EXPECT_EQ([webView _pdfHUDs].count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ([webView _pdfHUDs].count, 2u);
    bool hadLeftFrame = false;
    bool hadRightFrame = false;
    for (NSView *hud in [webView _pdfHUDs]) {
        if (hud.frame.origin.x == 10) {
            checkFrame(hud.frame, 10, 33, 150, 100);
            hadLeftFrame = true;
        } else {
            checkFrame(hud.frame, 164, 10, 134, 123);
            hadRightFrame = true;
        }
    }
    EXPECT_TRUE(hadLeftFrame);
    EXPECT_TRUE(hadRightFrame);
}

TEST(LegacyPDF, PDFHUDLoadPDFTypeWithPluginsBlocked)
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration _setOverrideContentSecurityPolicy:@"object-src 'none'"];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadData:testPDFData().get() MIMEType:@"application/pdf" characterEncodingName:@"" baseURL:[NSURL URLWithString:@"https://www.apple.com/testPath"]];
    EXPECT_EQ([webView _pdfHUDs].count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 0, 0, 800, 600);
}

}

#endif
