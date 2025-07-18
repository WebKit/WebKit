/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#import "WebExtensionAPIWebPageNamespace.h"
#import "WebExtensionControllerMessages.h"
#import "WebExtensionControllerProxy.h"
#import "WebExtensionEventListenerType.h"
#import "WebFrame.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <JavaScriptCore/APICast.h>
#import <JavaScriptCore/ScriptCallStack.h>
#import <JavaScriptCore/ScriptCallStackFactory.h>
#import <WebCore/LocalFrameInlines.h>

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

    WebProcess::singleton().send(Messages::WebExtensionController::TestFinished(nullString(), false, message, location.first, location.second), webExtensionControllerProxy->identifier());
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

    WebProcess::singleton().send(Messages::WebExtensionController::TestFinished(nullString(), true, message, location.first, location.second), webExtensionControllerProxy->identifier());
}

void WebExtensionAPITest::sendMessage(JSContextRef context, NSString *message, JSValue *argument)
{
    auto location = scriptLocation(context);

    RefPtr page = toWebPage(context);
    if (!page)
        return;

    RefPtr webExtensionControllerProxy = page->webExtensionControllerProxy();
    if (!webExtensionControllerProxy)
        return;

    WebProcess::singleton().send(Messages::WebExtensionController::TestSentMessage(message, argument._toSortedJSONString, location.first, location.second), webExtensionControllerProxy->identifier());
}

WebExtensionAPIEvent& WebExtensionAPITest::onMessage()
{
    if (!m_onMessage)
        m_onMessage = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TestOnMessage);

    return *m_onMessage;
}

WebExtensionAPIEvent& WebExtensionAPITest::onTestStarted()
{
    if (!m_onTestStarted)
        m_onTestStarted = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TestOnTestStarted);

    return *m_onTestStarted;
}

WebExtensionAPIEvent& WebExtensionAPITest::onTestFinished()
{
    if (!m_onTestFinished)
        m_onTestFinished = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::TestOnTestFinished);

    return *m_onTestFinished;
}

JSValue *WebExtensionAPITest::runWithUserGesture(WebFrame& frame, JSValue *function)
{
    RefPtr coreFrame = frame.coreLocalFrame();
    WebCore::UserGestureIndicator gestureIndicator(WebCore::IsProcessingUserGesture::Yes, coreFrame ? coreFrame->document() : nullptr);

    return [function callWithArguments:@[ ]];
}

bool WebExtensionAPITest::isProcessingUserGesture()
{
    return WebCore::UserGestureIndicator::processingUserGesture();
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

    WebProcess::singleton().send(Messages::WebExtensionController::TestLogMessage(debugString(value), location.first, location.second), webExtensionControllerProxy->identifier());
}

void WebExtensionAPITest::fail(JSContextRef context, NSString *message)
{
    assertTrue(context, false, message, nil);
}

void WebExtensionAPITest::succeed(JSContextRef context, NSString *message)
{
    assertTrue(context, true, message, nil);
}

void WebExtensionAPITest::assertTrue(JSContextRef context, bool actualValue, NSString *message, NSString **outExceptionString)
{
    auto location = scriptLocation(context);

    RefPtr page = toWebPage(context);
    if (!page)
        return;

    RefPtr webExtensionControllerProxy = page->webExtensionControllerProxy();
    if (!webExtensionControllerProxy)
        return;

    recordAssertionIfNeeded(actualValue, message, location, outExceptionString);

    WebProcess::singleton().send(Messages::WebExtensionController::TestResult(actualValue, message, location.first, location.second), webExtensionControllerProxy->identifier());
}

void WebExtensionAPITest::assertFalse(JSContextRef context, bool actualValue, NSString *message, NSString **outExceptionString)
{
    assertTrue(context, !actualValue, message, outExceptionString);
}

void WebExtensionAPITest::assertDeepEq(JSContextRef context, JSValue *actualValue, JSValue *expectedValue, NSString *message, NSString **outExceptionString)
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

    recordAssertionIfNeeded(deepEqual, message, location, outExceptionString);

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

