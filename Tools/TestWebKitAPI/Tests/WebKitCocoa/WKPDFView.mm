/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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

#import "ClassMethodSwizzler.h"
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(MAC)
#import <Carbon/Carbon.h>
#endif

#if PLATFORM(IOS) || ENABLE(UI_PROCESS_PDF_HUD)
static NSData *pdfData()
{
    return [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"test" withExtension:@"pdf" subdirectory:@"TestWebKitAPI.resources"]];
}
#endif

#if PLATFORM(IOS)

@interface PDFHostViewController : UIViewController
+ (void)createHostView:(void(^)(id hostViewController))callback forExtensionIdentifier:(NSString *)extensionIdentifier;
@end

@interface WKApplicationStateTrackingView : UIView
- (BOOL)isBackground;
@end

TEST(WebKit, WKPDFViewResizeCrash)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"test" withExtension:@"pdf" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView setFrame:NSMakeRect(0, 0, 100, 100)];
    webView = nil;

    __block bool finishedDispatch = false;
    dispatch_async(dispatch_get_main_queue(), ^{
        finishedDispatch = true;
    });

    TestWebKitAPI::Util::run(&finishedDispatch);
}

TEST(WebKit, WKPDFViewStablePresentationUpdateCallback)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"test" withExtension:@"pdf" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    __block bool finished;
    [webView _doAfterNextStablePresentationUpdate:^{
        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

static BOOL sIsBackground;
static BOOL isBackground(id self)
{
    return sIsBackground;
}

static void createHostViewForExtensionIdentifier(void(^callback)(id hostViewController), NSString *extensionIdentifier)
{
}

TEST(WebKit, WKPDFViewLosesApplicationForegroundNotification)
{
    std::unique_ptr<ClassMethodSwizzler> pdfHostViewControllerSwizzler = makeUnique<ClassMethodSwizzler>([PDFHostViewController class], @selector(createHostView:forExtensionIdentifier:), reinterpret_cast<IMP>(createHostViewForExtensionIdentifier));

    std::unique_ptr<InstanceMethodSwizzler> isInBackgroundSwizzler = makeUnique<InstanceMethodSwizzler>(NSClassFromString(@"WKApplicationStateTrackingView"), @selector(isBackground), reinterpret_cast<IMP>(isBackground));

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setClientNavigationsRunAtForegroundPriority:YES];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Load a PDF, so we install a WKPDFView.
    [webView loadData:pdfData() MIMEType:@"application/pdf" characterEncodingName:@"" baseURL:[NSURL URLWithString:@"https://www.apple.com/0"]];
    [webView _test_waitForDidFinishNavigation];

    // Go into the background and parent the WKWebView.
    // This will cause WKPDFView to send a applicationDidEnterBackground
    // to the Web Content process, freezing the layer tree.
    sIsBackground = YES;
    RetainPtr<UIWindow> uiWindow = adoptNS([[UIWindow alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [uiWindow addSubview:webView.get()];
    [webView removeFromSuperview];

    // Load a HTML document, so we switch back to WKContentView.
    [webView loadHTMLString:@"<meta name='viewport' content='width=device-width'><h1>hello world</h1>" baseURL:[NSURL URLWithString:@"https://www.apple.com/1"]];
    [webView _test_waitForDidFinishNavigationWithoutPresentationUpdate];

    // Go into the foreground, and parent the view.
    // This should be sufficient to unfreeze the layer tree.
    sIsBackground = NO;
    [uiWindow addSubview:webView.get()];

    __block bool finished = false;
    // If the bug reproduces, no stable presentation update will ever come,
    // so the test times out here.
    [webView _doAfterNextPresentationUpdate:^{
        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

#endif

#if ENABLE(UI_PROCESS_PDF_HUD)

static void checkFrame(NSRect frame, CGFloat x, CGFloat y, CGFloat width, CGFloat height)
{
    EXPECT_EQ(frame.origin.x, x);
    EXPECT_EQ(frame.origin.y, y);
    EXPECT_EQ(frame.size.width, width);
    EXPECT_EQ(frame.size.height, height);
}

TEST(PDFHUD, MainResourcePDF)
{
    TestWKWebView *webView = [[[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:[[WKWebViewConfiguration new] autorelease]] autorelease];
    [webView loadData:pdfData() MIMEType:@"application/pdf" characterEncodingName:@"" baseURL:[NSURL URLWithString:@"https://www.apple.com/testPath"]];
    EXPECT_EQ(webView._pdfHUDs.count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ(webView._pdfHUDs.count, 1u);
    checkFrame(webView._pdfHUDs.anyObject.frame, 0, 0, 800, 600);
    
    TestUIDelegate *delegate = [[TestUIDelegate new] autorelease];
    webView.UIDelegate = delegate;
    __block bool saveRequestReceived = false;
    delegate.saveDataToFile = ^(WKWebView *webViewFromDelegate, NSData *data, NSString *suggestedFilename, NSString *mimeType, NSURL *originatingURL) {
        EXPECT_EQ(webView, webViewFromDelegate);
        EXPECT_TRUE([data isEqualToData:pdfData()]);
        EXPECT_WK_STREQ(suggestedFilename, "testPath.pdf");
        EXPECT_WK_STREQ(mimeType, "application/pdf");
        saveRequestReceived = true;
    };
    [[webView _pdfHUDs].anyObject performSelector:NSSelectorFromString(@"_performActionForControl:") withObject:@"arrow.down.circle"];
    TestWebKitAPI::Util::run(&saveRequestReceived);

    EXPECT_EQ(webView._pdfHUDs.count, 1u);
    [webView _killWebContentProcess];
    while (webView._pdfHUDs.count)
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(PDFHUD, MoveIFrame)
{
    TestURLSchemeHandler *handler = [[TestURLSchemeHandler new] autorelease];
    handler.startURLSchemeTaskHandler = ^(WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            NSURLResponse *response = [[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil] autorelease];
            const char* html = "<br/><iframe src='test.pdf' id='pdfframe'></iframe>";
            [task didReceiveResponse:response];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        } else {
            EXPECT_WK_STREQ(task.request.URL.path, "/test.pdf");
            NSData *data = pdfData();
            NSURLResponse *response = [[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"application/pdf" expectedContentLength:data.length textEncodingName:nil] autorelease];
            [task didReceiveResponse:response];
            [task didReceiveData:data];
            [task didFinish];
        }
    };

    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration new] autorelease];
    [configuration setURLSchemeHandler:handler forURLScheme:@"test"];
    TestWKWebView *webView = [[[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration] autorelease];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///main.html"]]];
    EXPECT_EQ(webView._pdfHUDs.count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ(webView._pdfHUDs.count, 1u);
    checkFrame(webView._pdfHUDs.anyObject.frame, 10, 28, 300, 150);

    [webView evaluateJavaScript:@"pdfframe.width=400" completionHandler:nil];
    while (webView._pdfHUDs.anyObject.frame.size.width != 400)
        TestWebKitAPI::Util::spinRunLoop();
    checkFrame(webView._pdfHUDs.anyObject.frame, 10, 28, 400, 150);

    [webView evaluateJavaScript:@"var frameReference = pdfframe; document.body.removeChild(pdfframe)" completionHandler:nil];
    while (webView._pdfHUDs.count)
        TestWebKitAPI::Util::spinRunLoop();
    [webView evaluateJavaScript:@"document.body.appendChild(frameReference)" completionHandler:nil];
    while (!webView._pdfHUDs.count)
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_EQ(webView._pdfHUDs.count, 1u);
    checkFrame(webView._pdfHUDs.anyObject.frame, 0, 0, 0, 0);
    while (webView._pdfHUDs.anyObject.frame.size.width != 400)
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_EQ(webView._pdfHUDs.count, 1u);
    checkFrame(webView._pdfHUDs.anyObject.frame, 10, 28, 400, 150);

    webView.pageZoom = 1.4;
    while (webView._pdfHUDs.anyObject.frame.size.width != 560)
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_EQ(webView._pdfHUDs.count, 1u);
    checkFrame(webView._pdfHUDs.anyObject.frame, 14, 40, 560, 210);
}

TEST(PDFHUD, NestedIFrames)
{
    TestURLSchemeHandler *handler = [[TestURLSchemeHandler new] autorelease];
    handler.startURLSchemeTaskHandler = ^(WKWebView *, id<WKURLSchemeTask> task) {
        NSURLResponse *htmlResponse = [[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil] autorelease];
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            const char* html = "<iframe src='frame.html' id='parentframe'></iframe>";
            [task didReceiveResponse:htmlResponse];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        } else if ([task.request.URL.path isEqualToString:@"/frame.html"]) {
            const char* html = "<iframe src='test.pdf'></iframe>";
            [task didReceiveResponse:htmlResponse];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        } else {
            EXPECT_WK_STREQ(task.request.URL.path, "/test.pdf");
            NSData *data = pdfData();
            NSURLResponse *response = [[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"application/pdf" expectedContentLength:data.length textEncodingName:nil] autorelease];
            [task didReceiveResponse:response];
            [task didReceiveData:data];
            [task didFinish];
        }
    };

    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration new] autorelease];
    [configuration setURLSchemeHandler:handler forURLScheme:@"test"];
    TestWKWebView *webView = [[[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration] autorelease];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///main.html"]]];
    EXPECT_EQ(webView._pdfHUDs.count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ(webView._pdfHUDs.count, 1u);
    checkFrame(webView._pdfHUDs.anyObject.frame, 20, 20, 300, 150);
    
    [webView evaluateJavaScript:@"document.body.removeChild(parentframe)" completionHandler:nil];
    while (webView._pdfHUDs.count)
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(PDFHUD, IFrame3DTransform)
{
    TestURLSchemeHandler *handler = [[TestURLSchemeHandler new] autorelease];
    handler.startURLSchemeTaskHandler = ^(WKWebView *, id<WKURLSchemeTask> task) {
        NSURLResponse *htmlResponse = [[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil] autorelease];
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            const char* html = "<iframe src='test.pdf' height=500 width=500 style='transform:rotateY(235deg);'></iframe>";
            [task didReceiveResponse:htmlResponse];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        } else {
            EXPECT_WK_STREQ(task.request.URL.path, "/test.pdf");
            NSData *data = pdfData();
            NSURLResponse *response = [[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"application/pdf" expectedContentLength:data.length textEncodingName:nil] autorelease];
            [task didReceiveResponse:response];
            [task didReceiveData:data];
            [task didFinish];
        }
    };

    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration new] autorelease];
    [configuration setURLSchemeHandler:handler forURLScheme:@"test"];
    TestWKWebView *webView = [[[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration] autorelease];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///main.html"]]];
    EXPECT_EQ(webView._pdfHUDs.count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ(webView._pdfHUDs.count, 1u);
    checkFrame(webView._pdfHUDs.anyObject.frame, 403, 10, 500, 500);
}

TEST(PDFHUD, MultipleIFrames)
{
    TestURLSchemeHandler *handler = [[TestURLSchemeHandler new] autorelease];
    handler.startURLSchemeTaskHandler = ^(WKWebView *, id<WKURLSchemeTask> task) {
        NSURLResponse *htmlResponse = [[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil] autorelease];
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            const char* html = "<iframe src='test.pdf' height=100 width=150></iframe><iframe src='test.pdf' height=123 width=134></iframe>";
            [task didReceiveResponse:htmlResponse];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        } else {
            EXPECT_WK_STREQ(task.request.URL.path, "/test.pdf");
            NSData *data = pdfData();
            NSURLResponse *response = [[[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"application/pdf" expectedContentLength:data.length textEncodingName:nil] autorelease];
            [task didReceiveResponse:response];
            [task didReceiveData:data];
            [task didFinish];
        }
    };

    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration new] autorelease];
    [configuration setURLSchemeHandler:handler forURLScheme:@"test"];
    TestWKWebView *webView = [[[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration] autorelease];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///main.html"]]];
    EXPECT_EQ(webView._pdfHUDs.count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ(webView._pdfHUDs.count, 2u);
    bool hadLeftFrame = false;
    bool hadRightFrame = false;
    for (NSView *hud in webView._pdfHUDs) {
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

#endif
