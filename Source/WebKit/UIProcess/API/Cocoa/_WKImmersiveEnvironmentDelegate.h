/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <WebKit/WKFoundation.h>

NS_ASSUME_NONNULL_BEGIN

// FIXME: This SPI should become an API - rdar://problem/164244457

WK_SWIFT_UI_ACTOR
WK_API_AVAILABLE(visionos(WK_XROS_TBA))
@protocol _WKImmersiveEnvironmentDelegate <NSObject>

#if (defined(TARGET_OS_VISION) && TARGET_OS_VISION)
- (void)webView:(WKWebView *)webView allowImmersiveEnvironmentFromURL:(NSURL *)url completion:(void (^)(bool allow))completion NS_SWIFT_ASYNC_NAME(webView(_:allowImmersiveEnvironmentFromURL:)) WK_API_AVAILABLE(visionos(WK_XROS_TBA));
- (void)webView:(WKWebView *)webView presentImmersiveEnvironment:(UIView *)environmentView completion:(void (^)(NSError * _Nullable error))completion NS_SWIFT_ASYNC_NAME(webView(_:presentImmersiveEnvironment:)) WK_API_AVAILABLE(visionos(WK_XROS_TBA));
- (void)webView:(WKWebView *)webView dismissImmersiveEnvironment:(void (^)(void))completion NS_SWIFT_ASYNC_NAME(webViewDismissImmersiveEnvironment(_:)) WK_API_AVAILABLE(visionos(WK_XROS_TBA));
#endif

@end

NS_ASSUME_NONNULL_END
