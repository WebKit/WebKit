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

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

@class WKJSHandle;

/*! @abstract A WKHitTestResult object contains information about the content at a point in a webpage.
 @discussion A WKHitTestResult is a snapshot of a point in the webpage taken when the hit test was performed.
 It is not updated if the webpage changes afterwards.

 You can get a WKHitTestResult by calling `[WKWebView hitTestAtPoint:...]`, or from the `hitTestResult`
 property of a `WKNavigationAction` that was caused by a mouse event, such as the user clicking a link.
 */

NS_SWIFT_UI_ACTOR
NS_SWIFT_SENDABLE
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA))
@interface WKHitTestResult : NSObject <NSCopying>

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/*! @abstract A handle to the DOM node that was hit.
 @discussion The handle can be used as an argument to JavaScript run via `[WKWebView callAsyncJavaScript:...]`
 in the handle's `sourceFrame` and `contentWorld`.
 For results from `[WKWebView hitTestAtPoint:...]` the handle is in the content world which was hit tested.
 For results from a `WKNavigationAction` the handle is in `WKContentWorld.pageWorld`.
 Can be nil if no DOM node was hit.
 */
@property (nonatomic, readonly, nullable) WKJSHandle *domNodeHandle;

/*! @abstract The absolute URL of the link that contains the hit DOM node, or nil if the hit node is not inside a link.
 */
@property (nonatomic, readonly, copy, nullable) NSURL *absoluteLinkURL;

/*! @abstract The title of the link that contains the hit node, from the link's `title` attribute.
 @discussion This is nil if the hit node is not inside a link, or if the link has no title.
 It is an empty string if the link has a title that is empty.
 */
@property (nonatomic, readonly, copy, nullable) NSString *linkTitle;

/*! @abstract The text content of the link that contains the hit node.
 @discussion This is nil if the hit node is not inside a link.
 It is an empty string if the link has no text content.
 */
@property (nonatomic, readonly, copy, nullable) NSString *linkLabel;

@end

NS_HEADER_AUDIT_END(nullability, sendability)