void WebExtensionAPITest::assertEquals(JSContextRef context, bool result, NSString *expectedString, NSString *actualString, NSString *message, NSString **outExceptionString)
{
    auto location = scriptLocation(context);

    RefPtr page = toWebPage(context);
    if (!page)
        return;

    RefPtr webExtensionControllerProxy = page->webExtensionControllerProxy();
    if (!webExtensionControllerProxy)
        return;

    recordAssertionIfNeeded(result, message, location, outExceptionString);

    WebProcess::singleton().send(Messages::WebExtensionController::TestEqual(result, expectedString, actualString, message, location.first, location.second), webExtensionControllerProxy->identifier());
}

void WebExtensionAPITest::assertEq(JSContextRef context, JSValue *actualValue, JSValue *expectedValue, NSString *message, NSString **outExceptionString)
{
    NSString *expectedJSONValue = debugString(expectedValue);
    NSString *actualJSONValue = debugString(actualValue);

    BOOL strictEqual = [expectedValue isEqualToObject:actualValue];
    if (!strictEqual && [expectedJSONValue isEqualToString:actualJSONValue])
        actualJSONValue = [actualJSONValue stringByAppendingString:@" (different)"];

    assertEquals(context, strictEqual, expectedJSONValue, actualJSONValue, message, outExceptionString);
}

JSValue *WebExtensionAPITest::assertRejects(JSContextRef context, JSValue *promise, JSValue *expectedError, NSString *message)
{
    __block JSValue *resolveCallback;
    __block JSValue *rejectCallback;
    JSValue *resultPromise = [JSValue valueWithNewPromiseInContext:promise.context fromExecutor:^(JSValue *resolve, JSValue *reject) {
        resolveCallback = resolve;
        rejectCallback = reject;
    }];

    // Wrap in a native promise for consistency.
    promise = [JSValue valueWithNewPromiseResolvedWithResult:promise inContext:promise.context];

    [promise _awaitThenableResolutionWithCompletionHandler:^(JSValue *result, JSValue *error) {
        if (result || !error) {
            assertEquals(context, false, expectedError ? debugString(expectedError) : @"(any error)", result ? debugString(result) : @"(no error)", combineMessages(message, @"Promise did not reject with an error"), nil);
            [rejectCallback callWithArguments:nil];
            return;
        }

        JSValue *errorMessageValue = error.isObject && [error hasProperty:@"message"] ? error[@"message"] : error;

        if (!expectedError) {
            assertEquals(context, true, @"(any error)", debugString(errorMessageValue), combineMessages(message, @"Promise rejected with an error"), nil);
            [resolveCallback callWithArguments:nil];
            return;
        }

        if (expectedError._isRegularExpression) {
            JSValue *testResult = [expectedError invokeMethod:@"test" withArguments:@[ errorMessageValue ]];
            assertEquals(context, testResult.toBool, debugString(expectedError), debugString(errorMessageValue), combineMessages(message, @"Promise rejected with an error that didn't match the regular expression"), nil);
            [resolveCallback callWithArguments:nil];
            return;
        }

        assertEquals(context, [expectedError isEqualWithTypeCoercionToObject:errorMessageValue], debugString(expectedError), debugString(errorMessageValue), combineMessages(message, @"Promise rejected with an error that didn't equal"), nil);
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
        fail(context, combineMessages(message, adoptNS([[NSString alloc] initWithFormat:@"Promise rejected with an error: %@", debugString(errorMessageValue)]).get()));

        [resolveCallback callWithArguments:nil];
    }];

    return resultPromise;
}

void WebExtensionAPITest::assertThrows(JSContextRef context, JSValue *function, JSValue *expectedError, NSString *message, NSString **outExceptionString)
{
    [function callWithArguments:@[ ]];

    JSValue *exceptionValue = function.context.exception;
    if (!exceptionValue) {
        assertEquals(context, false, expectedError ? debugString(expectedError) : @"(any exception)", @"(no exception)", combineMessages(message, @"Function did not throw an exception"), outExceptionString);
        return;
    }

    JSValue *exceptionMessageValue = exceptionValue.isObject && [exceptionValue hasProperty:@"message"] ? exceptionValue[@"message"] : exceptionValue;

    // Clear the exception since it was caught.
    function.context.exception = nil;

    if (!expectedError) {
        assertEquals(context, true, @"(any exception)", debugString(exceptionMessageValue), combineMessages(message, @"Function threw an exception"), outExceptionString);
        return;
    }

    if (expectedError._isRegularExpression) {
        JSValue *testResult = [expectedError invokeMethod:@"test" withArguments:@[ exceptionMessageValue ]];
        assertEquals(context, testResult.toBool, debugString(expectedError), debugString(exceptionMessageValue), combineMessages(message, @"Function threw an exception that didn't match the regular expression"), outExceptionString);
        return;
    }

    assertEquals(context, [expectedError isEqualWithTypeCoercionToObject:exceptionMessageValue], debugString(expectedError), debugString(exceptionMessageValue), combineMessages(message, @"Function threw an exception that didn't equal"), outExceptionString);
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
    fail(context, combineMessages(message, adoptNS([[NSString alloc] initWithFormat:@"Function threw an exception: %@", debugString(exceptionMessageValue)]).get()));

    return [JSValue valueWithUndefinedInContext:function.context];
}

