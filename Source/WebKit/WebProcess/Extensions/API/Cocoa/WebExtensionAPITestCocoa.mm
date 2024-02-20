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
#import "WebExtensionAPITest.h"

#import "CocoaHelpers.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionControllerMessages.h"
#import "WebExtensionControllerProxy.h"
#import "WebExtensionEventListenerType.h"
#import "WebProcess.h"
#import <JavaScriptCore/APICast.h>
#import <JavaScriptCore/ScriptCallStack.h>
#import <JavaScriptCore/ScriptCallStackFactory.h>

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

static std::pair<String, unsigned> scriptLocation(JSContextRef context)
{
    auto callStack = Inspector::createScriptCallStack(toJS(context));
    if (const Inspector::ScriptCallFrame* frame = callStack->firstNonNativeCallFrame()) {
        auto sourceURL = frame->sourceURL();
        if (sourceURL.isEmpty())
            sourceURL = "global code"_s;
        return { sourceURL, frame->lineNumber() };
    }

    return { "unknown"_s, 0 };
}

void WebExtensionAPITest::notifyFail(JSContextRef context, NSString *message)
{
    auto location = scriptLocation(context);

    RefPtr page = toWebPage(context);
    if (!page)
        return;

    RefPtr webExtensionControllerProxy = page->webExtensionControllerProxy();
    if (!webExtensionControllerProxy)
        return;

    WebProcess::singleton().send(Messages::WebExtensionController::TestFinished(false, message, location.first, location.second), webExtensionControllerProxy->identifier());
}

void WebExtensionAPITest::notifyPass(JSContextRef context, NSString *message)
{
    auto location = scriptLocation(context);

    RefPtr page = toWebPage(context);
    if (!page)
        return;

    RefPtr webExtensionControllerProxy = page->webExtensionControllerProxy();
    if (!webExtensionControllerProxy)
        return;

    WebProcess::singleton().send(Messages::WebExtensionController::TestFinished(true, message, location.first, location.second), webExtensionControllerProxy->identifier());
}

void WebExtensionAPITest::yield(JSContextRef context, NSString *message)
{
    auto location = scriptLocation(context);

    RefPtr page = toWebPage(context);
    if (!page)
        return;

    RefPtr webExtensionControllerProxy = page->webExtensionControllerProxy();
    if (!webExtensionControllerProxy)
        return;

    WebProcess::singleton().send(Messages::WebExtensionController::TestYielded(message, location.first, location.second), webExtensionControllerProxy->identifier());
}

inline NSString *debugString(JSValue *value)
{
    if (value._isRegularExpression || value._isFunction)
        return value.toString;
    return value._toSortedJSONString ?: @"undefined";
}

void WebExtensionAPITest::log(JSContextRef context, JSValue *value)
{
    auto location = scriptLocation(context);

    RefPtr page = toWebPage(context);
    if (!page)
        return;

    RefPtr webExtensionControllerProxy = page->webExtensionControllerProxy();
    if (!webExtensionControllerProxy)
        return;

    WebProcess::singleton().send(Messages::WebExtensionController::TestMessage(debugString(value), location.first, location.second), webExtensionControllerProxy->identifier());
}

void WebExtensionAPITest::fail(JSContextRef context, NSString *message)
{
    assertTrue(context, false, message);
}

void WebExtensionAPITest::succeed(JSContextRef context, NSString *message)
{
    assertTrue(context, true, message);
}

void WebExtensionAPITest::assertTrue(JSContextRef context, bool actualValue, NSString *message)
{
    auto location = scriptLocation(context);

    RefPtr page = toWebPage(context);
    if (!page)
        return;

    RefPtr webExtensionControllerProxy = page->webExtensionControllerProxy();
    if (!webExtensionControllerProxy)
        return;

    WebProcess::singleton().send(Messages::WebExtensionController::TestResult(actualValue, message, location.first, location.second), webExtensionControllerProxy->identifier());
}

void WebExtensionAPITest::assertFalse(JSContextRef context, bool actualValue, NSString *message)
{
    assertTrue(context, !actualValue, message);
}

