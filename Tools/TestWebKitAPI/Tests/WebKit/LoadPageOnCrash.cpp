/*
 * Copyright (C) 2013 Adenilson Cavalcanti <cavalcantii@gmail.com>
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

namespace TestWebKitAPI {

static void didFinishLoad(WKPageRef, WKNavigationRef, WKTypeRef, const void*);

class WebKit2CrashLoader {
public:
    WebKit2CrashLoader()
        : context(adoptWK(WKContextCreateWithConfiguration(nullptr)))
        , webView(context.get())
        , url(adoptWK(WKURLCreateWithUTF8CString("about:blank")))
        , firstSuccessfulLoad(false)
        , secondSuccessfulLoad(false)
    {
        memset(&loaderClient, 0, sizeof(loaderClient));

        loaderClient.base.version = 0;
        loaderClient.base.clientInfo = this;
        loaderClient.didFinishNavigation = didFinishLoad;

        WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);
    }

    void loadUrl()
    {
        WKPageLoadURL(webView.page(), url.get());
    }

    void terminateWebProcess()
    {
        WKPageTerminate(webView.page());
    }

    WKRetainPtr<WKContextRef> context;
    WKPageNavigationClientV0 loaderClient;
    PlatformWebView webView;
    WKRetainPtr<WKURLRef> url;

    bool firstSuccessfulLoad;
    bool secondSuccessfulLoad;
};

// We are going to have 2 load events intertwined by a simulated crash
// (i.e. Load -> Crash -> Load).
void didFinishLoad(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    WebKit2CrashLoader* testHelper = const_cast<WebKit2CrashLoader*>(static_cast<const WebKit2CrashLoader*>(clientInfo));

    // First load worked, let's crash WebProcess.
    if (!testHelper->firstSuccessfulLoad) {
        testHelper->firstSuccessfulLoad = true;
        return;
    }

    // Second load worked, we are done.
    EXPECT_TRUE(testHelper->firstSuccessfulLoad);
    if (!testHelper->secondSuccessfulLoad) {
        testHelper->secondSuccessfulLoad = true;
        return;
    }
}

// This test will load a blank page and next kill WebProcess, the expected
// result is that a call to page load should spawn a new WebProcess.
TEST(WebKit, LoadPageAfterCrash)
{
    WebKit2CrashLoader helper;
    helper.loadUrl();
    Util::run(&helper.firstSuccessfulLoad);
    helper.terminateWebProcess();
    helper.loadUrl();
    Util::run(&helper.secondSuccessfulLoad);
}

} // namespace TestWebKitAPI

#endif
