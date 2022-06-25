/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include "Test.h"
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKSerializedScriptValue.h>

namespace TestWebKitAPI {

static bool testDone;

static void didRunJavaScript(WKSerializedScriptValueRef resultSerializedScriptValue, WKErrorRef error, void* context)
{
    EXPECT_EQ(reinterpret_cast<void*>(0x1234578), context);
    EXPECT_NULL(resultSerializedScriptValue);

    // FIXME: We should also check the error, but right now it's always null.
    // Assert that it's null so we can revisit when this changes.
    EXPECT_NULL(error);

    testDone = true;
}

TEST(WebKit, EvaluateJavaScriptThatThrowsAnException)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKRetainPtr<WKStringRef> javaScriptString = adoptWK(WKStringCreateWithUTF8CString("throw 'Hello'"));
    WKPageRunJavaScriptInMainFrame(webView.page(), javaScriptString.get(), reinterpret_cast<void*>(0x1234578), didRunJavaScript);

    Util::run(&testDone);
}

static void didCreateBlob(WKSerializedScriptValueRef serializedScriptValue, WKErrorRef error, void* context)
{
    EXPECT_NOT_NULL(serializedScriptValue);
    JSGlobalContextRef jsContext = JSGlobalContextCreate(0);
    EXPECT_NOT_NULL(jsContext);
    auto jsValue = WKSerializedScriptValueDeserialize(serializedScriptValue, jsContext, 0);
    EXPECT_NOT_NULL(jsValue);

    testDone = true;
}

TEST(WebKit, EvaluateJavaScriptThatCreatesBlob)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKRetainPtr<WKStringRef> javaScriptString = adoptWK(WKStringCreateWithUTF8CString("new Blob(['this is a test blob'])"));
    WKPageRunJavaScriptInMainFrame(webView.page(), javaScriptString.get(), 0, didCreateBlob);

    Util::run(&testDone);
}

} // namespace TestWebKitAPI

#endif
