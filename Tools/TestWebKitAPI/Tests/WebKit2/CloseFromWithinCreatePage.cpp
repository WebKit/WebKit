/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "PlatformUtilities.h"
#include "PlatformWebView.h"

namespace TestWebKitAPI {

static bool testDone;
static std::unique_ptr<PlatformWebView> openedWebView;

static void runJavaScriptAlert(WKPageRef page, WKStringRef alertText, WKFrameRef frame, const void* clientInfo)
{
    // FIXME: Check that the alert text matches the storage.
    testDone = true;
}

static WKPageRef createNewPage(WKPageRef page, WKURLRequestRef urlRequest, WKDictionaryRef features, WKEventModifiers modifiers, WKEventMouseButton mouseButton, const void *clientInfo)
{
    EXPECT_TRUE(openedWebView == nullptr);

    openedWebView = std::make_unique<PlatformWebView>(page);

    WKPageUIClientV5 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));

    uiClient.base.version = 5;
    uiClient.runJavaScriptAlert = runJavaScriptAlert;
    WKPageSetPageUIClient(openedWebView->page(), &uiClient.base);

    WKPageClose(page);

    WKRetain(openedWebView->page());
    return openedWebView->page();
}

TEST(WebKit2, CloseFromWithinCreatePage)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreate());

    PlatformWebView webView(context.get());

    WKPageUIClientV5 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));

    uiClient.base.version = 5;
    uiClient.createNewPage = createNewPage;
    uiClient.runJavaScriptAlert = runJavaScriptAlert;
    WKPageSetPageUIClient(webView.page(), &uiClient.base);

    WKRetainPtr<WKURLRef> url(AdoptWK, Util::createURLForResource("close-from-within-create-page", "html"));
    WKPageLoadURL(webView.page(), url.get());

    Util::run(&testDone);

    openedWebView = nullptr;
}

}
