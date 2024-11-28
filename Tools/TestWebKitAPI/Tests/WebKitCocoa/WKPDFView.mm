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
#import "TestCocoa.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "UnifiedPDFTestHelpers.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(MAC)
#import <Carbon/Carbon.h>
#endif

#if PLATFORM(IOS) || PLATFORM(MAC) || PLATFORM(VISION)
static NSData *pdfData()
{
    return [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]];
}
#endif

#if PLATFORM(IOS) || PLATFORM(VISION)

@interface PDFHostViewController : UIViewController
+ (void)createHostView:(void(^)(id hostViewController))callback forExtensionIdentifier:(NSString *)extensionIdentifier;
@end

@interface WKApplicationStateTrackingView : UIView
- (BOOL)isBackground;
@end

TEST(WebKit, WKPDFViewResizeCrash)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]];
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
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]];
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

#if HAVE(UIFINDINTERACTION)

TEST(WebKit, WKPDFViewFindActions)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    EXPECT_FALSE([webView canPerformAction:@selector(find:) withSender:nil]);
    EXPECT_FALSE([webView canPerformAction:@selector(findNext:) withSender:nil]);
    EXPECT_FALSE([webView canPerformAction:@selector(findPrevious:) withSender:nil]);
    EXPECT_FALSE([webView canPerformAction:@selector(findAndReplace:) withSender:nil]);

    [webView setFindInteractionEnabled:YES];

    EXPECT_TRUE([webView canPerformAction:@selector(find:) withSender:nil]);
    EXPECT_TRUE([webView canPerformAction:@selector(findNext:) withSender:nil]);
    EXPECT_TRUE([webView canPerformAction:@selector(findPrevious:) withSender:nil]);
    EXPECT_FALSE([webView canPerformAction:@selector(findAndReplace:) withSender:nil]);
}

#endif

TEST(WKPDFView, BackgroundColor)
{
    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"red" withExtension:@"html"]]];
    EXPECT_TRUE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, redColor.get()));

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]]];
    EXPECT_FALSE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, redColor.get()));

    [webView synchronouslyGoBack];
    EXPECT_TRUE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, redColor.get()));
}

#endif

#if PLATFORM(IOS) || PLATFORM(MAC) || PLATFORM(VISION)

TEST(WKWebView, IsDisplayingPDF)
{
    auto runTest = [](const auto& webView) {
        [webView loadData:pdfData() MIMEType:@"application/pdf" characterEncodingName:@"" baseURL:[NSURL URLWithString:@"https://www.apple.com/testPath"]];
        [webView _test_waitForDidFinishNavigation];
        EXPECT_TRUE([webView _isDisplayingPDF]);

        [webView loadHTMLString:@"<meta name='viewport' content='width=device-width'><h1>hello world</h1>" baseURL:[NSURL URLWithString:@"https://www.apple.com/1"]];
        [webView _test_waitForDidFinishNavigationWithoutPresentationUpdate];
        EXPECT_FALSE([webView _isDisplayingPDF]);
    };

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:adoptNS([WKWebViewConfiguration new]).get()]);

    runTest(webView);

#if ENABLE(UNIFIED_PDF)
    if constexpr (TestWebKitAPI::unifiedPDFForTestingEnabled) {
        webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:TestWebKitAPI::configurationForWebViewTestingUnifiedPDF().get() addToWindow:YES]);
        runTest(webView);
    }
#endif
}

#endif

#if ENABLE(LEGACY_PDFKIT_PLUGIN)

static void checkFrame(NSRect frame, CGFloat x, CGFloat y, CGFloat width, CGFloat height)
{
    EXPECT_EQ(frame.origin.x, x);
    EXPECT_EQ(frame.origin.y, y);
    EXPECT_EQ(frame.size.width, width);
    EXPECT_EQ(frame.size.height, height);
}

