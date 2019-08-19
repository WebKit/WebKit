/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include <wtf/StdLibExtras.h>

namespace TestWebKitAPI {

struct WebKit2TextFieldBeginAndEditEditingTest : public ::testing::Test {
    std::unique_ptr<PlatformWebView> webView;

    WKRetainPtr<WKStringRef> messageName;

    bool didFinishLoad { false };
    bool didReceiveMessage { false };

    static void didReceiveMessageFromInjectedBundle(WKContextRef, WKStringRef messageName, WKTypeRef, const void* clientInfo)
    {
        WebKit2TextFieldBeginAndEditEditingTest& client = *static_cast<WebKit2TextFieldBeginAndEditEditingTest*>(const_cast<void*>(clientInfo));
        client.messageName = messageName;
        client.didReceiveMessage = true;
    }

    static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void* clientInfo)
    {
        WebKit2TextFieldBeginAndEditEditingTest& client = *static_cast<WebKit2TextFieldBeginAndEditEditingTest*>(const_cast<void*>(clientInfo));
        client.didFinishLoad = true;
    }

    static void setInjectedBundleClient(WKContextRef context, const void* clientInfo)
    {
        WKContextInjectedBundleClientV1 injectedBundleClient;
        memset(&injectedBundleClient, 0, sizeof(injectedBundleClient));

        injectedBundleClient.base.version = 1;
        injectedBundleClient.base.clientInfo = clientInfo;
        injectedBundleClient.didReceiveMessageFromInjectedBundle = didReceiveMessageFromInjectedBundle;

        WKContextSetInjectedBundleClient(context, &injectedBundleClient.base);
    }

    static void setPageLoaderClient(WKPageRef page, const void* clientInfo)
    {
        WKPageNavigationClientV0 loaderClient;
        memset(&loaderClient, 0, sizeof(loaderClient));

        loaderClient.base.version = 0;
        loaderClient.base.clientInfo = clientInfo;
        loaderClient.didFinishNavigation = didFinishNavigation;

        WKPageSetPageNavigationClient(page, &loaderClient.base);
    }

    static void nullJavaScriptCallback(WKSerializedScriptValueRef, WKErrorRef, void*)
    {
    }

    void executeJavaScriptAndCheckDidReceiveMessage(const char* javaScriptCode, const char* expectedMessageName)
    {
        didReceiveMessage = false;
        WKPageRunJavaScriptInMainFrame(webView->page(), Util::toWK(javaScriptCode).get(), 0, nullJavaScriptCallback);
        Util::run(&didReceiveMessage);
        EXPECT_WK_STREQ(expectedMessageName, messageName);
    }

    // From ::testing::Test
    void SetUp() override
    {
        WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextForInjectedBundleTest("TextFieldDidBeginAndEndEditingEventsTest"));
        setInjectedBundleClient(context.get(), this);

        webView = makeUnique<PlatformWebView>(context.get());
        setPageLoaderClient(webView->page(), this);

        didFinishLoad = false;
        didReceiveMessage = false;

        WKPageLoadURL(webView->page(), adoptWK(Util::createURLForResource("input-focus-blur", "html")).get());
        Util::run(&didFinishLoad);
    }
};

TEST_F(WebKit2TextFieldBeginAndEditEditingTest, TextFieldDidBeginAndEndEditingEvents)
{
    executeJavaScriptAndCheckDidReceiveMessage("focusTextField('input')", "DidReceiveTextFieldDidBeginEditing");
    executeJavaScriptAndCheckDidReceiveMessage("blurTextField('input')", "DidReceiveTextFieldDidEndEditing");
}

TEST_F(WebKit2TextFieldBeginAndEditEditingTest, TextFieldDidBeginAndEndEditingEventsInReadOnlyField)
{
    executeJavaScriptAndCheckDidReceiveMessage("focusTextField('readonly')", "DidReceiveTextFieldDidBeginEditing");
    executeJavaScriptAndCheckDidReceiveMessage("blurTextField('readonly')", "DidReceiveTextFieldDidEndEditing");
}

TEST_F(WebKit2TextFieldBeginAndEditEditingTest, TextFieldDidBeginShouldNotBeDispatchedForAlreadyFocusedField)
{
    executeJavaScriptAndCheckDidReceiveMessage("focusTextField('input'); focusTextField('input')", "DidReceiveTextFieldDidBeginEditing");
    executeJavaScriptAndCheckDidReceiveMessage("blurTextField('input')", "DidReceiveTextFieldDidEndEditing");
}

TEST_F(WebKit2TextFieldBeginAndEditEditingTest, TextFieldDidEndShouldBeDispatchedForRemovedFocusField)
{
    executeJavaScriptAndCheckDidReceiveMessage("focusTextField('input')", "DidReceiveTextFieldDidBeginEditing");
    executeJavaScriptAndCheckDidReceiveMessage("removeTextField('input')", "DidReceiveTextFieldDidEndEditing");
}

} // namespace TestWebKitAPI

#endif
