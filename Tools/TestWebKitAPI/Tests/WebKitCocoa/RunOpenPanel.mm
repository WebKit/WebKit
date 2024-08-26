/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <AppKit/AppKit.h>
#import <WebKit/WKOpenPanelParametersPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

static bool fileSelected;
static NSString * const expectedFileName = @"这是中文";

@interface RunOpenPanelUIDelegate : NSObject <WKUIDelegate>
@end

@implementation RunOpenPanelUIDelegate

- (void)webView:(WKWebView *)webView runOpenPanelWithParameters:(WKOpenPanelParameters *)parameters initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSArray<NSURL *> *))completionHandler
{
    EXPECT_FALSE(parameters.allowsMultipleSelection);
    EXPECT_FALSE(parameters.allowsDirectories);
    EXPECT_EQ(0ull, parameters._acceptedMIMETypes.count);
    EXPECT_EQ(0ull, parameters._acceptedFileExtensions.count);
    completionHandler(@[ [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:expectedFileName]] ]);
    fileSelected = true;
}

@end

@interface FileInputTypeCancelEventUIDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation FileInputTypeCancelEventUIDelegate

- (void)webView:(WKWebView *)webView runOpenPanelWithParameters:(WKOpenPanelParameters *)parameters initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSArray<NSURL *> * URLs))completionHandler
{
    completionHandler(nil);
}

@end

namespace TestWebKitAPI {

TEST(WebKit, RunOpenPanelNonLatin1)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    auto uiDelegate = adoptNS([[RunOpenPanelUIDelegate alloc] init]);
    [webView setUIDelegate:uiDelegate.get()];
    [webView loadHTMLString:@"<!DOCTYPE html><input style='width: 100vw; height: 100vh;' type='file'>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];
    
    NSPoint clickPoint = NSMakePoint(50, 50);
    NSInteger windowNumber = [webView window].windowNumber;
    [webView mouseDown:[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:clickPoint modifierFlags:0 timestamp:0 windowNumber:windowNumber context:nil eventNumber:0 clickCount:1 pressure:1]];
    [webView mouseUp:[NSEvent mouseEventWithType:NSEventTypeLeftMouseUp location:clickPoint modifierFlags:0 timestamp:0 windowNumber:windowNumber context:nil eventNumber:0 clickCount:1 pressure:1]];
    Util::run(&fileSelected);
    
    __block bool testFinished = false;
    [webView evaluateJavaScript:@"document.getElementsByTagName('input')[0].files[0].name" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(expectedFileName, result);
        testFinished = true;
    }];
    Util::run(&testFinished);
}

TEST(WebKit, FileInputTypeCancelEvent)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    auto uiDelegate = adoptNS([[FileInputTypeCancelEventUIDelegate alloc] init]);
    [webView setUIDelegate:uiDelegate.get()];

    NSString *markup = @""
        "<script>"
        "    function loaded() {"
        "        setTimeout(() => {"
        "            const $file = document.getElementById('file');"
        "            $file.addEventListener('cancel', () => {"
        "                webkit.messageHandlers.testHandler.postMessage('cancel');"
        "            }, false);"
        "        }, 0);"
        "    }"
        "</script>"
        "<body style='width: 100vw; height: 100vh;' onload='loaded()'>"
        "   <input style='width: 100vw; height: 100vh;' type='file' id='file' name='file'>"
        "</body>";

    [webView loadHTMLString:markup baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    __block bool done = false;
    [webView performAfterReceivingMessage:@"cancel" action:^{
        done = true;
    }];

    NSPoint clickPoint = NSMakePoint(50, 50);
    [webView sendClickAtPoint:clickPoint];

    Util::run(&done);
}
    
} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
