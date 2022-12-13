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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionAPITest.h"
#include "WebExtensionAPIEvent.h"
#include "WebExtensionAPIObject.h"
#include "WebExtensionAPIWebNavigationEvent.h"

OBJC_CLASS NSString;

namespace WebKit {

class WebPage;

class WebExtensionAPITest : public WebExtensionAPIObject, public JSWebExtensionWrappable {
    WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(WebExtensionAPITest, test);

public:
#if PLATFORM(COCOA)
    void notifyFail(JSContextRef, NSString *message);
    void notifyPass(JSContextRef, NSString *message);

    void yield(JSContextRef, NSString *message);

    void log(JSContextRef, NSString *message);

    void fail(JSContextRef, NSString *message);
    void succeed(JSContextRef, NSString *message);

    void assertTrue(JSContextRef, bool testValue, NSString *message);
    void assertFalse(JSContextRef, bool testValue, NSString *message);

    void assertDeepEq(JSContextRef, JSValue *expectedValue, JSValue *actualValue, NSString *message);
    void assertEq(JSContextRef, JSValue *expectedValue, JSValue *actualValue, NSString *message);

    JSValue *assertRejects(JSContextRef, JSValue *promise, JSValue *expectedError, NSString *message);
    void assertThrows(JSContextRef, JSValue *function, JSValue *expectedError, NSString *message);

    WebExtensionAPIWebNavigationEvent& testWebNavigationEvent();
    void fireTestWebNavigationEvent(NSString *urlString);

    WebExtensionAPIEvent& testEvent();
    void fireTestEvent();

private:
    RefPtr<WebExtensionAPIWebNavigationEvent> m_webNavigationEvent;
    RefPtr<WebExtensionAPIEvent> m_event;
#endif
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
