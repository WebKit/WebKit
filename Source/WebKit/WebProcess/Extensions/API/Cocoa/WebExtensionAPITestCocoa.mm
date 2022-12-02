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

#import "WebExtensionContextMessages.h"
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
    WebProcess::singleton().send(Messages::WebExtensionContext::TestFinished(false, message, location.first, location.second), extensionContext().identifier());
}

void WebExtensionAPITest::notifyPass(JSContextRef context, NSString *message)
{
    auto location = scriptLocation(context);
    WebProcess::singleton().send(Messages::WebExtensionContext::TestFinished(true, message, location.first, location.second), extensionContext().identifier());
}

void WebExtensionAPITest::yield(JSContextRef context, NSString *message)
{
    auto location = scriptLocation(context);
    WebProcess::singleton().send(Messages::WebExtensionContext::TestYielded(message, location.first, location.second), extensionContext().identifier());
}

void WebExtensionAPITest::log(JSContextRef context, NSString *message)
{
    auto location = scriptLocation(context);
    WebProcess::singleton().send(Messages::WebExtensionContext::TestMessage(message, location.first, location.second), extensionContext().identifier());
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
    WebProcess::singleton().send(Messages::WebExtensionContext::TestResult(actualValue, message, location.first, location.second), extensionContext().identifier());
}

void WebExtensionAPITest::assertFalse(JSContextRef context, bool actualValue, NSString *message)
{
    assertTrue(context, !actualValue, message);
}

inline NSString *debugString(JSValue *value)
{
    if (value._regularExpression)
        return value.toString;
    return value._toSortedJSONString ?: @"undefined";
}

void WebExtensionAPITest::assertDeepEq(JSContextRef context, JSValue *expectedValue, JSValue *actualValue, NSString *message)
{
    NSString *expectedJSONValue = debugString(expectedValue);
    NSString *actualJSONValue = debugString(actualValue);

    // FIXME: Comparing JSON is a quick attempt that works, but can still fail due to any non-JSON values.
    // See Firefox's implementation: https://searchfox.org/mozilla-central/source/toolkit/components/extensions/child/ext-test.js#78
    BOOL strictEqual = [expectedValue isEqualToObject:actualValue];
    BOOL deepEqual = strictEqual || [expectedJSONValue isEqualToString:actualJSONValue];

    auto location = scriptLocation(context);
    WebProcess::singleton().send(Messages::WebExtensionContext::TestEqual(deepEqual, expectedJSONValue, actualJSONValue, message, location.first, location.second), extensionContext().identifier());
}

void WebExtensionAPITest::assertEq(JSContextRef context, JSValue *expectedValue, JSValue *actualValue, NSString *message)
{
    NSString *expectedJSONValue = debugString(expectedValue);
    NSString *actualJSONValue = debugString(actualValue);

    BOOL strictEqual = [expectedValue isEqualToObject:actualValue];
    if (!strictEqual && [expectedJSONValue isEqualToString:actualJSONValue])
        actualJSONValue = [actualJSONValue stringByAppendingString:@" (different)"];

    auto location = scriptLocation(context);
    WebProcess::singleton().send(Messages::WebExtensionContext::TestEqual(strictEqual, expectedJSONValue, actualJSONValue, message, location.first, location.second), extensionContext().identifier());
}

JSValue *WebExtensionAPITest::assertRejects(JSContextRef context, JSValue *promise, JSValue *expectedError, NSString *message)
{
    // Wrap in a native promise for consistency.
    promise = [JSValue valueWithNewPromiseResolvedWithResult:promise inContext:promise.context];

    [promise _awaitThenableResolutionWithCompletionHandler:^(JSValue *result, JSValue *error) {
        if (result || !error) {
            assertTrue(context, false, [NSString stringWithFormat:@"Promise resolved with a result (%@); expected an error (%@).", debugString(result), debugString(expectedError)]);
            return;
        }

        if (!expectedError)
            return;

        JSValue *errorMessageValue = error.isObject && [error hasProperty:@"message"] ? error[@"message"] : error;

        if (expectedError._regularExpression) {
            JSValue *testResult = [expectedError invokeMethod:@"test" withArguments:@[ errorMessageValue ]];
            assertTrue(context, testResult.toBool, [NSString stringWithFormat:@"Promise rejected with an error (%@) that didn't match %@.", debugString(errorMessageValue), debugString(expectedError)]);
            return;
        }

        assertTrue(context, [expectedError isEqualWithTypeCoercionToObject:errorMessageValue], [NSString stringWithFormat:@"Promise rejected with an error (%@) that didn't equal %@.", debugString(errorMessageValue), debugString(expectedError)]);
    }];

    return promise;
}

void WebExtensionAPITest::assertThrows(JSContextRef context, JSValue *function, JSValue *expectedError, NSString *message)
{
    [function callWithArguments:@[ ]];

    JSValue *exceptionValue = function.context.exception;
    if (!exceptionValue) {
        assertTrue(context, false, [NSString stringWithFormat:@"Function did not throw an exception; expected an error (%@).", debugString(expectedError)]);
        return;
    }

    if (!expectedError)
        return;

    JSValue *exceptionMessageValue = exceptionValue.isObject && [exceptionValue hasProperty:@"message"] ? exceptionValue[@"message"] : exceptionValue;

    if (expectedError._regularExpression) {
        JSValue *testResult = [expectedError invokeMethod:@"test" withArguments:@[ exceptionMessageValue ]];
        assertTrue(context, testResult.toBool, [NSString stringWithFormat:@"Function throw an exception (%@) that didn't match %@.", debugString(exceptionMessageValue), debugString(expectedError)]);
        return;
    }

    assertTrue(context, [expectedError isEqualWithTypeCoercionToObject:exceptionMessageValue], [NSString stringWithFormat:@"Function throw an exception (%@) that didn't equal %@.", debugString(exceptionMessageValue), debugString(expectedError)]);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
