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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionAPITest.h"
#include "WebExtensionAPIEvent.h"
#include "WebExtensionAPIObject.h"
#include "WebExtensionAPIWebNavigationEvent.h"
#include <wtf/Deque.h>

namespace WebKit {

class WebExtensionAPITest : public WebExtensionAPIObject, public JSWebExtensionWrappable {
    WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(WebExtensionAPITest, test, test);

public:
    void notifyFail(JSContextRef, const String& message);
    void notifyPass(JSContextRef, const String& message);

    void sendMessage(JSContextRef, const String& message, JSValueRef argument);
    WebExtensionAPIEvent& onMessage();
    WebExtensionAPIEvent& onTestStarted();
    WebExtensionAPIEvent& onTestFinished();

    JSValueRef runWithUserGesture(WebFrame&, JSContextRef, JSValueRef function);
    bool isProcessingUserGesture();

    void log(JSContextRef, JSValueRef);

    void fail(JSContextRef, const String& message);
    void succeed(JSContextRef, const String& message);

    void assertTrue(JSContextRef, bool testValue, const String& message, String& outExceptionString);
    void assertFalse(JSContextRef, bool testValue, const String& message, String& outExceptionString);

    void assertDeepEq(JSContextRef, JSValueRef actualValue, JSValueRef expectedValue, const String& message, String& outExceptionString);
    void assertEq(JSContextRef, JSValueRef actualValue, JSValueRef expectedValue, const String& message, String& outExceptionString);

    JSValueRef assertRejects(JSContextRef, JSValueRef promise, JSValueRef expectedError, const String& message);
    JSValueRef assertResolves(JSContextRef, JSValueRef promise, const String& message);

    void assertThrows(JSContextRef, JSValueRef function, JSValueRef expectedError, const String& message, String& outExceptionString);
    JSValueRef assertSafe(JSContextRef, JSValueRef function, const String& message);

    JSValueRef assertSafeResolve(JSContextRef, JSValueRef function, const String& message);

    JSValueRef addTest(JSContextRef, JSValueRef testFunction);
    JSValueRef runTests(JSContextRef, Vector<Protected<JSValueRef>> testFunctions);

private:
    RefPtr<WebExtensionAPIEvent> m_onMessage;
    RefPtr<WebExtensionAPIEvent> m_onTestStarted;
    RefPtr<WebExtensionAPIEvent> m_onTestFinished;

    struct Test {
        String testName;
        std::pair<String, unsigned> location;
        WebExtensionControllerIdentifier webExtensionControllerIdentifier;
        Protected<JSValueRef> testFunction;
        Protected<JSValueRef> resolveCallback;
        Protected<JSValueRef> rejectCallback;
    };

    Deque<Test> m_testQueue;
    bool m_runningTest { false };
    bool m_hitAssertion { false };
    String m_assertionMessage;

    template<size_t ArgumentCount>
    JSValueRef invokeMethod(JSContextRef, JSValueRef, const String& method, std::array<JSValueRef, ArgumentCount>&& arguments, JSValueRef* exception = nullptr);
    std::pair<String, unsigned> scriptLocation(JSContextRef);
    String debugString(JSContextRef, JSValueRef);
    String combineMessages(const String& messageOne, const String& messageTwo);

    JSValueRef addTest(JSContextRef, JSValueRef testFunction, String callingAPIName);
    void assertEquals(JSContextRef, bool result, const String& expectedString, const String& actualString, const String& message, String& outExceptionString);
    void startNextTest();
    void recordAssertionIfNeeded(bool result, const String& message, std::pair<String, unsigned> location, String& outExceptionString);
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_WEB_EXTENSION(WebExtensionAPITest, test);

#endif // ENABLE(WK_WEB_EXTENSIONS)
