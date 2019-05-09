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
#import "PlatformUtilities.h"
#import "WTFStringUtilities.h"

#import <Carbon/Carbon.h>
#import <WebKit/WebViewPrivate.h>
#import <wtf/RetainPtr.h>

extern "C" void JSSynchronousGarbageCollectForDebugging(JSContextRef);

#if JSC_OBJC_API_ENABLED

static bool didFinishLoad = false;
static bool didClose = false;
static RetainPtr<WebView> webView;

@interface DeallocWebViewInEventListenerLoadDelegate : NSObject <WebFrameLoadDelegate>
@end

@implementation DeallocWebViewInEventListenerLoadDelegate
- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}
@end

@interface DeallocWebViewInEventListener : NSObject <DOMEventListener>
@end

@implementation DeallocWebViewInEventListener
- (void)handleEvent:(DOMEvent *)event
{

}

- (void)dealloc
{
    webView = nullptr;
    [super dealloc];
    didClose = true;
}
@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, DeallocWebViewInEventListener)
{
    {
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) frameName:nil groupName:nil]);
        auto loadDelegate = adoptNS([[DeallocWebViewInEventListenerLoadDelegate alloc] init]);
        webView.get().frameLoadDelegate = loadDelegate.get();

        [[webView mainFrame] loadHTMLString:@"<html><body></body></html>" baseURL:nil];
        Util::run(&didFinishLoad);

        auto listener = adoptNS([[DeallocWebViewInEventListener alloc] init]);
        [[[webView mainFrameDocument] body] addEventListener:@"keypress" listener:listener.get() useCapture:NO];
        [[[webView mainFrameDocument] body] addEventListener:@"keypress" listener:nullptr useCapture:NO];
        listener = nullptr;
        [webView close];
        [pool drain];
    }
    Util::run(&didClose);
}

} // namespace TestWebKitAPI

#endif // ENABLE(JSC_OBJC_API)
