/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include <WebKit/WKFrame.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKSecurityOriginRef.h>

namespace TestWebKitAPI {

static bool done;
static unsigned dialogsSeen;
static const unsigned dialogsExpected = 4;
static const unsigned dialogsBeforeUnloadConfirmPanel = dialogsExpected - 1;

static std::unique_ptr<PlatformWebView> openedWebView;

static void analyzeDialogArguments(WKPageRef page, WKFrameRef frame, WKSecurityOriginRef securityOrigin = nullptr)
{
    EXPECT_EQ(page, WKFrameGetPage(frame));

    WKRetainPtr<WKURLRef> url = adoptWK(WKFrameCopyURL(frame));
    WKRetainPtr<WKStringRef> urlString = adoptWK(WKURLCopyString(url.get()));
    EXPECT_WK_STREQ("about:blank", urlString.get());

    if (securityOrigin) {
        WKRetainPtr<WKStringRef> protocol = adoptWK(WKSecurityOriginCopyProtocol(securityOrigin));
        EXPECT_WK_STREQ("file", protocol.get());

        WKRetainPtr<WKStringRef> host = adoptWK(WKSecurityOriginCopyHost(securityOrigin));
        EXPECT_WK_STREQ("", host.get());

        EXPECT_EQ(WKSecurityOriginGetPort(securityOrigin), 0);
    }

    ++dialogsSeen;

    // Test beforeunload last as we need to navigate the window to trigger it.
    if (dialogsSeen == dialogsBeforeUnloadConfirmPanel) {
        // User interaction is required to show the beforeunload prompt.
        openedWebView->simulateButtonClick(kWKEventMouseButtonLeftButton, 5, 5, 0);
        WKRetainPtr<WKURLRef> blankDataURL = adoptWK(WKURLCreateWithUTF8CString("data:text/html"));
        WKPageLoadURL(page, blankDataURL.get());
        return;
    }

    if (dialogsSeen == dialogsExpected)
        done = true;
}

static void runJavaScriptAlert(WKPageRef page, WKStringRef, WKFrameRef frame, WKSecurityOriginRef securityOrigin, const void*)
{
    analyzeDialogArguments(page, frame, securityOrigin);
}

static bool runJavaScriptConfirm(WKPageRef page, WKStringRef, WKFrameRef frame, WKSecurityOriginRef securityOrigin, const void*)
{
    analyzeDialogArguments(page, frame, securityOrigin);
    return false;
}

static WKStringRef runJavaScriptPrompt(WKPageRef page, WKStringRef, WKStringRef, WKFrameRef frame, WKSecurityOriginRef securityOrigin, const void*)
{
    analyzeDialogArguments(page, frame, securityOrigin);
    return nullptr;
}

static bool runBeforeUnloadConfirmPanel(WKPageRef page, WKStringRef, WKFrameRef frame, const void*)
{
    analyzeDialogArguments(page, frame);
    return false;
}

static WKPageRef createNewPage(WKPageRef page, WKURLRequestRef urlRequest, WKDictionaryRef features, WKEventModifiers modifiers, WKEventMouseButton mouseButton, const void *clientInfo)
{
    EXPECT_TRUE(openedWebView == nullptr);

    openedWebView = std::make_unique<PlatformWebView>(page);

    WKPageUIClientV5 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));

    uiClient.base.version = 5;
    uiClient.runJavaScriptAlert = runJavaScriptAlert;
    uiClient.runJavaScriptConfirm = runJavaScriptConfirm;
    uiClient.runJavaScriptPrompt = runJavaScriptPrompt;
    uiClient.runBeforeUnloadConfirmPanel = runBeforeUnloadConfirmPanel;

    WKPageSetPageUIClient(openedWebView->page(), &uiClient.base);

    WKRetain(openedWebView->page());
    return openedWebView->page();
}

TEST(WebKit, ModalAlertsSPI)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKPageUIClientV5 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));

    uiClient.base.version = 5;
    uiClient.createNewPage = createNewPage;

    WKPageSetPageUIClient(webView.page(), &uiClient.base);

    WKRetainPtr<WKURLRef> url(AdoptWK, Util::createURLForResource("modal-alerts-in-new-about-blank-window", "html"));
    WKPageLoadURL(webView.page(), url.get());

    Util::run(&done);
}

} // namespace TestWebKitAPI

#endif