void WebExtensionAPITest::assertDeepEq(JSContextRef context, JSValue *actualValue, JSValue *expectedValue, NSString *message)
{
    NSString *expectedJSONValue = debugString(expectedValue);
    NSString *actualJSONValue = debugString(actualValue);

    // FIXME: Comparing JSON is a quick attempt that works, but can still fail due to any non-JSON values.
    // See Firefox's implementation: https://searchfox.org/mozilla-central/source/toolkit/components/extensions/child/ext-test.js#78
    BOOL strictEqual = [expectedValue isEqualToObject:actualValue];
    BOOL deepEqual = strictEqual || [expectedJSONValue isEqualToString:actualJSONValue];

    auto location = scriptLocation(context);

    RefPtr page = toWebPage(context);
    if (!page)
        return;

    RefPtr webExtensionControllerProxy = page->webExtensionControllerProxy();
    if (!webExtensionControllerProxy)
        return;

    WebProcess::singleton().send(Messages::WebExtensionController::TestEqual(deepEqual, expectedJSONValue, actualJSONValue, message, location.first, location.second), webExtensionControllerProxy->identifier());
}

static NSString *combineMessages(NSString *messageOne, NSString *messageTwo)
{
    if (messageOne.length && messageTwo.length)
        return [[messageOne stringByAppendingString:@"\n"] stringByAppendingString:messageTwo];
    if (messageOne.length && !messageTwo.length)
        return messageOne;
    return messageTwo;
}

static void assertEquals(JSContextRef context, bool result, NSString *expectedString, NSString *actualString, NSString *message)
{
    auto location = scriptLocation(context);

    RefPtr page = toWebPage(context);
    if (!page)
        return;

    RefPtr webExtensionControllerProxy = page->webExtensionControllerProxy();
    if (!webExtensionControllerProxy)
        return;

    WebProcess::singleton().send(Messages::WebExtensionController::TestEqual(result, expectedString, actualString, message, location.first, location.second), webExtensionControllerProxy->identifier());
}

void WebExtensionAPITest::assertEq(JSContextRef context, JSValue *actualValue, JSValue *expectedValue, NSString *message)
{
    NSString *expectedJSONValue = debugString(expectedValue);
    NSString *actualJSONValue = debugString(actualValue);

    BOOL strictEqual = [expectedValue isEqualToObject:actualValue];
    if (!strictEqual && [expectedJSONValue isEqualToString:actualJSONValue])
        actualJSONValue = [actualJSONValue stringByAppendingString:@" (different)"];

    assertEquals(context, strictEqual, expectedJSONValue, actualJSONValue, message);
}

JSValue *WebExtensionAPITest::assertRejects(JSContextRef context, JSValue *promise, JSValue *expectedError, NSString *message)
{
    __block JSValue *resolveCallback;
    JSValue *resultPromise = [JSValue valueWithNewPromiseInContext:promise.context fromExecutor:^(JSValue *resolve, JSValue *reject) {
        resolveCallback = resolve;
    }];

    // Wrap in a native promise for consistency.
    promise = [JSValue valueWithNewPromiseResolvedWithResult:promise inContext:promise.context];

    [promise _awaitThenableResolutionWithCompletionHandler:^(JSValue *result, JSValue *error) {
        if (result || !error) {
            assertEquals(context, false, expectedError ? debugString(expectedError) : @"(any error)", result ? debugString(result) : @"(no error)", combineMessages(message, @"Promise did not reject with an error"));
            [resolveCallback callWithArguments:nil];
            return;
        }

        JSValue *errorMessageValue = error.isObject && [error hasProperty:@"message"] ? error[@"message"] : error;

        if (!expectedError) {
            assertEquals(context, true, @"(any error)", debugString(errorMessageValue), combineMessages(message, @"Promise rejected with an error"));
            [resolveCallback callWithArguments:nil];
            return;
        }

        if (expectedError._isRegularExpression) {
            JSValue *testResult = [expectedError invokeMethod:@"test" withArguments:@[ errorMessageValue ]];
            assertEquals(context, testResult.toBool, debugString(expectedError), debugString(errorMessageValue), combineMessages(message, @"Promise rejected with an error that didn't match the regular expression"));
            [resolveCallback callWithArguments:nil];
            return;
        }

        assertEquals(context, [expectedError isEqualWithTypeCoercionToObject:errorMessageValue], debugString(expectedError), debugString(errorMessageValue), combineMessages(message, @"Promise rejected with an error that didn't equal"));
        [resolveCallback callWithArguments:nil];
    }];

    return resultPromise;
}