TEST(PDFHUD, MainResourcePDF)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:adoptNS([WKWebViewConfiguration new]).get()]);
    [webView loadData:pdfData() MIMEType:@"application/pdf" characterEncodingName:@"" baseURL:[NSURL URLWithString:@"https://www.apple.com/testPath"]];
    EXPECT_EQ([webView _pdfHUDs].count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 0, 0, 800, 600);
    
    auto delegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:delegate.get()];
    __block bool saveRequestReceived = false;
    delegate.get().saveDataToFile = ^(WKWebView *webViewFromDelegate, NSData *data, NSString *suggestedFilename, NSString *mimeType, NSURL *originatingURL) {
        EXPECT_EQ(webView.get(), webViewFromDelegate);
        EXPECT_TRUE([data isEqualToData:pdfData()]);
        EXPECT_WK_STREQ(suggestedFilename, "testPath.pdf");
        EXPECT_WK_STREQ(mimeType, "application/pdf");
        saveRequestReceived = true;
    };
    [[webView _pdfHUDs].anyObject performSelector:NSSelectorFromString(@"_performActionForControl:") withObject:@"arrow.down.circle"];
    TestWebKitAPI::Util::run(&saveRequestReceived);

    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    [webView _killWebContentProcess];
    while ([webView _pdfHUDs].count)
        TestWebKitAPI::Util::spinRunLoop();
}

// FIXME: Re-enable this test once rdar://68639688 is resolved.
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 110000
TEST(PDFHUD, MoveIFrame)
{
    auto handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
            const char* html = "<br/><iframe src='test.pdf' id='pdfframe'></iframe>";
            [task didReceiveResponse:response.get()];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        } else {
            EXPECT_WK_STREQ(task.request.URL.path, "/test.pdf");
            NSData *data = pdfData();
            auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"application/pdf" expectedContentLength:data.length textEncodingName:nil]);
            [task didReceiveResponse:response.get()];
            [task didReceiveData:data];
            [task didFinish];
        }
    };

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test"];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///main.html"]]];
    EXPECT_EQ([webView _pdfHUDs].count, 0u);
    [webView _test_waitForDidFinishNavigation];

    // If the TestWKWebView is not visible, visibilityDidChange will be called with false, and there will be no HUD.
    if (![webView _pdfHUDs].count)
        return;

    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 10, 28, 300, 150);

    [webView evaluateJavaScript:@"pdfframe.width=400" completionHandler:nil];
    while ([webView _pdfHUDs].anyObject.frame.size.width != 400)
        TestWebKitAPI::Util::spinRunLoop();
    checkFrame([webView _pdfHUDs].anyObject.frame, 10, 28, 400, 150);

    [webView evaluateJavaScript:@"var frameReference = pdfframe; document.body.removeChild(pdfframe)" completionHandler:nil];
    while ([webView _pdfHUDs].count)
        TestWebKitAPI::Util::spinRunLoop();
    [webView evaluateJavaScript:@"document.body.appendChild(frameReference)" completionHandler:nil];
    while (![webView _pdfHUDs].count)
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 0, 0, 0, 0);
    while ([webView _pdfHUDs].anyObject.frame.size.width != 400)
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 10, 28, 400, 150);

    [webView setPageZoom:1.4];
    while ([webView _pdfHUDs].anyObject.frame.size.width != 560)
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 14, 40, 560, 210);
}
#endif

TEST(PDFHUD, NestedIFrames)
{
    auto handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id<WKURLSchemeTask> task) {
        auto htmlResponse = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
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
            NSData *data = pdfData();
            auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"application/pdf" expectedContentLength:data.length textEncodingName:nil]);
            [task didReceiveResponse:response.get()];
            [task didReceiveData:data];
            [task didFinish];
        }
    };

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test"];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///main.html"]]];
    EXPECT_EQ([webView _pdfHUDs].count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 20, 20, 300, 150);
    
    [webView evaluateJavaScript:@"document.body.removeChild(parentframe)" completionHandler:nil];
    while ([webView _pdfHUDs].count)
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(PDFHUD, IFrame3DTransform)
{
    auto handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id<WKURLSchemeTask> task) {
        auto htmlResponse = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            const char* html = "<iframe src='test.pdf' height=500 width=500 style='transform:rotateY(235deg);'></iframe>";
            [task didReceiveResponse:htmlResponse.get()];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        } else {
            EXPECT_WK_STREQ(task.request.URL.path, "/test.pdf");
            NSData *data = pdfData();
            auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"application/pdf" expectedContentLength:data.length textEncodingName:nil]);
            [task didReceiveResponse:response.get()];
            [task didReceiveData:data];
            [task didFinish];
        }
    };

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test"];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"test:///main.html"]]];
    EXPECT_EQ([webView _pdfHUDs].count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 403, 10, 500, 500);
}

