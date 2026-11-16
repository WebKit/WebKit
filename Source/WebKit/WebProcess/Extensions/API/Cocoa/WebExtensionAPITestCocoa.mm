/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Igalia, S.L. All rights reserved.
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
#import "WebExtensionControllerMessages.h"
#import "WebExtensionControllerProxy.h"
#import "WebExtensionUtilities.h"
#import "WebPage.h"
#import "WebProcess.h"

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

JSValueRef WebExtensionAPITest::assertRejects(JSContextRef context, JSValueRef promiseRef, JSValueRef expectedError, const String& message)
{
    __block JSValue *resolveCallback;
    __block JSValue *rejectCallback;
    JSValue *resultPromise = [JSValue valueWithNewPromiseInContext:toJSContext(context) fromExecutor:^(JSValue *resolve, JSValue *reject) {
        resolveCallback = resolve;
        rejectCallback = reject;
    }];

    // Wrap in a native promise for consistency.
    JSValue *promise = [JSValue valueWithNewPromiseResolvedWithResult:toJSValue(context, promiseRef) inContext:toJSContext(context)];

    if (!isThenable(context, promise.JSValueRef))
        return JSValueMakeNull(context);

    auto *nsMessage = message.createNSString().get();
    auto generation = beginAsyncAssertion();

    // A failed assertion rejects the returned promise, so awaiting it throws like the other assertions.
    auto promiseHandler = ^(JSValue *result, JSValue *error) {
        bool passed = true;
        String failureMessage;

        settleAsyncAssertion(generation, [&] {
            String falseException = nullString();

            if (result || !error) {
                passed = false;
                failureMessage = combineMessages(nsMessage, "Promise did not reject with an error"_s);
                assertEquals(context, false, expectedError ? debugString(context, expectedError) : "(any error)"_s, result ? debugString(context, result.JSValueRef) : "(no error)"_s, failureMessage, falseException);
                return;
            }

            JSValueRef errorMessageValue = error.isObject && [error hasProperty:@"message"] ? error[@"message"].JSValueRef : error.JSValueRef;

            // By default, JSValueRef is set to an undefined value in the JS implementation.
            // It should not be possible to get a null value here.
            if (JSValueIsUndefined(context, expectedError) || !expectedError) {
                assertEquals(context, true, "(any error)"_s, debugString(context, errorMessageValue), combineMessages(nsMessage, "Promise rejected with an error"_s), falseException);
                return;
            }

            if (isRegularExpression(context, expectedError)) {
                JSValueRef testResult = invokeMethod<1>(context, expectedError, "test"_s, { errorMessageValue });
                passed = JSValueToBoolean(context, testResult);
                failureMessage = combineMessages(nsMessage, "Promise rejected with an error that didn't match the regular expression"_s);
                assertEquals(context, passed, debugString(context, expectedError), debugString(context, errorMessageValue), failureMessage, falseException);
                return;
            }

            passed = JSValueIsEqual(context, expectedError, errorMessageValue, nullptr);
            failureMessage = combineMessages(nsMessage, "Promise rejected with an error that didn't equal"_s);
            assertEquals(context, passed, debugString(context, expectedError), debugString(context, errorMessageValue), failureMessage, falseException);
        });

        if (passed) {
            [resolveCallback callWithArguments:nil];
            return;
        }

        [rejectCallback callWithArguments:@[ [JSValue valueWithNewErrorFromMessage:failureMessage.createNSString().get() inContext:toJSContext(context)] ]];
    };

    [promise invokeMethod:@"then" withArguments:@[
        ^(JSValue *result) {
            promiseHandler(result, nil);
        },

        ^(JSValue *error) {
            promiseHandler(nil, error);
        },
    ]];

    return resultPromise.JSValueRef;
}

JSValueRef WebExtensionAPITest::assertResolves(JSContextRef context, JSValueRef promiseRef, const String& message)
{
    __block JSValue *resolveCallback;
    __block JSValue *rejectCallback;
    JSValue *resultPromise = [JSValue valueWithNewPromiseInContext:toJSContext(context) fromExecutor:^(JSValue *resolve, JSValue *reject) {
        resolveCallback = resolve;
        rejectCallback = reject;
    }];

    // Wrap in a native promise for consistency.
    JSValue *promise = [JSValue valueWithNewPromiseResolvedWithResult:toJSValue(context, promiseRef) inContext:toJSContext(context)];

    if (!isThenable(context, promiseRef))
        return JSValueMakeNull(context);

    // The handler runs after this function returns, so it needs its own copy of the message.
    String messageCopy = message;

    auto generation = beginAsyncAssertion();

    // A failed assertion rejects the returned promise, so awaiting it throws like the other assertions.
    auto promiseHandler = ^(JSValue *result, JSValue *error) {
        String failureMessage;

        settleAsyncAssertion(generation, [&] {
            if (!error) {
                succeed(context, @"Promise resolved without an error");
                return;
            }

            JSValue *errorMessageValue = error.isObject && [error hasProperty:@"message"] ? error[@"message"] : error;
            failureMessage = combineMessages(messageCopy, makeString("Promise rejected with an error: "_s, debugString(context, errorMessageValue.JSValueRef)));
            fail(context, failureMessage);
        });

        if (!error) {
            [resolveCallback callWithArguments:@[ result ]];
            return;
        }

        [rejectCallback callWithArguments:@[ [JSValue valueWithNewErrorFromMessage:failureMessage.createNSString().get() inContext:toJSContext(context)] ]];
    };

    [promise invokeMethod:@"then" withArguments:@[
        ^(JSValue *result) {
            promiseHandler(result, nil);
        },

        ^(JSValue *error) {
            promiseHandler(nil, error);
        },
    ]];

    return resultPromise.JSValueRef;
}

