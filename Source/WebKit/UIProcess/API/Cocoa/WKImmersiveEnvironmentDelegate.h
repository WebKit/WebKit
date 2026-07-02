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

#import <Foundation/Foundation.h>
#import <WebKit/WKFoundation.h>

@class WKFrameInfo;
@class WKImmersiveEnvironment;
@class WKWebView;

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

/*! @abstract A protocol for managing immersive environment presentation in a web view.
 @discussion Implement the methods of this protocol to control authorization, presentation,
 and dismissal of immersive environments requested by websites.
 */
NS_SWIFT_UI_ACTOR
WK_API_AVAILABLE(visionos(WK_XROS_TBA))
WK_API_UNAVAILABLE(macos, ios)
@protocol WKImmersiveEnvironmentDelegate <NSObject>

/*! @abstract Asks the delegate whether to allow an immersive environment from the specified frame.
 @param webView The web view that received the immersive environment request.
 @param frame The frame information from the website requesting the immersive environment.
 @param completionHandler The completion handler you must invoke with the request's answer. `YES` to allow
 the environment presentation, or `NO` to deny it.
 */
- (void)webView:(WKWebView *)webView shouldAllowImmersiveEnvironmentFromFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL allow))completionHandler NS_SWIFT_ASYNC_NAME(webView(_:shouldAllowImmersiveEnvironmentFrom:)) WK_API_AVAILABLE(visionos(WK_XROS_TBA));

/*! @abstract Asks the delegate to present an immersive environment.
 @param webView The web view requesting presentation.
 @param environment The immersive environment to present.
 @param completionHandler The completion handler you must invoke once the presentation transition has completed.
 The error argument should be used in case the presentation failed and the environment couldn't be presented.
 */
- (void)webView:(WKWebView *)webView presentImmersiveEnvironment:(WKImmersiveEnvironment *)environment completionHandler:(void (^)(NSError * _Nullable error))completionHandler NS_SWIFT_ASYNC_NAME(webView(_:presentImmersiveEnvironment:)) WK_API_AVAILABLE(visionos(WK_XROS_TBA));

/*! @abstract Asks the delegate to dismiss an immersive environment.
 @param webView The web view requesting dismissal.
 @param environment The immersive environment to dismiss.
 @param completionHandler The completion handler you must invoke once the dismissal transition has completed.
 */
- (void)webView:(WKWebView *)webView dismissImmersiveEnvironment:(WKImmersiveEnvironment *)environment completionHandler:(void (^)(void))completionHandler NS_SWIFT_ASYNC_NAME(webView(_:dismissImmersiveEnvironment:)) WK_API_AVAILABLE(visionos(WK_XROS_TBA));

@end

NS_HEADER_AUDIT_END(nullability, sendability)