JSValue *WebExtensionAPITest::assertSafeResolve(JSContextRef context, JSValue *function, NSString *message)
{
    JSValue *result = assertSafe(context, function, message);
    if (!result._isThenable)
        return result;

    return assertResolves(context, result, message);
}

JSValue *WebExtensionAPITest::addTest(JSContextRef context, JSValue *testFunction)
{
    return addTest(context, testFunction, "test.addTest()"_s);
}

JSValue *WebExtensionAPITest::runTests(JSContextRef context, NSArray *testFunctions)
{
    JSValue *testResultPromises = [JSValue valueWithNewArrayInContext:toJSContext(context)];

    for (JSValue *testFunction in testFunctions)
        [testResultPromises invokeMethod:@"push" withArguments:@[ addTest(context, testFunction, "test.runTests()"_s) ]];

    return [toJSContext(context)[@"Promise"] invokeMethod:@"all" withArguments:@[ testResultPromises ]];
}

JSValue *WebExtensionAPITest::addTest(JSContextRef context, JSValue *testFunction, String callingAPIName)
{
    auto testName = testFunction[@"name"].toString;
    if (!testName.length)
        return [JSValue valueWithNewPromiseRejectedWithReason:toErrorString(callingAPIName, nullString(), "The supplied test function must be named."_s).createNSString().get() inContext:toJSContext(context)];

    RefPtr page = toWebPage(context);
    if (!page)
        return [JSValue valueWithNewPromiseRejectedWithReason:toErrorString(callingAPIName, nullString(), "Error creating a new test."_s).createNSString().get() inContext:toJSContext(context)];

    RefPtr webExtensionControllerProxy = page->webExtensionControllerProxy();
    if (!webExtensionControllerProxy)
        return [JSValue valueWithNewPromiseRejectedWithReason:toErrorString(callingAPIName, nullString(), "Error creating a new test."_s).createNSString().get() inContext:toJSContext(context)];

    __block JSValue *resolveCallback;
    __block JSValue *rejectCallback;
    JSValue *resultPromise = [JSValue valueWithNewPromiseInContext:testFunction.context fromExecutor:^(JSValue *resolve, JSValue *reject) {
        resolveCallback = resolve;
        rejectCallback = reject;
    }];

    auto location = scriptLocation(context);
    auto webExtensionControllerIdentifier = webExtensionControllerProxy->identifier();

    m_testQueue.append({
        testName,
        location,
        webExtensionControllerIdentifier,
        testFunction,
        resolveCallback,
        rejectCallback
    });

    WebProcess::singleton().send(Messages::WebExtensionController::TestAdded(testName, location.first, location.second), webExtensionControllerIdentifier);

    if (!m_runningTest) {
        m_runningTest = true;

        WorkQueue::mainSingleton().dispatch([this, protectedThis = Ref { *this }] {
            startNextTest();
        });
    }

    return resultPromise;
}

