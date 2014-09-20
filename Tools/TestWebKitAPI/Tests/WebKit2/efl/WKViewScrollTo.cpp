/*
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "ewk_view_private.h"
#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include <WebKit/WKContext.h>
#include <WebKit/WKRetainPtr.h>
#include "Test.h"

namespace TestWebKitAPI {

static bool finishedLoad = false;

static void didFinishLoadForFrame(WKPageRef, WKFrameRef, WKTypeRef, const void*)
{
    finishedLoad = true;
}

TEST(WebKit2, WKViewScrollTo)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreate());
    PlatformWebView webView(context.get());
    WKRetainPtr<WKViewRef> view = EWKViewGetWKView(webView.platformView());

    WKPageSetUseFixedLayout(webView.page(), true);

    WKPageLoaderClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));
    loaderClient.base.version = 0;
    loaderClient.didFinishLoadForFrame = didFinishLoadForFrame;
    WKPageSetPageLoaderClient(webView.page(), &loaderClient.base);

    WKViewClientV0 viewClient;
    memset(&viewClient, 0, sizeof(viewClient));
    viewClient.base.version = 0;
    viewClient.didChangeContentsPosition = 0;
    WKViewSetViewClient(view.get(), &viewClient.base);

    // Load page.
    WKRetainPtr<WKURLRef> url(AdoptWK, Util::createURLForResource("efl/scrollTo", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&finishedLoad);

    WKPoint currentPagePosition = WKViewGetContentPosition(view.get());
    EXPECT_EQ(100, currentPagePosition.x);
    EXPECT_EQ(300, currentPagePosition.y);
}

} // TestWebKitAPI
