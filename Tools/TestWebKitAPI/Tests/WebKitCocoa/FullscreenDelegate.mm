/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import <WebKit/WKPagePrivateMac.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKView.h>
#import <WebKit/WKViewPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFullscreenDelegate.h>
#import <wtf/RetainPtr.h>

static bool receivedWillEnterFullscreenMessage;
static bool receivedDidEnterFullscreenMessage;
static bool receivedWillExitFullscreenMessage;
static bool receivedDidExitFullscreenMessage;

@interface FullscreenDelegateMessageHandler : NSObject <WKScriptMessageHandler, _WKFullscreenDelegate>
@end

@implementation FullscreenDelegateMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    NSString *bodyString = (NSString *)[message body];
    if ([bodyString isEqualToString:@"load"])
        receivedLoadedMessage = true;
}

- (void)_webViewWillEnterFullscreen:(WKWebView *)view
{
    receivedWillEnterFullscreenMessage = true;
}

- (void)_webViewDidEnterFullscreen:(WKWebView *)view
{
    receivedDidEnterFullscreenMessage = true;
}

- (void)_webViewWillExitFullscreen:(WKWebView *)view
{
    receivedWillExitFullscreenMessage = true;
}

- (void)_webViewDidExitFullscreen:(WKWebView *)view
{
    receivedDidExitFullscreenMessage = true;
}
@end

namespace TestWebKitAPI {

TEST(Fullscreen, Delegate)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences]._fullScreenEnabled = YES;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    RetainPtr<FullscreenDelegateMessageHandler> handler = adoptNS([[FullscreenDelegateMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"fullscreenChangeHandler"];
    [webView _setFullscreenDelegate:handler.get()];

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    [window makeKeyAndOrderFront:nil];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"FullscreenDelegate" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&receivedLoadedMessage);

    NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:NSMakePoint(5, 5) modifierFlags:0 timestamp:0 windowNumber:window.get().windowNumber context:0 eventNumber:0 clickCount:0 pressure:0];
    [webView mouseDown:event];

    ASSERT_FALSE([webView _isInFullscreen]);
    TestWebKitAPI::Util::run(&receivedWillEnterFullscreenMessage);
    TestWebKitAPI::Util::run(&receivedDidEnterFullscreenMessage);

    ASSERT_TRUE([webView _isInFullscreen]);
    [webView mouseDown:event];
    TestWebKitAPI::Util::run(&receivedWillExitFullscreenMessage);
    TestWebKitAPI::Util::run(&receivedDidExitFullscreenMessage);

    ASSERT_FALSE([webView _isInFullscreen]);
}

} // namespace TestWebKitAPI

#endif