JSValueRef WebExtensionAPITest::addTest(JSContextRef context, JSValueRef testFunctionRef, String callingAPIName)
{
    if (!JSValueIsObject(context, testFunctionRef))
        return [JSValue valueWithNewPromiseRejectedWithReason:toErrorString(callingAPIName, nullString(), "Error creating a new test."_s).createNSString().get() inContext:toJSContext(context)].JSValueRef;

    JSValueRef testName = JSObjectGetProperty(context, JSValueToObject(context, testFunctionRef, nullptr), toJSString("name"_s).get(), nullptr);
    if (toString(context, testName).isEmpty())
        return [JSValue valueWithNewPromiseRejectedWithReason:toErrorString(callingAPIName, nullString(), "The supplied test function must be named."_s).createNSString().get() inContext:toJSContext(context)].JSValueRef;

    RefPtr page = toWebPage(context);
    if (!page)
        return [JSValue valueWithNewPromiseRejectedWithReason:toErrorString(callingAPIName, nullString(), "Error creating a new test."_s).createNSString().get() inContext:toJSContext(context)].JSValueRef;

    RefPtr webExtensionControllerProxy = page->webExtensionControllerProxy();
    if (!webExtensionControllerProxy)
        return [JSValue valueWithNewPromiseRejectedWithReason:toErrorString(callingAPIName, nullString(), "Error creating a new test."_s).createNSString().get() inContext:toJSContext(context)].JSValueRef;

    __block JSValue *resolveCallback;
    __block JSValue *rejectCallback;
    JSValue *resultPromise = [JSValue valueWithNewPromiseInContext:toJSContext(context) fromExecutor:^(JSValue *resolve, JSValue *reject) {
        resolveCallback = resolve;
        rejectCallback = reject;
    }];

    auto location = scriptLocation(context);
    auto webExtensionControllerIdentifier = webExtensionControllerProxy->identifier();

    JSValueRef resolveCallbackRef = resolveCallback.JSValueRef;
    JSValueRef rejectCallbackRef = rejectCallback.JSValueRef;

    m_testQueue.append({
        toString(context, testName),
        location,
        webExtensionControllerIdentifier,
        Protected(JSContextGetGlobalContext(context), testFunctionRef),
        Protected(JSContextGetGlobalContext(context), resolveCallbackRef),
        Protected(JSContextGetGlobalContext(context), rejectCallbackRef)
    });

    WebProcess::singleton().send(Messages::WebExtensionController::TestAdded(toString(context, testName), location.first, location.second), webExtensionControllerIdentifier);

    if (!m_runningTest) {
        m_runningTest = true;

        WorkQueue::mainSingleton().dispatch([this, protectedThis = Ref { *this }] {
            startNextTest();
        });
    }

    return resultPromise.JSValueRef;
}

void WebExtensionAPITest::startNextTest()
{
    auto test = m_testQueue.takeFirst();

    m_testInProgress = true;
    m_pendingAsyncAssertions = 0;
    ++m_testGeneration;

    WebProcess::singleton().send(Messages::WebExtensionController::TestStarted(test.testName, test.location.first, test.location.second), test.webExtensionControllerIdentifier);

    JSValueRef exception = nullptr;
    JSValueRef result = callObjectWithArguments<0>(test.testFunction.get(), test.testFunction.context().get(), { }, &exception);

    auto testComplete = [this, protectedThis = Ref { *this }, test](JSValue *result, JSValue *error) {
        bool hasPendingAssertions = m_pendingAsyncAssertions;

        m_testInProgress = false;
        m_pendingAsyncAssertions = 0;

        if (error || m_hitAssertion || hasPendingAssertions) {
            NSString *errorMessage;
            if (error) {
                JSValue *errorMessageValue = error.isObject && [error hasProperty:@"message"] ? error[@"message"] : error;
                errorMessage = debugString(test.testFunction.context().get(), errorMessageValue.JSValueRef).createNSString().get();
            } else if (!m_assertionMessage.isNull())
                errorMessage = m_assertionMessage.createNSString().get();
            else if (hasPendingAssertions)
                errorMessage = @"The test finished with an assertion promise still pending. Await browser.test.assertRejects() and browser.test.assertResolves().";

            errorMessage = errorMessage ? combineMessages("Promise rejected with an error: "_s, errorMessage).createNSString().get() : @"Promise rejected without an error";

            WebProcess::singleton().send(Messages::WebExtensionController::TestFinished(test.testName, false, errorMessage, test.location.first, test.location.second), test.webExtensionControllerIdentifier);

            callObjectWithArguments<0>(test.rejectCallback.get(), test.rejectCallback.context().get(), { });
        } else {
            WebProcess::singleton().send(Messages::WebExtensionController::TestFinished
            (test.testName, true, @"Promise resolved without an error.", test.location.first, test.location.second), test.webExtensionControllerIdentifier);
            callObjectWithArguments<1>(test.resolveCallback.get(), test.resolveCallback.context().get(), { result.JSValueRef });
        }

        m_hitAssertion = false;

        if (!m_testQueue.isEmpty())
            startNextTest();
        else
            m_runningTest = false;
    };

    if (isThenable(test.testFunction.context().get(), result)) {
        auto resolveBlock = ^(JSValue *result) {
            testComplete(result, nil);
        };

        auto rejectBlock = ^(JSValue *error) {
            testComplete(nil, error);
        };

        [toJSValue(test.testFunction.context().get(), result) invokeMethod:@"then" withArguments:@[ resolveBlock, rejectBlock ]];
    } else
        testComplete(toJSValue(test.testFunction.context().get(), result), exception ? toJSValue(test.testFunction.context().get(), exception) : nil);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