TEST(PDFHUD, MultipleIFrames)
{
    auto handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id<WKURLSchemeTask> task) {
        auto htmlResponse = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        if ([task.request.URL.path isEqualToString:@"/main.html"]) {
            const char* html = "<iframe src='test.pdf' height=100 width=150></iframe><iframe src='test.pdf' height=123 width=134></iframe>";
            [task didReceiveResponse:htmlResponse.get()];
            [task didReceiveData:[NSData dataWithBytes:html length:strlen(html)]];
            [task didFinish];
        } else {
            EXPECT_WK_STREQ(task.request.URL.path, "/test.pdf");
            NSData *data = pdfData();
            auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"application/pdf" expectedContentLength:data.length textEncodingName:nil]);
            [task didReceiveResponse:response.get()];
            [task didReceiveData:data];
            [task didFinish];
        }
    };

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"test"];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
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

TEST(PDFHUD, LoadPDFTypeWithPluginsBlocked)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setOverrideContentSecurityPolicy:@"object-src 'none'"];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadData:pdfData() MIMEType:@"application/pdf" characterEncodingName:@"" baseURL:[NSURL URLWithString:@"https://www.apple.com/testPath"]];
    EXPECT_EQ([webView _pdfHUDs].count, 0u);
    [webView _test_waitForDidFinishNavigation];
    EXPECT_EQ([webView _pdfHUDs].count, 1u);
    checkFrame([webView _pdfHUDs].anyObject.frame, 0, 0, 800, 600);
}

#endif // ENABLE(LEGACY_PDFKIT_PLUGIN)

#if PLATFORM(MAC)

@interface PDFPrintUIDelegate : NSObject <WKUIDelegate>

- (NSSize)waitForPageSize;
- (_WKFrameHandle *)lastPrintedFrame;

@end

@implementation PDFPrintUIDelegate {
    NSSize _pageSize;
    bool _receivedSize;
    RetainPtr<_WKFrameHandle> _lastPrintedFrame;
}

- (void)_webView:(WKWebView *)webView printFrame:(_WKFrameHandle *)frame pdfFirstPageSize:(CGSize)size completionHandler:(void (^)(void))completionHandler
{
    _pageSize = size;
    _receivedSize = true;
    _lastPrintedFrame = frame;
    completionHandler();
}

- (NSSize)waitForPageSize
{
    _receivedSize = false;
    while (!_receivedSize)
        TestWebKitAPI::Util::spinRunLoop();
    return _pageSize;
}

- (_WKFrameHandle *)lastPrintedFrame
{
    return _lastPrintedFrame.get();
}

@end

TEST(PDF, PrintSize)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto schemeHandler = adoptNS([TestURLSchemeHandler new]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"test"];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([PDFPrintUIDelegate new]);
    [webView setUIDelegate:delegate.get()];

    schemeHandler.get().startURLSchemeTaskHandler = ^(WKWebView *, id<WKURLSchemeTask> task) {
        auto url = task.request.URL;
        NSData *data;
        NSString *mimeType;
        if ([url.path isEqualToString:@"/main.html"]) {
            mimeType = @"text/html";
            const char* html = "<br/><iframe src='test.pdf' id='pdfframe'></iframe>";
            data = [NSData dataWithBytes:html length:strlen(html)];
        } else if ([url.path isEqualToString:@"/test.pdf"]) {
            mimeType = @"application/pdf";
            data = pdfData();
        } else {
            EXPECT_WK_STREQ(url.path, "/test_print.pdf");
            mimeType = @"application/pdf";
            data = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"test_print" withExtension:@"pdf"]];
        }
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:url MIMEType:mimeType expectedContentLength:data.length textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:data];
        [task didFinish];
    };

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

TEST(PDF, SetPageZoomFactorDoesNotBailIncorrectly)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:adoptNS([WKWebViewConfiguration new]).get()]);
    [webView loadData:pdfData() MIMEType:@"application/pdf" characterEncodingName:@"" baseURL:[NSURL URLWithString:@"https://www.apple.com/testPath"]];
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

#endif
