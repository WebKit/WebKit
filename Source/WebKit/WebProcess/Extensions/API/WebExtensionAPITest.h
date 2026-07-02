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

OBJC_CLASS NSString;

namespace WebKit {

class WebExtensionAPITest : public WebExtensionAPIObject, public JSWebExtensionWrappable {
    WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(WebExtensionAPITest, test, test);

public:
#if PLATFORM(COCOA)
    void notifyFail(JSContextRef, NSString *message);
    void notifyPass(JSContextRef, NSString *message);

    void sendMessage(JSContextRef, NSString *message, JSValue *argument);
    WebExtensionAPIEvent& onMessage();
    WebExtensionAPIEvent& onTestStarted();
    WebExtensionAPIEvent& onTestFinished();

    JSValue *runWithUserGesture(WebFrame&, JSValue *function);
    bool isProcessingUserGesture();

    void log(JSContextRef, JSValue *);

    void fail(JSContextRef, NSString *message);
    void succeed(JSContextRef, NSString *message);

    void assertTrue(JSContextRef, bool testValue, NSString *message, NSString **outExceptionString);
    void assertFalse(JSContextRef, bool testValue, NSString *message, NSString **outExceptionString);

    void assertDeepEq(JSContextRef, JSValue *actualValue, JSValue *expectedValue, NSString *message, NSString **outExceptionString);
    void assertEq(JSContextRef, JSValue *actualValue, JSValue *expectedValue, NSString *message, NSString **outExceptionString);

    JSValue *assertRejects(JSContextRef, JSValue *promise, JSValue *expectedError, NSString *message);
    JSValue *assertResolves(JSContextRef, JSValue *promise, NSString *message);

    void assertThrows(JSContextRef, JSValue *function, JSValue *expectedError, NSString *message, NSString **outExceptionString);
    JSValue *assertSafe(JSContextRef, JSValue *function, NSString *message);

    JSValue *assertSafeResolve(JSContextRef, JSValue *function, NSString *message);

    JSValue *addTest(JSContextRef, JSValue *testFunction);
    JSValue *runTests(JSContextRef, NSArray *testFunctions);

private:
    RefPtr<WebExtensionAPIEvent> m_onMessage;
    RefPtr<WebExtensionAPIEvent> m_onTestStarted;
    RefPtr<WebExtensionAPIEvent> m_onTestFinished;

    struct Test {
        String testName;
        std::pair<String, unsigned> location;
        WebExtensionControllerIdentifier webExtensionControllerIdentifier;
        RetainPtr<JSValue> testFunction;
        RetainPtr<JSValue> resolveCallback;
        RetainPtr<JSValue> rejectCallback;
    };

    Deque<Test> m_testQueue;
    bool m_runningTest { false };
    bool m_hitAssertion { false };
    String m_assertionMessage;

    JSValue *addTest(JSContextRef, JSValue *testFunction, String callingAPIName);
    void assertEquals(JSContextRef, bool result, NSString *expectedString, NSString *actualString, NSString *message, NSString **outExceptionString);
    void startNextTest();
    void recordAssertionIfNeeded(bool result, const String& message, std::pair<String, unsigned> location, NSString **outExceptionString);
#endif
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_WEB_EXTENSION(WebExtensionAPITest, test);

#endif // ENABLE(WK_WEB_EXTENSIONS)
