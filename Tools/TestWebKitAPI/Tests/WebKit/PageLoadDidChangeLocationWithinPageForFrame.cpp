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
#include <WebKit/WKRetainPtr.h>

namespace TestWebKitAPI {

static void nullJavaScriptCallback(WKSerializedScriptValueRef, WKErrorRef error, void*)
{
}

static bool didFinishLoad;
static bool didPopStateWithinPage;
static bool didChangeLocationWithinPage;

static void didSameDocumentNavigation(WKPageRef, WKNavigationRef, WKSameDocumentNavigationType type, WKTypeRef, const void*)
{
    if (!didPopStateWithinPage) {
        EXPECT_EQ(static_cast<uint32_t>(kWKSameDocumentNavigationSessionStatePop), type);
        EXPECT_FALSE(didChangeLocationWithinPage);
        didPopStateWithinPage = true;
        return;
    }

    EXPECT_EQ(static_cast<uint32_t>(kWKSameDocumentNavigationAnchorNavigation), type);
    didChangeLocationWithinPage = true;
}

TEST(WebKit, PageLoadDidChangeLocationWithinPage)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = [] (WKPageRef, WKNavigationRef, WKTypeRef, const void*) {
        didFinishLoad = true;
    };
    loaderClient.didSameDocumentNavigation = didSameDocumentNavigation;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("file-with-anchor", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&didFinishLoad);

    WKRetainPtr<WKURLRef> initialURL = adoptWK(WKFrameCopyURL(WKPageGetMainFrame(webView.page())));

    WKPageRunJavaScriptInMainFrame(webView.page(), Util::toWK("clickLink()").get(), 0, nullJavaScriptCallback);
    Util::run(&didChangeLocationWithinPage);

    WKRetainPtr<WKURLRef> urlAfterAnchorClick = adoptWK(WKFrameCopyURL(WKPageGetMainFrame(webView.page())));

    EXPECT_FALSE(WKURLIsEqual(initialURL.get(), urlAfterAnchorClick.get()));
}

} // namespace TestWebKitAPI

#endif
