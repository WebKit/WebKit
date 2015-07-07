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

#import <WebKit/WKFoundation.h>

#if WK_API_ENABLED

#if PLATFORM(IOS)

@class NSData;
@class UIScrollView;
@class UIView;
@class WKWebView;
@protocol NSObject;
@protocol UIScrollViewDelegate;
struct CGSize;
struct UIEdgeInsets;

// FIXME: This should be API (and probably should not be a UIScrollViewDelegate).
@protocol WKWebViewContentProvider <NSObject, UIScrollViewDelegate>

- (instancetype)web_initWithFrame:(CGRect) frame webView:(WKWebView *)webView;
- (void)web_setContentProviderData:(NSData *)data suggestedFilename:(NSString *)filename;
- (void)web_setMinimumSize:(CGSize)size;
- (void)web_setOverlaidAccessoryViewsInset:(CGSize)inset;
- (void)web_computedContentInsetDidChange;
- (void)web_setFixedOverlayView:(UIView *)fixedOverlayView;

@end

#endif // PLATFORM(IOS)

#endif // WK_API_ENABLED

