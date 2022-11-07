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
#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import <Carbon/Carbon.h>
#import <JavaScriptCore/JSContext.h>
#import <JavaScriptCore/JSExport.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebViewPrivate.h>
#import <wtf/RetainPtr.h>

#if JSC_OBJC_API_ENABLED

static RetainPtr<WebView> webView;

@interface ClosingWebViewThenSendingItAKeyDownEventLoadDelegate : NSObject <WebFrameLoadDelegate>
@end

@implementation ClosingWebViewThenSendingItAKeyDownEventLoadDelegate
- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}
@end

@interface KeyboardEventListener : NSObject <DOMEventListener>
@end

@implementation KeyboardEventListener
- (void)handleEvent:(DOMEvent *)event
{
    if ([event isKindOfClass:[DOMKeyboardEvent class]])
        [webView _close];
}
@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, ClosingWebViewThenSendingItAKeyDownEvent)
{
    webView = adoptNS([[WebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) frameName:nil groupName:nil]);
    auto* webHTMLView = (WebHTMLView *)[[[webView mainFrame] frameView] documentView];
    
    EXPECT_TRUE([webView respondsToSelector:@selector(_close)]);
    EXPECT_TRUE([webHTMLView respondsToSelector:@selector(keyDown:)]);
    EXPECT_TRUE([webHTMLView respondsToSelector:@selector(keyUp:)]);

    auto loadDelegate = adoptNS([[ClosingWebViewThenSendingItAKeyDownEventLoadDelegate alloc] init]);
    webView.get().frameLoadDelegate = loadDelegate.get();
    
    [[webView mainFrame] loadHTMLString:@"<html><body contenteditable><script>function addKeyPressHandler() { document.body.addEventListener('keypress', function(event) { event.preventDefault(); }, true) }; document.body.focus();</script></body></html>" baseURL:nil];

    Util::run(&didFinishLoad);

    // First, add a native event listener that closes the WebView.
    auto listener = adoptNS([[KeyboardEventListener alloc] init]);
    [[[webView mainFrameDocument] body] addEventListener:@"keypress" listener:listener.get() useCapture:NO];

    // Second, add a JavaScript event handler that consumes the event.
    [[[webView mainFrame] javaScriptContext] evaluateScript:@"addKeyPressHandler()"];

    // Finally, fire an event that triggers both handlers. This should not crash.
    NSEvent *spaceBarDown = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                             location:NSMakePoint(5, 5)
                                        modifierFlags:0
                                            timestamp:GetCurrentEventTime()
                                         windowNumber:[[webView window] windowNumber]
                                              context:[NSGraphicsContext currentContext]
                                           characters:@" "
                          charactersIgnoringModifiers:@" "
                                            isARepeat:NO
                                              keyCode:0x31];
    [webHTMLView keyDown:spaceBarDown];
}

} // namespace TestWebKitAPI

#endif // ENABLE(JSC_OBJC_API)
