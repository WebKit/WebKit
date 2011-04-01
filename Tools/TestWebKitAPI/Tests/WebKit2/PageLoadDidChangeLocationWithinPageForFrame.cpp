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

#include "Test.h"

#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include <WebKit2/WebKit2.h>
#include <WebKit2/WKRetainPtr.h>

namespace TestWebKitAPI {

static void nullJavaScriptCallback(WKSerializedScriptValueRef, WKErrorRef error, void*)
{
}

static bool didFinishLoad;
static void didFinishLoadForFrame(WKPageRef, WKFrameRef, WKTypeRef, const void*)
{
    didFinishLoad = true;
}

static bool didPopStateWithinPage;
static bool didChangeLocationWithinPage;
static void didSameDocumentNavigationForFrame(WKPageRef, WKFrameRef, WKSameDocumentNavigationType type, WKTypeRef, const void*)
{
    if (!didPopStateWithinPage) {
        TEST_ASSERT(type == kWKSameDocumentNavigationSessionStatePop);
        TEST_ASSERT(!didChangeLocationWithinPage);
        didPopStateWithinPage = true;
        return;
    }

    TEST_ASSERT(type == kWKSameDocumentNavigationAnchorNavigation);
    didChangeLocationWithinPage = true;
}

TEST(WebKit2, PageLoadDidChangeLocationWithinPageForFrame)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreate());
    PlatformWebView webView(context.get());

    WKPageLoaderClient loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));
    loaderClient.didFinishLoadForFrame = didFinishLoadForFrame;
    loaderClient.didSameDocumentNavigationForFrame = didSameDocumentNavigationForFrame;
    WKPageSetPageLoaderClient(webView.page(), &loaderClient);

    WKRetainPtr<WKURLRef> url(AdoptWK, Util::createURLForResource("file-with-anchor", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&didFinishLoad);

    WKRetainPtr<WKURLRef> initialURL = adoptWK(WKFrameCopyURL(WKPageGetMainFrame(webView.page())));

    WKPageRunJavaScriptInMainFrame(webView.page(), Util::toWK("clickLink()").get(), 0, nullJavaScriptCallback);
    Util::run(&didChangeLocationWithinPage);

    WKRetainPtr<WKURLRef> urlAfterAnchorClick = adoptWK(WKFrameCopyURL(WKPageGetMainFrame(webView.page())));

    TEST_ASSERT(!WKURLIsEqual(initialURL.get(), urlAfterAnchorClick.get()));
}

} // namespace TestWebKitAPI
