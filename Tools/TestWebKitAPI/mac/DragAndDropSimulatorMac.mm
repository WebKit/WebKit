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
#import "DragAndDropSimulator.h"

#if ENABLE(DRAG_SUPPORT) && PLATFORM(MAC) && WK_API_ENABLED

#import "TestWKWebView.h"

@implementation DragAndDropSimulator {
    RetainPtr<TestWKWebView> _webView;
}

- (instancetype)initWithWebView:(TestWKWebView *)webView
{
    if (self = [super init]) {
        _webView = webView;
        [_webView setUIDelegate:self];
        [_webView _setInputDelegate:self];
    }
    return self;
}

- (void)dealloc
{
    if ([_webView UIDelegate] == self)
        [_webView setUIDelegate:nil];

    if ([_webView _inputDelegate] == self)
        [_webView _setInputDelegate:nil];

    [super dealloc];
}

- (void)runFrom:(CGPoint)startLocation to:(CGPoint)endLocation
{
    // FIXME: Add a mechanism to simulate dragging on macOS.
    UNUSED_PARAM(startLocation);
    UNUSED_PARAM(endLocation);
}

- (NSArray<_WKAttachment *> *)insertedAttachments
{
    return @[ ];
}

- (NSArray<_WKAttachment *> *)removedAttachments
{
    return @[ ];
}

- (DragAndDropPhase)phase
{
    return DragAndDropPhaseCancelled;
}

@end

#endif // ENABLE(DRAG_SUPPORT) && PLATFORM(MAC) && WK_API_ENABLED
