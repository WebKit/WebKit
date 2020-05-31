/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#if HAVE(UIWEBVIEW)

#import "PlatformUtilities.h"
#import <JavaScriptCore/JSVirtualMachine.h>
#import <JavaScriptCore/JSVirtualMachineInternal.h>
#import <UIKit/UIKit.h>
#import <WebCore/WebCoreThread.h>
#import <stdlib.h>
#import <wtf/RetainPtr.h>

@interface WebGLPrepareDisplayOnWebThreadDelegate : NSObject <UIWebViewDelegate> {
}
@end

static bool didFinishLoad = false;
static bool didFinishPainting = false;
static bool isReady = false;

@implementation WebGLPrepareDisplayOnWebThreadDelegate

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)webViewDidFinishLoad:(UIWebView *)webView
{
    didFinishLoad = true;
}

- (BOOL)webView:(UIWebView *)webView shouldStartLoadWithRequest:(NSURLRequest *)request navigationType:(UIWebViewNavigationType)navigationType
{
    if ([request.URL.scheme isEqualToString:@"callback"] && [request.URL.resourceSpecifier isEqualToString:@"didFinishPainting"]) {
        didFinishPainting = true;
        return NO;
    }

    if ([request.URL.scheme isEqualToString:@"callback"] && [request.URL.resourceSpecifier isEqualToString:@"isReady"]) {
        isReady = true;
        return NO;
    }

    return YES;
}
IGNORE_WARNINGS_END

@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, WebGLPrepareDisplayOnWebThread)
{
    RetainPtr<UIWindow> uiWindow = adoptNS([[UIWindow alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    RetainPtr<UIWebView> uiWebView = adoptNS([[UIWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [uiWindow addSubview:uiWebView.get()];

    RetainPtr<WebGLPrepareDisplayOnWebThreadDelegate> uiDelegate = adoptNS([[WebGLPrepareDisplayOnWebThreadDelegate alloc] init]);
    uiWebView.get().delegate = uiDelegate.get();

    NSURL *url = [[NSBundle mainBundle] URLForResource:@"webgl" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSLog(@"Loading %@", url);
    [uiWebView loadRequest:[NSURLRequest requestWithURL:url]];

    Util::run(&didFinishLoad);
    Util::run(&isReady);

    RetainPtr<JSContext> jsContext = [uiWebView valueForKeyPath:@"documentView.webView.mainFrame.javaScriptContext"];

    EXPECT_TRUE(!!jsContext.get());

    RetainPtr<JSVirtualMachine> jsVM = [jsContext virtualMachine];
    EXPECT_TRUE([jsVM isWebThreadAware]);

    // Release WebThread lock.
    Util::spinRunLoop(1);

    EXPECT_FALSE(WebThreadIsLocked());

    [jsContext evaluateScript:@"loadColorIntoTexture(128, 128, 255, 255)"];
    [jsContext evaluateScript:@"draw()"];
    Util::run(&didFinishPainting);

}

}

#endif
