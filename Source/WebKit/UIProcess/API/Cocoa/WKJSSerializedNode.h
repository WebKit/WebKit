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

NS_ASSUME_NONNULL_BEGIN

/*! A `WKJSSerializedNode` object contains the serialized representation of a DOM node
 @discussion There are various ways that JavaScript executing inside web content results in some return value
 being passed up to the WebKit application.
 Examples include calls to `[WKWebView evaluateJavaScript:...]`, `[WKWebView callAsyncJavaScript:...]`, and the
 body of a `WKScriptMessage`.

 When application JavaScript returns a JavaScript value, the default behavior is to try to convert it to a foundational type.
 e.g. a JavaScript Number becomes an NSNumber, or a JavaScript array becomes an NSArray, etc.

 When the return value is a DOM node, the default conversion is to "stringify" it and return that to the application as an NSString.
 If the JavaScript instead calls `window.webkit.serializeNode(...)` then WebKit will create a serialized representation of
 that node as the return value.

 The node is an opaque object as far as the application is concerned, but it can be used as an argument to future JavaScript programs
 via `[WKWebView callAsyncJavaScript:...]`

 Unlike `WKJSHandle` - which keeps an actual JavaScript object alive in its originating context - a `WKJSSerializedNode` is not attached
 to a live JavaScript object, and it can be used as an argument to a JavaScript program running in any context.
 e.g. In a different frame, or after a navigation.
 */
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA))
@interface WKJSSerializedNode : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
