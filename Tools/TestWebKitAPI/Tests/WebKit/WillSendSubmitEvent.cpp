/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

namespace TestWebKitAPI {

static bool didReceiveMessage;

static void didReceiveMessageFromInjectedBundle(WKContextRef, WKStringRef messageName, WKTypeRef body, const void*)
{
    didReceiveMessage = true;

    EXPECT_WK_STREQ("DidReceiveWillSendSubmitEvent", messageName);

    EXPECT_EQ(WKDictionaryGetTypeID(), WKGetTypeID(body));
    WKDictionaryRef values = static_cast<WKDictionaryRef>(body);

    WKRetainPtr<WKStringRef> textFieldKey = adoptWK(WKStringCreateWithUTF8CString("textField"));
    WKStringRef textFieldValueWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(values, textFieldKey.get()));
    EXPECT_WK_STREQ("text field", textFieldValueWK);

    WKRetainPtr<WKStringRef> passwordFieldKey = adoptWK(WKStringCreateWithUTF8CString("passwordField"));
    WKStringRef passwordFieldValueWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(values, passwordFieldKey.get()));
    EXPECT_WK_STREQ("password field", passwordFieldValueWK);

    // <input type="hidden"> fields are not sent.
    WKRetainPtr<WKStringRef> hiddenFieldKey = adoptWK(WKStringCreateWithUTF8CString("hiddenField"));
    WKStringRef hiddenFieldValueWK = static_cast<WKStringRef>(WKDictionaryGetItemForKey(values, hiddenFieldKey.get()));
    EXPECT_NULL(hiddenFieldValueWK);

    WKRetainPtr<WKStringRef> targetFrameKey = adoptWK(WKStringCreateWithUTF8CString("targetFrameIsMainFrame"));
    WKBooleanRef targetFrameIsMainFrame = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(values, targetFrameKey.get()));
    EXPECT_FALSE(WKBooleanGetValue(targetFrameIsMainFrame));

    WKRetainPtr<WKStringRef> sourceFrameKey = adoptWK(WKStringCreateWithUTF8CString("sourceFrameIsMainFrame"));
    WKBooleanRef sourceFrameIsMainFrame = static_cast<WKBooleanRef>(WKDictionaryGetItemForKey(values, sourceFrameKey.get()));
    EXPECT_TRUE(WKBooleanGetValue(sourceFrameIsMainFrame));
}

static void setInjectedBundleClient(WKContextRef context)
{
    WKContextInjectedBundleClientV0 injectedBundleClient;
    memset(&injectedBundleClient, 0, sizeof(injectedBundleClient));

    injectedBundleClient.base.version = 0;
    injectedBundleClient.didReceiveMessageFromInjectedBundle = didReceiveMessageFromInjectedBundle;

    WKContextSetInjectedBundleClient(context, &injectedBundleClient.base);
}

TEST(WebKit, WillSendSubmitEvent)
{
    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextForInjectedBundleTest("WillSendSubmitEventTest"));
    setInjectedBundleClient(context.get());

    PlatformWebView webView(context.get());
    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("auto-submitting-form", "html")).get());
    Util::run(&didReceiveMessage);
}

} // namespace TestWebKitAPI

#endif
