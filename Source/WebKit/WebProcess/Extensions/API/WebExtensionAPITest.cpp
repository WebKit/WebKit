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

#include "config.h"
#include "WebExtensionAPITest.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "MessageSenderInlines.h"
#include "WebExtensionAPINamespace.h"
#include "WebExtensionAPIWebPageNamespace.h"
#include "WebExtensionControllerMessages.h"
#include "WebExtensionControllerProxy.h"
#include "WebExtensionEventListenerType.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <WebCore/LocalFrameInlines.h>

namespace WebKit {

std::pair<String, unsigned> WebExtensionAPITest::scriptLocation(JSContextRef context)
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

template<size_t ArgumentCount>
JSValueRef WebExtensionAPITest::invokeMethod(JSContextRef context, JSValueRef value, const String& method, std::array<JSValueRef, ArgumentCount>&& arguments, JSValueRef* exception)
{
    if (!context || !value)
        return JSValueMakeUndefined(context);

    if (!JSValueIsObject(context, value))
        return JSValueMakeUndefined(context);

    JSObjectRef thisObject = JSValueToObject(context, value, exception);
    if (!thisObject || (exception && *exception))
        return JSValueMakeUndefined(context);

    // This is a safer cpp false positive (rdar://163760990).
    SUPPRESS_UNCOUNTED_ARG JSValueRef function = JSObjectGetProperty(context, thisObject, toJSString(method).get(), exception);
    if (!function || (exception && *exception))
        return JSValueMakeUndefined(context);

    JSObjectRef funcObject = JSValueToObject(context, function, exception);
    if (!funcObject || (exception && *exception))
        return JSValueMakeUndefined(context);

    return JSObjectCallAsFunction(context, funcObject, thisObject, ArgumentCount, arguments.data(), exception);
}

void WebExtensionAPITest::notifyFail(JSContextRef context, const String& message)
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

void WebExtensionAPITest::notifyPass(JSContextRef context, const String& message)
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

void WebExtensionAPITest::sendMessage(JSContextRef context, const String& message, JSValueRef argument)
{
    auto location = scriptLocation(context);

    RefPtr page = toWebPage(context);
    if (!page)
        return;

    RefPtr webExtensionControllerProxy = page->webExtensionControllerProxy();
    if (!webExtensionControllerProxy)
        return;

    WebProcess::singleton().send(Messages::WebExtensionController::TestSentMessage(message, toSortedJSONString(context, argument), location.first, location.second), webExtensionControllerProxy->identifier());
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

JSValueRef WebExtensionAPITest::runWithUserGesture(WebFrame& frame, JSContextRef context, JSValueRef function)
{
    RefPtr coreFrame = frame.coreLocalFrame();
    WebCore::UserGestureIndicator gestureIndicator(WebCore::IsProcessingUserGesture::Yes, coreFrame ? coreFrame->document() : nullptr);

    return callObjectWithArguments<0>(function, context, { });
}

bool WebExtensionAPITest::isProcessingUserGesture()
{
    return WebCore::UserGestureIndicator::processingUserGesture();
}

String WebExtensionAPITest::debugString(JSContextRef contextRef, JSValueRef valueRef)
{
    if (isRegularExpression(contextRef, valueRef) || isFunction(contextRef, valueRef))
        return toString(contextRef, valueRef);
    auto sortedJSON = toSortedJSONString(contextRef, valueRef);
    if (!sortedJSON.isEmpty())
        return sortedJSON;
    return "undefined"_s;
}

void WebExtensionAPITest::log(JSContextRef context, JSValueRef value)
{
    auto location = scriptLocation(context);

    RefPtr page = toWebPage(context);
    if (!page)
        return;

    RefPtr webExtensionControllerProxy = page->webExtensionControllerProxy();
    if (!webExtensionControllerProxy)
        return;

    WebProcess::singleton().send(Messages::WebExtensionController::TestLogMessage(debugString(context, value), location.first, location.second), webExtensionControllerProxy->identifier());
}

void WebExtensionAPITest::fail(JSContextRef context, const String& message)
{
    String empty = nullString();
    assertTrue(context, false, message, empty);
}

void WebExtensionAPITest::succeed(JSContextRef context, const String& message)
{
    String empty = nullString();
    assertTrue(context, true, message, empty);
}

void WebExtensionAPITest::assertTrue(JSContextRef context, bool actualValue, const String& message, String& outExceptionString)
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

void WebExtensionAPITest::assertFalse(JSContextRef context, bool actualValue, const String& message, String& outExceptionString)
{
    assertTrue(context, !actualValue, message, outExceptionString);
}

void WebExtensionAPITest::assertDeepEq(JSContextRef context, JSValueRef actualValue, JSValueRef expectedValue, const String& message, String& outExceptionString)
{
    String expectedJSONValue = debugString(context, expectedValue);
    String actualJSONValue = debugString(context, actualValue);

    // FIXME: Comparing JSON is a quick attempt that works, but can still fail due to any non-JSON values.
    // See Firefox's implementation: https://searchfox.org/mozilla-central/source/toolkit/components/extensions/child/ext-test.js#78
    bool strictEqual = JSValueIsStrictEqual(context, expectedValue, actualValue);
    bool deepEqual = strictEqual || expectedJSONValue == actualJSONValue;

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

String WebExtensionAPITest::combineMessages(const String& messageOne, const String& messageTwo)
{
    if (!messageOne.isEmpty() && !messageTwo.isEmpty()) {
        Vector<String> stringList = { messageOne, messageTwo };
        return makeStringByJoining(stringList, "\n"_s);
    }

    if (!messageOne.isEmpty() && messageTwo.isEmpty())
        return messageOne;
    return messageTwo;
}

void WebExtensionAPITest::assertEquals(JSContextRef context, bool result, const String& expectedString, const String& actualString, const String& message, String& outExceptionString)
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

void WebExtensionAPITest::assertEq(JSContextRef context, JSValueRef actualValue, JSValueRef expectedValue, const String& message, String& outExceptionString)
{
    String expectedJSONValue = debugString(context, expectedValue);
    String actualJSONValue = debugString(context, actualValue);

    bool strictEqual = JSValueIsStrictEqual(context, expectedValue, actualValue);
    if (!strictEqual && expectedJSONValue == actualJSONValue)
        actualJSONValue = makeString(actualJSONValue, " (different)"_s);

    assertEquals(context, strictEqual, expectedJSONValue, actualJSONValue, message, outExceptionString);
}

void WebExtensionAPITest::assertThrows(JSContextRef context, JSValueRef function, JSValueRef expectedError, const String& message, String& outExceptionString)
{
    JSValueRef exceptionValue = nullptr;
    callObjectWithArguments<0>(function, context, { }, &exceptionValue, false);

    if (!exceptionValue) {
        assertEquals(context, false, expectedError ? debugString(context, expectedError) : "(any exception)"_s, "(no exception)"_s, combineMessages(message, "Function did not throw an exception"_s), outExceptionString);
        return;
    }

    JSValueRef exceptionMessageValue = exceptionValue;
    if (JSValueIsObject(context, exceptionValue)) {
        JSObjectRef object = JSValueToObject(context, exceptionValue, nullptr);

        // This is a safer cpp false positive (rdar://163760990).
        SUPPRESS_UNCOUNTED_ARG if (JSObjectHasProperty(context, object, toJSString("message"_s).get()))
            SUPPRESS_UNCOUNTED_ARG exceptionMessageValue = JSObjectGetProperty(context, object, toJSString("message"_s).get(), 0);
    }

    // Clear the exception since it was caught.
    exceptionValue = nullptr;

    if (JSValueIsUndefined(context, expectedError) || !expectedError) {
        assertEquals(context, true, "(any exception)"_s, debugString(context, exceptionMessageValue), combineMessages(message, "Function threw an exception"_s), outExceptionString);
        return;
    }

    if (isRegularExpression(context, expectedError)) {
        JSValueRef testResult = invokeMethod<1>(context, expectedError, "test"_s, { exceptionMessageValue });
        assertEquals(context, JSValueToBoolean(context, testResult), debugString(context, expectedError), debugString(context, exceptionMessageValue), combineMessages(message, "Function threw an exception that didn't match the regular expression"_s), outExceptionString);
        return;
    }

    assertEquals(context, JSValueIsEqual(context, expectedError, exceptionMessageValue, nullptr), debugString(context, expectedError), debugString(context, exceptionMessageValue), combineMessages(message, "Function threw an exception that didn't equal"_s), outExceptionString);
}

JSValueRef WebExtensionAPITest::assertSafe(JSContextRef context, JSValueRef function, const String& message)
{
    JSValueRef exceptionValue = nullptr;
    JSValueRef result = callObjectWithArguments<0>(function, context, { }, &exceptionValue, false);

    if (!exceptionValue) {
        succeed(context, "Function did not throw an exception"_s);
        return result;
    }

    JSValueRef exceptionMessageValue = exceptionValue;
    if (JSValueIsObject(context, exceptionValue)) {
        JSObjectRef object = JSValueToObject(context, exceptionValue, nullptr);

        // This is a safer cpp false positive (rdar://163760990).
        SUPPRESS_UNCOUNTED_ARG if (JSObjectHasProperty(context, object, toJSString("message"_s).get()))
            SUPPRESS_UNCOUNTED_ARG exceptionMessageValue = JSObjectGetProperty(context, object, toJSString("message"_s).get(), 0);
    }

    // Clear the exception since it was caught.
    exceptionValue = nullptr;

    fail(context, combineMessages(message, makeString("Function threw an exception: "_s, debugString(context, exceptionMessageValue))));

    return JSValueMakeUndefined(context);
}

JSValueRef WebExtensionAPITest::assertSafeResolve(JSContextRef context, JSValueRef function, const String& message)
{
    JSValueRef result = assertSafe(context, function, message);
    if (!isThenable(context, function))
        return result;

    return assertResolves(context, result, message);
}

JSValueRef WebExtensionAPITest::addTest(JSContextRef context, JSValueRef testFunction)
{
    return addTest(context, testFunction, "test.addTest()"_s);
}

JSValueRef WebExtensionAPITest::runTests(JSContextRef context, Vector<Protected<JSValueRef>> testFunctions)
{
    JSObjectRef testResultPromises = JSObjectMakeArray(context, 0, nullptr, nullptr);

    for (Protected<JSValueRef> testFunction : testFunctions)
        invokeMethod<1>(context, testResultPromises, "push"_s, { addTest(context, testFunction.get(), "test.runTests()"_s) });

    // This is a safer cpp false positive (rdar://163760990).
    SUPPRESS_UNCOUNTED_ARG return invokeMethod<1>(context, JSObjectGetProperty(context, JSContextGetGlobalObject(context), toJSString("Promise"_s).get(), nullptr), "all"_s, { testResultPromises });
}

void WebExtensionAPITest::recordAssertionIfNeeded(bool result, const String& message, std::pair<String, unsigned> location, String& outExceptionString)
{
    if (!m_runningTest || (m_runningTest && result))
        return;

    m_hitAssertion = true;
    m_assertionMessage = message;

    Vector<String> locationVector = { location.first, String::number(location.second) };
    auto locationString = makeStringByJoining(locationVector, ":"_s);
    outExceptionString = message.isNull()
        ? makeString("Assertion Failed: "_s, locationString)
        : makeString("Assertion Failed: "_s, message, ". "_s, locationString);
}

void WebExtensionContextProxy::dispatchTestMessageEvent(const String& message, const String& argumentJSON, WebExtensionContentWorldType contentWorldType)
{
    if (contentWorldType == WebExtensionContentWorldType::WebPage) {
        enumerateFramesAndWebPageNamespaceObjects([&](auto&, auto& namespaceObject) {
            namespaceObject.test().onMessage().invokeListenersWithJSONArgument(message, argumentJSON);
        });

        return;
    }

    enumerateFramesAndNamespaceObjects([&](auto&, auto& namespaceObject) {
        namespaceObject.test().onMessage().invokeListenersWithJSONArgument(message, argumentJSON);
    }, toDOMWrapperWorld(contentWorldType));
}

void WebExtensionContextProxy::dispatchTestStartedEvent(const String& argumentJSON, WebExtensionContentWorldType contentWorldType)
{
    if (contentWorldType == WebExtensionContentWorldType::WebPage) {
        enumerateFramesAndWebPageNamespaceObjects([&](auto&, auto& namespaceObject) {
            namespaceObject.test().onTestStarted().invokeListenersWithJSONArgument(argumentJSON);
        });

        return;
    }

    enumerateFramesAndNamespaceObjects([&](auto&, auto& namespaceObject) {
        namespaceObject.test().onTestStarted().invokeListenersWithJSONArgument(argumentJSON);
    }, toDOMWrapperWorld(contentWorldType));
}

void WebExtensionContextProxy::dispatchTestFinishedEvent(const String& argumentJSON, WebExtensionContentWorldType contentWorldType)
{
    if (contentWorldType == WebExtensionContentWorldType::WebPage) {
        enumerateFramesAndWebPageNamespaceObjects([&](auto&, auto& namespaceObject) {
            namespaceObject.test().onTestFinished().invokeListenersWithJSONArgument(argumentJSON);
        });

        return;
    }

    enumerateFramesAndNamespaceObjects([&](auto&, auto& namespaceObject) {
        namespaceObject.test().onTestFinished().invokeListenersWithJSONArgument(argumentJSON);
    }, toDOMWrapperWorld(contentWorldType));
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
