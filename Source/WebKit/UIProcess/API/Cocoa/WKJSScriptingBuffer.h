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

/*! A WKJSScriptingBuffer object exposes an application controlled data buffer to JavaScript.
 @discussion JavaScript has access to various WebKit extensions via the `window.webkit` object.
 One such feature is `window.webkit.buffers` which provides access to named data buffers that can be
 provided by the WebKit application.

 To provide a data buffer to JavaScript the application first creates a `WKJSScriptingBuffer` object.
 It then adds it to the appropriate `WKUserContentController` with an application provided name.

 For example, if the application creates a `WKJSScriptingBuffer` and adds it to a web view's `WKUserContentController`
 with the name `"mybuffer"`, then JavaScript can access it by referencing `window.webkit.buffers.mybuffer`
 */
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA))
@interface WKJSScriptingBuffer : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/*! @abstract Initializes the newly created `WKJSScriptingBuffer` object with the passed in `NSData` data
 @param data The data to provide to the `window.webkit.buffers` JavaScript object.
*/
- (nullable instancetype)initWithData:(NSData *)data;

@end

NS_ASSUME_NONNULL_END
