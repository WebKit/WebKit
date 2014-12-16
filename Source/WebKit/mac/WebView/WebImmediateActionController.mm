/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "WebImmediateActionController.h"

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000

#import "WebFrameInternal.h"
#import "WebHTMLView.h"
#import "WebHTMLViewInternal.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <WebCore/EventHandler.h>
#import <WebCore/Frame.h>
#import <WebCore/NSImmediateActionGestureRecognizerSPI.h>
#import <objc/objc-class.h>
#import <objc/objc.h>

using namespace WebCore;

@implementation WebImmediateActionController

- (instancetype)initWithWebView:(WebView *)webView
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;
    _type = WebImmediateActionNone;

    return self;
}

- (void)webViewClosed
{
    _webView = nil;
}

- (void)_clearImmediateActionState
{
    _type = WebImmediateActionNone;
}

- (void)performHitTestAtPoint:(NSPoint)viewPoint
{
    Frame* coreFrame = core([[[[_webView _selectedOrMainFrame] frameView] documentView] _frame]);
    if (!coreFrame)
        return;
    _hitTestResult = coreFrame->eventHandler().hitTestResultAtPoint(IntPoint(viewPoint));
}

#pragma mark NSGestureRecognizerDelegate

- (void)immediateActionRecognizerWillPrepare:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (!_webView)
        return;

    if (immediateActionRecognizer.view != _webView)
        return;

    WebHTMLView *documentView = [[[_webView _selectedOrMainFrame] frameView] documentView];
    NSPoint locationInDocumentView = [immediateActionRecognizer locationInView:documentView];
    [self performHitTestAtPoint:locationInDocumentView];
}

- (void)immediateActionRecognizerWillBeginAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer.view != _webView)
        return;

    // FIXME: Add support for the types of functionality provided in Action menu's menuNeedsUpdate.
}

- (void)immediateActionRecognizerDidCancelAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer.view != _webView)
        return;

    [self _clearImmediateActionState];
}

- (void)immediateActionRecognizerDidCompleteAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer.view != _webView)
        return;

    // FIXME: Add support for the types of functionality provided in Action menu's willOpenMenu.
}

#pragma mark Immediate actions

- (void)_updateImmediateActionItem
{
    // FIXME: Implement. Inspect _hitTestResult to determine if there is an immediate action to take.
}

@end

#endif // PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
