/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebFrame.h>
#import <wtf/RetainPtr.h>

static bool didFinishLoad;

@interface MessagePortFrameLoadDelegate : NSObject <WebFrameLoadDelegate> {
}
@end
@implementation MessagePortFrameLoadDelegate

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}

@end

namespace TestWebKitAPI {

TEST(MessagePort, Providers)
{
    // Loading a WebView that uses message ports guarantees that the default MessagePortChannelProviderImpl is set.
    auto wk1View = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) frameName:nil groupName:nil]);
    auto delegate = adoptNS([[MessagePortFrameLoadDelegate alloc] init]);
    [wk1View.get() setFrameLoadDelegate:delegate.get()];
    [[wk1View mainFrame] loadHTMLString:@"<script>new MessageChannel;</script>" baseURL:nil];

    Util::run(&didFinishLoad);

    // Now using a WKWebView to load content that uses message ports will use the WK2-style message ports.
    // This should not conflict with WK1-style message ports.
    // The conflict is caught by a RELEASE_ASSERT so, if this doesn't crash, it passes.
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<script>new MessageChannel;</script>"];
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
