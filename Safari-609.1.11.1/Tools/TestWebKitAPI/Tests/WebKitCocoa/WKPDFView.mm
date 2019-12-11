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

#include "config.h"

#import "ClassMethodSwizzler.h"
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <wtf/RetainPtr.h>

#if HAVE(PDFKIT) && PLATFORM(IOS)

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
    [configuration _setAlwaysRunsAtForegroundPriority:YES];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Load a PDF, so we install a WKPDFView.
    [webView loadData:[NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"test" withExtension:@"pdf" subdirectory:@"TestWebKitAPI.resources"]] MIMEType:@"application/pdf" characterEncodingName:@"" baseURL:[NSURL URLWithString:@"https://www.apple.com/0"]];
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
