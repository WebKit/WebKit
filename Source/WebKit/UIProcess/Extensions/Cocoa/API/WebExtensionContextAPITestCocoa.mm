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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "WebExtensionController.h"
#import "_WKWebExtensionControllerDelegatePrivate.h"
#import "_WKWebExtensionControllerInternal.h"

namespace WebKit {

void WebExtensionContext::testResult(bool result, String message, String sourceURL, unsigned lineNumber)
{
    if (!isLoaded())
        return;

    auto delegate = (id<_WKWebExtensionControllerDelegatePrivate>)extensionController()->wrapper().delegate;
    if ([delegate respondsToSelector:@selector(_webExtensionController:recordTestAssertionResult:withMessage:andSourceURL:lineNumber:forExtensionContext:)]) {
        [delegate _webExtensionController:extensionController()->wrapper() recordTestAssertionResult:result withMessage:message andSourceURL:sourceURL lineNumber:lineNumber forExtensionContext:wrapper()];
        return;
    }

    if (result)
        return;

    if (message.isEmpty())
        message = "(no message)"_s;

    NSLog(@"EXTENSION TEST ASSERTION FAILED: %@ (%@:%u)", (NSString *)message, (NSString *)sourceURL, lineNumber);
}

void WebExtensionContext::testEqual(bool result, String expectedValue, String actualValue, String message, String sourceURL, unsigned lineNumber)
{
    if (!isLoaded())
        return;

    auto delegate = (id<_WKWebExtensionControllerDelegatePrivate>)extensionController()->wrapper().delegate;
    if ([delegate respondsToSelector:@selector(_webExtensionController:recordTestEqualityResult:expectedValue:actualValue:withMessage:andSourceURL:lineNumber:forExtensionContext:)]) {
        [delegate _webExtensionController:extensionController()->wrapper() recordTestEqualityResult:result expectedValue:expectedValue actualValue:actualValue withMessage:message andSourceURL:sourceURL lineNumber:lineNumber forExtensionContext:wrapper()];
        return;
    }

    if (result)
        return;

    if (message.isEmpty())
        message = "Expected equality of these values"_s;

    NSLog(@"EXTENSION TEST EQUALITY FAILED: %@: %@ !== %@ (%@:%u)", (NSString *)message, (NSString *)expectedValue, (NSString *)actualValue, (NSString *)sourceURL, lineNumber);
}

void WebExtensionContext::testMessage(String message, String sourceURL, unsigned lineNumber)
{
    if (!isLoaded())
        return;

    auto delegate = (id<_WKWebExtensionControllerDelegatePrivate>)extensionController()->wrapper().delegate;
    if ([delegate respondsToSelector:@selector(_webExtensionController:recordTestMessage:andSourceURL:lineNumber:forExtensionContext:)]) {
        [delegate _webExtensionController:extensionController()->wrapper() recordTestMessage:message andSourceURL:sourceURL lineNumber:lineNumber forExtensionContext:wrapper()];
        return;
    }

    if (message.isEmpty())
        message = "(no message)"_s;

    NSLog(@"EXTENSION TEST MESSAGE: %@ (%@:%u)", (NSString *)message, (NSString *)sourceURL, lineNumber);
}

void WebExtensionContext::testYielded(String message, String sourceURL, unsigned lineNumber)
{
    if (!isLoaded())
        return;

    auto delegate = (id<_WKWebExtensionControllerDelegatePrivate>)extensionController()->wrapper().delegate;
    if ([delegate respondsToSelector:@selector(_webExtensionController:recordTestYieldedWithMessage:andSourceURL:lineNumber:forExtensionContext:)]) {
        [delegate _webExtensionController:extensionController()->wrapper() recordTestYieldedWithMessage:message andSourceURL:sourceURL lineNumber:lineNumber forExtensionContext:wrapper()];
        return;
    }

    if (message.isEmpty())
        message = "(no message)"_s;

    NSLog(@"EXTENSION TEST YIELDED: %@ (%@:%u)", (NSString *)message, (NSString *)sourceURL, lineNumber);
}

void WebExtensionContext::testFinished(bool result, String message, String sourceURL, unsigned lineNumber)
{
    if (!isLoaded())
        return;

    auto delegate = (id<_WKWebExtensionControllerDelegatePrivate>)extensionController()->wrapper().delegate;
    if ([delegate respondsToSelector:@selector(_webExtensionController:recordTestFinishedWithResult:message:andSourceURL:lineNumber:forExtensionContext:)]) {
        [delegate _webExtensionController:extensionController()->wrapper() recordTestFinishedWithResult:result message:message andSourceURL:sourceURL lineNumber:lineNumber forExtensionContext:wrapper()];
        return;
    }

    if (result)
        return;

    if (message.isEmpty())
        message = "(no message)"_s;

    NSLog(@"EXTENSION TEST FAILED: %@ (%@:%u)", (NSString *)message, (NSString *)sourceURL, lineNumber);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
