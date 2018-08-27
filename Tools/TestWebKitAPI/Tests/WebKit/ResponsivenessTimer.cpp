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

#include "config.h"

#if WK_HAVE_C_SPI

#include "PlatformUtilities.h"
#include "PlatformWebView.h"

namespace TestWebKitAPI {

static bool didFinishLoad { false };
static bool didBecomeUnresponsive { false };

static void didFinishLoadForFrame(WKPageRef, WKFrameRef, WKTypeRef, const void*)
{
    didFinishLoad = true;
}

static void processDidBecomeUnresponsive(WKPageRef page, const void*)
{
    didBecomeUnresponsive = true;
}

static void setPageLoaderClient(WKPageRef page)
{
    WKPageLoaderClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.didFinishLoadForFrame = didFinishLoadForFrame;
    loaderClient.processDidBecomeUnresponsive = processDidBecomeUnresponsive;

    WKPageSetPageLoaderClient(page, &loaderClient.base);
}

TEST(WebKit, ResponsivenessTimerShouldNotFireAfterTearDown)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreate());
    // The two views need to share the same WebContent process.
    WKContextSetMaximumNumberOfProcesses(context.get(), 1);

    PlatformWebView webView1(context.get());
    setPageLoaderClient(webView1.page());

    WKPageLoadURL(webView1.page(), adoptWK(Util::createURLForResource("simple", "html")).get());
    Util::run(&didFinishLoad);

    EXPECT_FALSE(didBecomeUnresponsive);

    PlatformWebView webView2(context.get());
    setPageLoaderClient(webView2.page());

    didFinishLoad = false;
    WKPageLoadURL(webView2.page(), adoptWK(Util::createURLForResource("simple", "html")).get());
    Util::run(&didFinishLoad);

    EXPECT_FALSE(didBecomeUnresponsive);

    // Call stopLoading() and close() on the first page in quick succession.
    WKPageStopLoading(webView1.page());
    WKPageClose(webView1.page());

    // We need to wait here because it takes 3 seconds for a process to be recognized as unresponsive.
    Util::sleep(4);

    // We should not report the second page sharing the same process as unresponsive.
    EXPECT_FALSE(didBecomeUnresponsive);
}

} // namespace TestWebKitAPI

#endif
