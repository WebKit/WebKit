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

struct TestStatesData {
    TestStatesData(WKContextRef context)
        : webView(context)
        , firstLoad(false)
        , resizeAfterCrash(false)
    {
    }

    PlatformWebView webView;
    bool firstLoad;
    bool resizeAfterCrash;
};

static void didFinishLoad(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    TestStatesData* states = const_cast<TestStatesData*>(static_cast<const TestStatesData*>(clientInfo));
    if (!states->firstLoad) {
        // Loading a blank page worked, next we will kill WebProcess.
        states->firstLoad = true;
        return;
    }

    EXPECT_FALSE(states->resizeAfterCrash);
    states->resizeAfterCrash = true;
}

static void didCrash(WKPageRef page, const void* clientInfo)
{
    TestStatesData* states = const_cast<TestStatesData*>(static_cast<const TestStatesData*>(clientInfo));
    EXPECT_TRUE(states->firstLoad);
    // Resize should work after WebProcess was terminated, unless
    // the port's View is accessing nulled WebProcess related data,
    // which would cause a crash.
    states->webView.resizeTo(100, 200);
    states->webView.resizeTo(300, 400);

    WKPageReload(page);
}

TEST(WebKit, ResizeWindowAfterCrash)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    TestStatesData states(context.get());

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.base.clientInfo = &states;
    loaderClient.didFinishNavigation = didFinishLoad;
    loaderClient.webProcessDidCrash = didCrash;

    WKPageSetPageNavigationClient(states.webView.page(), &loaderClient.base);

    WKRetainPtr<WKURLRef> url = adoptWK(WKURLCreateWithUTF8CString("about:blank"));
    // Load a blank page and next kills WebProcess.
    WKPageLoadURL(states.webView.page(), url.get());
    Util::run(&states.firstLoad);
    WKPageTerminate(states.webView.page());

    // Let's try load a page and see what happens.
    WKPageLoadURL(states.webView.page(), url.get());
    Util::run(&states.resizeAfterCrash);
}

} // namespace TestWebKitAPI

#endif
