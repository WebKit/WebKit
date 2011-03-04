/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "JavaScriptTest.h"

#include "PlatformUtilities.h"
#include "Test.h"
#include <JavaScriptCore/JavaScriptCore.h>
#include <WebKit2/WKRetainPtr.h>
#include <WebKit2/WKSerializedScriptValue.h>

namespace TestWebKitAPI {

struct JavaScriptCallbackContext {
    JavaScriptCallbackContext(const char* expectedString) : didFinish(false), expectedString(expectedString), didMatchExpectedString(false) { }

    bool didFinish;
    const char* expectedString;
    bool didMatchExpectedString;
};

static void javaScriptCallback(WKSerializedScriptValueRef resultSerializedScriptValue, WKErrorRef error, void* ctx)
{
    TEST_ASSERT(resultSerializedScriptValue);

    JavaScriptCallbackContext* context = static_cast<JavaScriptCallbackContext*>(ctx);

    JSGlobalContextRef scriptContext = JSGlobalContextCreate(0);
    TEST_ASSERT(scriptContext);

    JSValueRef scriptValue = WKSerializedScriptValueDeserialize(resultSerializedScriptValue, scriptContext, 0);
    TEST_ASSERT(scriptValue);

    JSStringRef scriptString = JSValueToStringCopy(scriptContext, scriptValue, 0);
    TEST_ASSERT(scriptString);

    context->didFinish = true;
    context->didMatchExpectedString = JSStringIsEqualToUTF8CString(scriptString, context->expectedString);

    JSStringRelease(scriptString);
    JSGlobalContextRelease(scriptContext);

    TEST_ASSERT(!error);
}

static WKRetainPtr<WKStringRef> wk(const char* utf8String)
{
    return WKRetainPtr<WKStringRef>(AdoptWK, WKStringCreateWithUTF8CString(utf8String));
}

bool runJSTest(WKPageRef page, const char* script, const char* expectedResult)
{
    JavaScriptCallbackContext context(expectedResult);
    WKPageRunJavaScriptInMainFrame(page, wk(script).get(), &context, javaScriptCallback);
    Util::run(&context.didFinish);
    return context.didMatchExpectedString;
}

} // namespace TestWebKitAPI
