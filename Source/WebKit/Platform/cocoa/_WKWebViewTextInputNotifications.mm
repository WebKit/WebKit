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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "_WKWebViewTextInputNotifications.h"

#if HAVE(REDESIGNED_TEXT_CURSOR) && PLATFORM(MAC)

#import "WebPageProxy.h"
#import "WebViewImpl.h"
#import <pal/spi/mac/NSTextInputContextSPI.h>

@implementation _WKWebViewTextInputNotifications {
    WeakPtr<WebKit::WebViewImpl> _webView;
    WebCore::CaretAnimatorType _caretType;
}

- (WebCore::CaretAnimatorType)caretType
{
    return _caretType;
}

- (instancetype)initWithWebView:(WebKit::WebViewImpl*)webView
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;
    _caretType = WebCore::CaretAnimatorType::Default;

    return self;
}

- (void)dictationDidStart
{
    if (_caretType == WebCore::CaretAnimatorType::Dictation) {
        if (_webView)
            _webView->page().setCaretBlinkingSuspended(false);
        return;
    }

    _caretType = WebCore::CaretAnimatorType::Dictation;
    if (_webView) {
        if (NSTextInputContext *context = _webView->inputContext())
            context.showsCursorAccessories = YES;

        _webView->page().setCaretAnimatorType(WebCore::CaretAnimatorType::Dictation);
    }
}

- (void)dictationDidEnd
{
    if (_caretType == WebCore::CaretAnimatorType::Default)
        return;

    _caretType = WebCore::CaretAnimatorType::Default;
    if (_webView)
        _webView->page().setCaretAnimatorType(WebCore::CaretAnimatorType::Default);
}

- (void)dictationDidPause
{
    if (_webView)
        _webView->page().setCaretBlinkingSuspended(true);
}

- (void)dictationDidResume
{
    if (_caretType == WebCore::CaretAnimatorType::Dictation) {
        if (_webView)
            _webView->page().setCaretBlinkingSuspended(false);
        return;
    }

    _caretType = WebCore::CaretAnimatorType::Dictation;
    if (_webView)
        _webView->page().setCaretAnimatorType(WebCore::CaretAnimatorType::Dictation);
}

@end

#endif
