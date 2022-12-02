/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import <WebKit/_WKWebExtensionControllerDelegate.h>

NS_ASSUME_NONNULL_BEGIN

WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@protocol _WKWebExtensionControllerDelegatePrivate <_WKWebExtensionControllerDelegate>
@optional

/*!
 @abstract Delegate for the `browser.test.assertTrue()`, `browser.test.assertFalse()`, `browser.test.assertThrows()`, and `browser.test.assertRejects()`  JavaScript testing APIs.
 @discussion Default implementation logs a message to the system console when `result` is `NO`.
 */
- (void)_webExtensionController:(_WKWebExtensionController *)controller recordTestAssertionResult:(BOOL)result withMessage:(NSString *)message andSourceURL:(NSString *)sourceURL lineNumber:(unsigned)lineNumber forExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Delegate for the `browser.test.assertEq()` and `browser.test.assertDeepEq()` JavaScript testing APIs.
 @discussion Default implementation logs a message to the system console when `result` is `NO`.
 */
- (void)_webExtensionController:(_WKWebExtensionController *)controller recordTestEqualityResult:(BOOL)result expectedValue:(NSString *)expectedValue actualValue:(NSString *)actualValue withMessage:(NSString *)message andSourceURL:(NSString *)sourceURL lineNumber:(unsigned)lineNumber forExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Delegate for the `browser.test.log()` JavaScript testing API.
 @discussion Default implementation always logs the message to the system console.
 */
- (void)_webExtensionController:(_WKWebExtensionController *)controller recordTestMessage:(NSString *)message andSourceURL:(NSString *)sourceURL lineNumber:(unsigned)lineNumber forExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Delegate for the `browser.test.yield()` JavaScript testing API.
 @discussion Default implementation always logs the message to the system console. Test harnesses should use this to exit the run loop to preform other work before resuming extension execution.
 */
- (void)_webExtensionController:(_WKWebExtensionController *)controller recordTestYieldedWithMessage:(NSString *)message andSourceURL:(NSString *)sourceURL lineNumber:(unsigned)lineNumber forExtensionContext:(_WKWebExtensionContext *)context;

/*!
 @abstract Delegate for the `browser.test.notifyPass()` and `browser.test.notifyFail()` JavaScript testing APIs.
 @discussion Default implementation logs a message to the system console when `result` is `NO`. Test harnesses should use this to exit the run loop and record a test pass or failure.
 */
- (void)_webExtensionController:(_WKWebExtensionController *)controller recordTestFinishedWithResult:(BOOL)result message:(NSString *)message andSourceURL:(NSString *)sourceURL lineNumber:(unsigned)lineNumber forExtensionContext:(_WKWebExtensionContext *)context;

@end

NS_ASSUME_NONNULL_END
