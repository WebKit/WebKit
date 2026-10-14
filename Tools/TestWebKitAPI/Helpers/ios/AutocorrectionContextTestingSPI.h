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

#pragma once

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPIForTesting.h"
#import <UIKit/UIKit.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

// The context that WKContentView gives UIKit with async text input is a BETextDocumentContext, which has no text of its
// own. It wraps the UIKit document context, which does. BrowserEngineKitSPI.h declares BETextDocumentContext, but it is
// empty in iOS simulator module builds, so declare the one property tests need on a protocol instead.
// UIKitSPIForTesting.h declares UIWKDocumentContext, UIWKAutocorrectionContext, and +[UIKeyboard usesInputSystemUI].
@protocol UIKitDocumentContextProviding <NSObject>
@property (nonatomic, readonly, strong) UIWKDocumentContext *_uikitDocumentContext;
@end

// WKContentView implements these, but it isn't exported. Tests send them to the UIView that TestWKWebView's
// textInputContentView returns instead.
@interface UIView (AutocorrectionContextTestingSPI)

// How UIKit asks with async text input, which is when the web view's hasAsyncTextInput is true.
- (void)requestTextContextForAutocorrectionWithCompletionHandler:(void (^)(id<UIKitDocumentContextProviding> _Nullable context))completionHandler;

// How UIKit asks without it. WKContentView release-asserts if it gets this with async text input.
- (void)requestAutocorrectionContextWithCompletionHandler:(void (^)(UIWKAutocorrectionContext * _Nullable context))completionHandler;

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // PLATFORM(IOS_FAMILY)
