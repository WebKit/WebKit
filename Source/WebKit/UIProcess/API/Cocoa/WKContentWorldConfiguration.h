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

/*! @abstract A WKContentWorldConfiguration object allows you to specify custom behavior for a WKContentWorld instance.
@discussion WKContentWorldConfiguration allows applications to create WKContentWorld instances which have extra JavaScript
capabilities exposed to script in their environment. It does not change any default WebKit behaviors, nor change anything that web page
JavaScript can do. Only application JavaScript run in the created `WKContentWorld` will have different capabilities.

For example:
- If your scripts help provide autofill capabilities, you would want to set autofillEnabled to YES.
*/
WK_SWIFT_UI_ACTOR
NS_SWIFT_SENDABLE
NS_SWIFT_NAME(WKContentWorld.Configuration)
WK_CLASS_AVAILABLE(macos(27.0), ios(27.0), visionos(27.0))
@interface WKContentWorldConfiguration : NSObject<NSCopying, NSSecureCoding>

/*! @abstract A boolean value indicating whether every shadow root should be treated as open mode shadow root or not. */
@property (nonatomic) BOOL allowAccessingClosedShadowRoots;

/*! @abstract A boolean value indicating whether the capability to trigger autofill is exposed to scripts or not. */
@property (nonatomic, getter=isAutofillScriptingEnabled) BOOL autofillScriptingEnabled NS_SWIFT_NAME(autofillScriptingEnabled);

/*! @abstract A boolean value indicating whether the ability to attach user info on an element is exposed to scripts or not. */
@property (nonatomic, getter=isElementUserInfoEnabled) BOOL elementUserInfoEnabled NS_SWIFT_NAME(elementUserInfoEnabled);

/*! @abstract A boolean value indicating whether the behavior that elements with a name attribute overrides builtin methods on document object should be enabled or not. */
@property (nonatomic, getter=isLegacyBuiltinOverridesEnabled) BOOL legacyBuiltinOverridesEnabled NS_SWIFT_NAME(legacyBuiltinOverridesEnabled);

/*! @abstract A boolean indicating whether or not `window.webkit.createNodeSnapshot` is available.
 @discussion JavaScript can call `window.webkit.createNodeSnapshot` with a return value to create a `WKDOMNodeSnapshot`
 object for the application to use in future JavaScript programs.
 Refer to the `WKDOMNodeSnapshot` documentation for more information.
 */
@property (nonatomic, getter=isNodeSnapshotCreationEnabled) BOOL nodeSnapshotCreationEnabled NS_SWIFT_NAME(nodeSnapshotCreationEnabled);

/*! @abstract A boolean indicating whether or not `window.webkit.createJSHandle` is available. */
@property (nonatomic, getter=isJSHandleCreationEnabled, setter=setJSHandleCreationEnabled:) BOOL jsHandleCreationEnabled NS_SWIFT_NAME(jsHandleCreationEnabled);

/*! @abstract A boolean indicating whether the JavaScript in this world is visible to the Web Inspector. */
@property (nonatomic, getter=isInspectable) BOOL inspectable NS_SWIFT_NAME(inspectable);

@end

NS_ASSUME_NONNULL_END
