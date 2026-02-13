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

#import <WebKit/WKFoundation.h>

@class WKContentWorld;
@class WKFrameInfo;

NS_ASSUME_NONNULL_BEGIN

/*! A WKJSHandle object contains a reference to a JavaScript object.
 @discussion There are various ways that JavaScript executing inside web content results in some return value
 being passed up to the WebKit application.
 Examples include calls to `[WKWebView evaluateJavaScript:...]`, `[WKWebView callAsyncJavaScript:...]`, and the
 body of a `WKScriptMessage`.

 Usually these result objects are a foundational type, such as a number, string, array, dictionary, etc.
 In some environments the result object can be a `WKJSHandle` or be a container that contains one or more `WKJSHandle` objects.
 These environments are:
     - The JavaScript in question executed in a `WKContentWorld` that has `allowJSHandleCreation` set to `YES`
     - The most recent navigation in the `WKWebView` had `WKWebpagePreferences.allowsJSHandleCreationInPageWorld`
       set to `YES`

 JavaScript running in those environments can make a `WKJSHandle` instead of following normal serialization rules by calling
 `window.webkit.createJSHandle(...)` with the target value as an argument.

 Whatever JavaScript object the `WKJSHandle` represents, it will be protected from garbage collection for the lifetime of the `WKJSHandle`
 The `WKJSHandle` can also be used as an argument to future JavaScript run via `[WKWebView callAsyncJavaScript:...]`
 */
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA))
@interface WKJSHandle : NSObject<NSCopying>

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/*! @abstract The frame in which the `WKJSHandle` can be used.
 @discussion If the `WKJSHandle` is used as an argument to JavaScript in another frame or after the indicated frame has navigated,
 it will be interpreted as the JavaScript value `undefined`.
 */
@property (nonatomic, readonly, copy) WKFrameInfo *frame;

/*! @abstract The world in which the `WKJSHandle` can be used.
 @discussion If the `WKJSHandle` is used in another world it will be interpreted as the JavaScript value `undefined`.
 */
@property (nonatomic, readonly, weak) WKContentWorld *world;

/*! @abstract The frame represented by the JavaScript value.
 @discussion If the `WKJSHandle` represents a JavaScript Window proxy object, the result of this method will be a snapshot of the
 frame represented by that Window object.
 Otherwise the result of this method will be `nil`
 */
- (void)windowProxyFrameInfo:(void (^)(WKFrameInfo * _Nullable))completionHandler;

@end

NS_ASSUME_NONNULL_END
