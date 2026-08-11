/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestInputDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKInputDelegate.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(ModalDialogDuringOverlappingFocus, AlertNotDeferredAfterFIFOAsyncFocusCompletions)
{
    __block BlockPtr<void(BOOL)> firstCompletionHandler;
    __block BlockPtr<void(BOOL)> secondCompletionHandler;
    __block bool gotBothCompletions = false;

    RetainPtr inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:^(WKWebView *, id<_WKFocusedElementInfo>) {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [inputDelegate setFocusRequiresStrongPasswordAssistanceHandler:^(WKWebView *, id<_WKFocusedElementInfo>, void(^completionHandler)(BOOL)) {
        if (!firstCompletionHandler)
            firstCompletionHandler = makeBlockPtr(completionHandler);
        else {
            secondCompletionHandler = makeBlockPtr(completionHandler);
            gotBothCompletions = true;
        }
    }];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView focusInWindow];
    [webView _setInputDelegate:inputDelegate.get()];

    __block bool alertDelivered = false;
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    [uiDelegate setRunJavaScriptAlertPanelWithMessage:^(WKWebView *, NSString *, WKFrameInfo *, void(^completion)(void)) {
        alertDelivered = true;
        completion();
    }];
    [webView setUIDelegate:uiDelegate.get()];

    [webView synchronouslyLoadHTMLString:@""
        "<input id='a'></input>"
        "<input id='b'></input>"];

    // ElementDidFocus IPCs can be deferred until the next rendering update, so make two explicit
    // evaluateJavaScript calls for focus() to force any pending ElementDidFocus IPC to be sent.
    // Both focus() calls in one script would send only one IPC (the last).
    [webView evaluateJavaScript:@"document.getElementById('a').focus();" completionHandler:nil];
    [webView evaluateJavaScript:@"document.getElementById('b').focus();" completionHandler:nil];
    Util::run(&gotBothCompletions);

    // Invoke the completion handlers in FIFO order, releasing each block immediately afterwards. The
    // captured CompletionHandlerCallingScope only unwinds (resetting _isFocusingElementWithKeyboard and
    // firing any deferred modal dialog) when its block is released, mirroring how the delegate releases
    // the handler in production. Retaining the blocks past their invocation would keep the scopes alive
    // and hang the subsequent alert() forever.
    firstCompletionHandler(NO);
    firstCompletionHandler = nullptr;
    secondCompletionHandler(NO);
    secondCompletionHandler = nullptr;

    [webView evaluateJavaScript:@"alert('hello')" completionHandler:nil];
    Util::run(&alertDelivered);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
