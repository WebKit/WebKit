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

#if WK_HAVE_C_SPI

#include "JavaScriptTest.h"
#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include "Test.h"
#include <JavaScriptCore/JSContextRef.h>
#include <WebKit/WKContextPrivate.h>
#include <WebKit/WKPagePrivate.h>
#include <WebKit/WKSerializedScriptValue.h>

namespace TestWebKitAPI {

static bool testDone;

static const unsigned pageLength = 100;
static const unsigned pageGap = 100;
static const unsigned expectedPageCount = 20;

static void didLayout(WKPageRef page, WKPageRenderingProgressEvents milestones, WKTypeRef, const void* clientInfo)
{
    if (milestones & kWKDidFirstLayoutAfterSuppressedIncrementalRendering) {
        PlatformWebView* webView = (PlatformWebView*)clientInfo;

        unsigned pageCount = WKPageGetPageCount(page);
        EXPECT_EQ(expectedPageCount, pageCount);

        webView->resizeTo((pageLength * pageCount) + (pageGap * (pageCount - 1)), 500);
        EXPECT_JS_EQ(page, "window.scrollX", "0");

        testDone = true;
    }
}

TEST(WebKit, ResizeReversePaginatedWebView)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKPageNavigationClientV3 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 3;
    loaderClient.base.clientInfo = &webView;
    loaderClient.renderingProgressDidChange = didLayout;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKPageListenForLayoutMilestones(webView.page(), kWKDidFirstLayoutAfterSuppressedIncrementalRendering);

    WKPageGroupRef pageGroup =  WKPageGetPageGroup(webView.page());
    WKPreferencesRef preferences = WKPageGroupGetPreferences(pageGroup);
    WKPreferencesSetSuppressesIncrementalRendering(preferences, true);

    WKPageSetPaginationMode(webView.page(), kWKPaginationModeRightToLeft);
    WKPageSetPageLength(webView.page(), pageLength);
    WKPageSetGapBetweenPages(webView.page(), pageGap);
    WKPageSetPaginationBehavesLikeColumns(webView.page(), true);

    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("lots-of-text-vertical-lr", "html")).get());

    Util::run(&testDone);
    EXPECT_TRUE(testDone);
}

} // namespace TestWebKitAPI

#endif
