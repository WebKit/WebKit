/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import "config.h"
#import <WebKit/WKFoundation.h>

#if WK_HAVE_C_SPI

#import "PlatformUtilities.h"
#import "PlatformWebView.h"
#import "Test.h"
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static bool receivedAlert;
static RetainPtr<WKStringRef> lastAlert;

static void runJavaScriptAlert(WKPageRef page, WKStringRef alertText, WKFrameRef frame, WKSecurityOriginRef, const void* clientInfo)
{
    lastAlert = alertText;
    receivedAlert = true;
}

static void decidePolicyForNavigationAction(WKPageRef, WKNavigationActionRef, WKFramePolicyListenerRef listener, WKTypeRef, const void*)
{
    WKFramePolicyListenerUse(listener);
}

static void decidePolicyForNavigationResponse(WKPageRef, WKNavigationResponseRef, WKFramePolicyListenerRef listener, WKTypeRef, const void*)
{
    WKFramePolicyListenerUse(listener);
}

TEST(WebKit, NavigationClientDefaultCrypto)
{
    auto context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    // The navigationClient quite explicitly does *not* have a copyWebCryptoMasterKey callback,
    // which allows this test to make sure that WebKit2 instead creates a default crypto master key.
    WKPageNavigationClientV0 navigationClient;
    memset(&navigationClient, 0, sizeof(navigationClient));
    navigationClient.base.version = 0;
    navigationClient.decidePolicyForNavigationAction = decidePolicyForNavigationAction;
    navigationClient.decidePolicyForNavigationResponse = decidePolicyForNavigationResponse;

    WKPageSetPageNavigationClient(webView.page(), &navigationClient.base);

    WKPageUIClientV5 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));
    uiClient.base.version = 5;
    uiClient.runJavaScriptAlert = runJavaScriptAlert;

    WKPageSetPageUIClient(webView.page(), &uiClient.base);

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("navigation-client-default-crypto", "html"));
    WKPageLoadURL(webView.page(), url.get());

    Util::run(&receivedAlert);
    EXPECT_TRUE(WKStringIsEqualToUTF8CString(lastAlert.get(), "successfully stored key"));

    receivedAlert = false;
    Util::run(&receivedAlert);
    EXPECT_TRUE(WKStringIsEqualToUTF8CString(lastAlert.get(), "retrieved valid key"));
}

} // namespace TestWebKitAPI

#endif // WK_HAVE_C_SPI
