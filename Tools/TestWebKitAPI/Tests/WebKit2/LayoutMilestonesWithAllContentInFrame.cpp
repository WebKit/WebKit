/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "Test.h"
#include <WebKit2/WKContextPrivate.h>
#include <WebKit2/WKRetainPtr.h>

namespace TestWebKitAPI {

static bool testDone;

static void didLayout(WKPageRef page, WKLayoutMilestones milestones, WKTypeRef, const void* clientInfo)
{
    // This test ensures that the DidFirstVisuallyNonEmptyLayout will be reached for the main frame
    // even when all of the content is in a subframe.
    if (milestones & kWKDidFirstVisuallyNonEmptyLayout)
        testDone = true;
}

TEST(WebKit2, LayoutMilestonesWithAllContentInFrame)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreate());
    PlatformWebView webView(context.get());

    WKPageLoaderClientV3 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 3;
    loaderClient.base.clientInfo = &webView;
    loaderClient.didLayout = didLayout;

    WKPageSetPageLoaderClient(webView.page(), &loaderClient.base);

    WKPageListenForLayoutMilestones(webView.page(), kWKDidFirstVisuallyNonEmptyLayout);
    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("all-content-in-one-iframe", "html")).get());

    Util::run(&testDone);
    EXPECT_TRUE(testDone);
}

} // namespace TestWebKitAPI
