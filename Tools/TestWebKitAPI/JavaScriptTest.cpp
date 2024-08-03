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

#include "config.h"

#if WK_HAVE_C_SPI

#include "JavaScriptTest.h"

#include "PlatformUtilities.h"
#include "Test.h"
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKSerializedScriptValue.h>
#include <WebKit/WKStringPrivate.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UniqueArray.h>
#include <wtf/text/MakeString.h>

namespace TestWebKitAPI {

struct JavaScriptCallbackContext {
    JavaScriptCallbackContext() : didFinish(false) { }

    bool didFinish;
    JSRetainPtr<JSStringRef> actualString;
};

static void javaScriptCallback(WKTypeRef result, WKErrorRef error, void* ctx)
{
    EXPECT_NULL(error);
    JavaScriptCallbackContext* context = static_cast<JavaScriptCallbackContext*>(ctx);

    if (!result)
        context->actualString = adopt(JSStringCreateWithUTF8CString("undefined"));
    else if (WKBooleanGetTypeID() == WKGetTypeID(result))
        context->actualString = adopt(JSStringCreateWithUTF8CString(WKBooleanGetValue((WKBooleanRef)result) ? "true" : "false"));
    else if (WKStringGetTypeID() == WKGetTypeID(result))
        context->actualString = adopt(WKStringCopyJSString((WKStringRef)result));
    else if (WKDoubleGetTypeID() == WKGetTypeID(result)) {
        double value = WKDoubleGetValue((WKDoubleRef)result);
        String s = makeString(value);
        context->actualString = adopt(JSStringCreateWithUTF8CString(s.utf8().data()));
    } else
        WTFLogAlways("Unexpected type %d", WKGetTypeID(result));

    context->didFinish = true;
}

::testing::AssertionResult runJSTest(const char*, const char*, const char*, WKPageRef page, const char* script, const char* expectedResult)
{
    JavaScriptCallbackContext context;
    WKPageEvaluateJavaScriptInMainFrame(page, Util::toWK(script).get(), &context, javaScriptCallback);
    Util::run(&context.didFinish);

    size_t bufferSize = JSStringGetMaximumUTF8CStringSize(context.actualString.get());
    auto buffer = makeUniqueArray<char>(bufferSize);
    JSStringGetUTF8CString(context.actualString.get(), buffer.get(), bufferSize);

    return compareJSResult(script, buffer.get(), expectedResult);
}
    
::testing::AssertionResult compareJSResult(const char* script, const char* actualResult, const char* expectedResult)
{
    if (!strcmp(actualResult, expectedResult))
        return ::testing::AssertionSuccess();

    return ::testing::AssertionFailure()
        << "JS expression: " << script << "\n"
        << "       Actual: " << actualResult << "\n"
        << "     Expected: " << expectedResult;
}

} // namespace TestWebKitAPI

#endif
