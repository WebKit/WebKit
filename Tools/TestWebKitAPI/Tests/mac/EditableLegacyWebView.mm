/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import <WebKit/WebFrameLoadDelegate.h>
#import <WebKit/WebViewPrivate.h>

@interface EditableLegacyWebViewLoadDelegate : NSObject <WebFrameLoadDelegate>
@end

@implementation EditableLegacyWebViewLoadDelegate {
    bool _doneLoading;
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    _doneLoading = true;
}

- (void)waitForLoadToFinish
{
    TestWebKitAPI::Util::run(&_doneLoading);
}

@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, SelectionAppearanceUpdatesInEditableWebView)
{
    auto webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 300)]);
    auto delegate = adoptNS([EditableLegacyWebViewLoadDelegate new]);
    [webView setEditable:YES];
    [webView setFrameLoadDelegate:delegate.get()];
    [[webView mainFrame] loadHTMLString:@"<html><body>Hello world.<br>This is a test.</body>" baseURL:nil];
    [delegate waitForLoadToFinish];

    [webView stringByEvaluatingJavaScriptFromString:@"getSelection().setPosition(document.body, 0)"];
    [webView setTracksRepaints:YES];
    [webView selectAll:nil];

    Util::waitForConditionWithLogging([webView] {
        return [webView trackedRepaintRects].count >= 3;
    }, 3, @"Timed out waiting for repaint rects after selecting text");
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
