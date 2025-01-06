/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionController.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "WKWebExtensionControllerDelegatePrivate.h"
#import "WKWebExtensionControllerInternal.h"

namespace WebKit {

void WebExtensionController::testResult(bool result, String message, String sourceURL, unsigned lineNumber)
{
    auto delegate = this->delegate();
    if ([delegate respondsToSelector:@selector(_webExtensionController:recordTestAssertionResult:withMessage:andSourceURL:lineNumber:)]) {
        [delegate _webExtensionController:wrapper() recordTestAssertionResult:result withMessage:message andSourceURL:sourceURL lineNumber:lineNumber];
        return;
    }

    if (message.isEmpty())
        message = "(no message)"_s;

    if (result) {
        RELEASE_LOG_INFO(Extensions, "Test assertion passed: %{public}@ (%{public}@:%{public}u)", (NSString *)message, (NSString *)sourceURL, lineNumber);
        return;
    }

    RELEASE_LOG_ERROR(Extensions, "Test assertion failed: %{public}@ (%{public}@:%{public}u)", (NSString *)message, (NSString *)sourceURL, lineNumber);
}

void WebExtensionController::testEqual(bool result, String expectedValue, String actualValue, String message, String sourceURL, unsigned lineNumber)
{
    auto delegate = this->delegate();
    if ([delegate respondsToSelector:@selector(_webExtensionController:recordTestEqualityResult:expectedValue:actualValue:withMessage:andSourceURL:lineNumber:)]) {
        [delegate _webExtensionController:wrapper() recordTestEqualityResult:result expectedValue:expectedValue actualValue:actualValue withMessage:message andSourceURL:sourceURL lineNumber:lineNumber];
        return;
    }

    if (message.isEmpty())
        message = "Expected equality of these values"_s;

    if (result) {
        RELEASE_LOG_INFO(Extensions, "Test equality passed: %{public}@: %{public}@ === %{public}@ (%{public}@:%{public}u)", (NSString *)message, (NSString *)expectedValue, (NSString *)actualValue, (NSString *)sourceURL, lineNumber);
        return;
    }

    RELEASE_LOG_ERROR(Extensions, "Test equality failed: %{public}@: %{public}@ !== %{public}@ (%{public}@:%{public}u)", (NSString *)message, (NSString *)expectedValue, (NSString *)actualValue, (NSString *)sourceURL, lineNumber);
}

void WebExtensionController::testMessage(String message, String sourceURL, unsigned lineNumber)
{
    auto delegate = this->delegate();
    if ([delegate respondsToSelector:@selector(_webExtensionController:recordTestMessage:andSourceURL:lineNumber:)]) {
        [delegate _webExtensionController:wrapper() recordTestMessage:message andSourceURL:sourceURL lineNumber:lineNumber];
        return;
    }

    if (message.isEmpty())
        message = "(no message)"_s;

    RELEASE_LOG_INFO(Extensions, "Test message: %{public}@ (%{public}@:%{public}u)", (NSString *)message, (NSString *)sourceURL, lineNumber);
}

void WebExtensionController::testYielded(String message, String sourceURL, unsigned lineNumber)
{
    auto delegate = this->delegate();
    if ([delegate respondsToSelector:@selector(_webExtensionController:recordTestYieldedWithMessage:andSourceURL:lineNumber:)]) {
        [delegate _webExtensionController:wrapper() recordTestYieldedWithMessage:message andSourceURL:sourceURL lineNumber:lineNumber];
        return;
    }

    if (message.isEmpty())
        message = "(no message)"_s;

    RELEASE_LOG_INFO(Extensions, "Test yielded: %{public}@ (%{public}@:%{public}u)", (NSString *)message, (NSString *)sourceURL, lineNumber);
}

void WebExtensionController::testSentMessage(String message, String argument, String sourceURL, unsigned lineNumber)
{
    auto delegate = this->delegate();
    if ([delegate respondsToSelector:@selector(_webExtensionController:receivedTestMessage:withArgument:andSourceURL:lineNumber:)]) {
        [delegate _webExtensionController:wrapper() receivedTestMessage:message withArgument:parseJSON(argument, JSONOptions::FragmentsAllowed) andSourceURL:sourceURL lineNumber:lineNumber];
        return;
    }

    RELEASE_LOG_INFO(Extensions, "Test sent message: %{public}@ %{public}@ (%{public}@:%{public}u)", (NSString *)message, (NSString *)argument, (NSString *)sourceURL, lineNumber);
}

void WebExtensionController::testFinished(bool result, String message, String sourceURL, unsigned lineNumber)
{
    auto delegate = this->delegate();
    if ([delegate respondsToSelector:@selector(_webExtensionController:recordTestFinishedWithResult:message:andSourceURL:lineNumber:)]) {
        [delegate _webExtensionController:wrapper() recordTestFinishedWithResult:result message:message andSourceURL:sourceURL lineNumber:lineNumber];
        return;
    }

    if (message.isEmpty())
        message = "(no message)"_s;

    if (result) {
        RELEASE_LOG_INFO(Extensions, "Test passed: %{public}@ (%{public}@:%{public}u)", (NSString *)message, (NSString *)sourceURL, lineNumber);
        return;
    }

    RELEASE_LOG_ERROR(Extensions, "Test failed: %{public}@ (%{public}@:%{public}u)", (NSString *)message, (NSString *)sourceURL, lineNumber);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