JSValue *WebExtensionAPITest::assertResolves(JSContextRef context, JSValue *promise, NSString *message)
{
    __block JSValue *resolveCallback;
    JSValue *resultPromise = [JSValue valueWithNewPromiseInContext:promise.context fromExecutor:^(JSValue *resolve, JSValue *reject) {
        resolveCallback = resolve;
    }];

    // Wrap in a native promise for consistency.
    promise = [JSValue valueWithNewPromiseResolvedWithResult:promise inContext:promise.context];

    [promise _awaitThenableResolutionWithCompletionHandler:^(JSValue *result, JSValue *error) {
        if (!error) {
            succeed(context, @"Promise resolved without an error");
            [resolveCallback callWithArguments:@[ result ]];
            return;
        }

        JSValue *errorMessageValue = error.isObject && [error hasProperty:@"message"] ? error[@"message"] : error;
        notifyFail(context, combineMessages(message, [NSString stringWithFormat:@"Promise rejected with an error: %@", debugString(errorMessageValue)]));

        [resolveCallback callWithArguments:nil];
    }];

    return resultPromise;
}

void WebExtensionAPITest::assertThrows(JSContextRef context, JSValue *function, JSValue *expectedError, NSString *message)
{
    [function callWithArguments:@[ ]];

    JSValue *exceptionValue = function.context.exception;
    if (!exceptionValue) {
        assertEquals(context, false, expectedError ? debugString(expectedError) : @"(any exception)", @"(no exception)", combineMessages(message, @"Function did not throw an exception"));
        return;
    }

    JSValue *exceptionMessageValue = exceptionValue.isObject && [exceptionValue hasProperty:@"message"] ? exceptionValue[@"message"] : exceptionValue;

    // Clear the exception since it was caught.
    function.context.exception = nil;

    if (!expectedError) {
        assertEquals(context, true, @"(any exception)", debugString(exceptionMessageValue), combineMessages(message, @"Function threw an exception"));
        return;
    }

    if (expectedError._isRegularExpression) {
        JSValue *testResult = [expectedError invokeMethod:@"test" withArguments:@[ exceptionMessageValue ]];
        assertEquals(context, testResult.toBool, debugString(expectedError), debugString(exceptionMessageValue), combineMessages(message, @"Function threw an exception that didn't match the regular expression"));
        return;
    }

    assertEquals(context, [expectedError isEqualWithTypeCoercionToObject:exceptionMessageValue], debugString(expectedError), debugString(exceptionMessageValue), combineMessages(message, @"Function threw an exception that didn't equal"));
}

JSValue *WebExtensionAPITest::assertSafe(JSContextRef context, JSValue *function, NSString *message)
{
    JSValue *result = [function callWithArguments:@[ ]];

    JSValue *exceptionValue = function.context.exception;
    if (!exceptionValue) {
        succeed(context, @"Function did not throw an exception");
        return result;
    }

    // Clear the exception since it was caught.
    function.context.exception = nil;

    JSValue *exceptionMessageValue = exceptionValue.isObject && [exceptionValue hasProperty:@"message"] ? exceptionValue[@"message"] : exceptionValue;
    notifyFail(context, combineMessages(message, [NSString stringWithFormat:@"Function threw an exception: %@", debugString(exceptionMessageValue)]));

    return [JSValue valueWithUndefinedInContext:function.context];
}

JSValue *WebExtensionAPITest::assertSafeResolve(JSContextRef context, JSValue *function, NSString *message)
{
    JSValue *result = assertSafe(context, function, message);
    if (!result._isThenable)
        return result;

    return assertResolves(context, result, message);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