void WebExtensionAPITest::startNextTest()
{
    auto test = m_testQueue.takeFirst();

    WebProcess::singleton().send(Messages::WebExtensionController::TestStarted(test.testName, test.location.first, test.location.second), test.webExtensionControllerIdentifier);

    JSValue *result = [test.testFunction callWithArguments:nil];

    auto testComplete = [this, protectedThis = Ref { *this }, test](JSValue *result, JSValue *error) {
        if (error || m_hitAssertion) {
            NSString *errorMessage;
            if (error) {
                JSValue *errorMessageValue = error.isObject && [error hasProperty:@"message"] ? error[@"message"] : error;
                errorMessage = debugString(errorMessageValue);
            } else if (!m_assertionMessage.isNull())
                errorMessage = m_assertionMessage.createNSString().get();

            errorMessage = errorMessage ? combineMessages(@"Promise rejected with an error: ", errorMessage) : @"Promise rejected without an error";

            WebProcess::singleton().send(Messages::WebExtensionController::TestFinished(test.testName, false, errorMessage, test.location.first, test.location.second), test.webExtensionControllerIdentifier);

            [test.rejectCallback callWithArguments:nil];
        } else {
            WebProcess::singleton().send(Messages::WebExtensionController::TestFinished(test.testName, true, @"Promise resolved without an error.", test.location.first, test.location.second), test.webExtensionControllerIdentifier);
            [test.resolveCallback callWithArguments:@[ result ]];
        }

        m_hitAssertion = false;

        // Clear the exception since it was caught.
        test.testFunction.get().context.exception = nil;

        if (!m_testQueue.isEmpty())
            startNextTest();
        else
            m_runningTest = false;
    };

    if (result._isThenable) {
        [result _awaitThenableResolutionWithCompletionHandler:^(JSValue *result, JSValue *error) {
            testComplete(result, error);
        }];
    } else
        testComplete(result, test.testFunction.get().context.exception);
}

void WebExtensionAPITest::recordAssertionIfNeeded(bool result, const String& message, std::pair<String, unsigned> location, NSString **outExceptionString)

{
    if (!m_runningTest || (m_runningTest && result))
        return;

    m_hitAssertion = true;
    m_assertionMessage = message;

    if (!outExceptionString)
        return;

    auto *locationString = [NSString stringWithFormat:@"%@:%u", location.first.createNSString().get(), location.second];
    *outExceptionString = message.isNull()
        ? [NSString stringWithFormat:@"Assertion Failed: %@", locationString]
        : [NSString stringWithFormat:@"Assertion Failed: %@. %@", message.createNSString().get(), locationString];
}

void WebExtensionContextProxy::dispatchTestMessageEvent(const String& message, const String& argumentJSON, WebExtensionContentWorldType contentWorldType)
{
    id argument = parseJSON(argumentJSON.createNSString().get(), JSONOptions::FragmentsAllowed);

    RetainPtr nsMessage = message.createNSString();
    if (contentWorldType == WebExtensionContentWorldType::WebPage) {
        enumerateFramesAndWebPageNamespaceObjects([&](auto&, auto& namespaceObject) {
            namespaceObject.test().onMessage().invokeListenersWithArgument(nsMessage.get(), argument);
        });

        return;
    }

    enumerateFramesAndNamespaceObjects([&](auto&, auto& namespaceObject) {
        namespaceObject.test().onMessage().invokeListenersWithArgument(nsMessage.get(), argument);
    }, toDOMWrapperWorld(contentWorldType));
}

void WebExtensionContextProxy::dispatchTestStartedEvent(const String& argumentJSON, WebExtensionContentWorldType contentWorldType)
{
    id argument = parseJSON(argumentJSON.createNSString().get(), JSONOptions::FragmentsAllowed);

    if (contentWorldType == WebExtensionContentWorldType::WebPage) {
        enumerateFramesAndWebPageNamespaceObjects([&](auto&, auto& namespaceObject) {
            namespaceObject.test().onTestStarted().invokeListenersWithArgument(argument);
        });

        return;
    }

    enumerateFramesAndNamespaceObjects([&](auto&, auto& namespaceObject) {
        namespaceObject.test().onTestStarted().invokeListenersWithArgument(argument);
    }, toDOMWrapperWorld(contentWorldType));
}

void WebExtensionContextProxy::dispatchTestFinishedEvent(const String& argumentJSON, WebExtensionContentWorldType contentWorldType)
{
    id argument = parseJSON(argumentJSON.createNSString().get(), JSONOptions::FragmentsAllowed);

    if (contentWorldType == WebExtensionContentWorldType::WebPage) {
        enumerateFramesAndWebPageNamespaceObjects([&](auto&, auto& namespaceObject) {
            namespaceObject.test().onTestFinished().invokeListenersWithArgument(argument);
        });

        return;
    }

    enumerateFramesAndNamespaceObjects([&](auto&, auto& namespaceObject) {
        namespaceObject.test().onTestFinished().invokeListenersWithArgument(argument);
    }, toDOMWrapperWorld(contentWorldType));
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
