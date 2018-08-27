/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include <WebKit/WKPagePrivate.h>
#include <WebKit/WKSessionStateRef.h>

namespace TestWebKitAPI {

static bool didFinishLoad;
static bool didChangeBackForwardList;

static void didFinishLoadForFrame(WKPageRef, WKFrameRef, WKTypeRef, const void*)
{
    didFinishLoad = true;
}

static void didChangeBackForwardListForPage(WKPageRef, WKBackForwardListItemRef addedItem, WKArrayRef, const void*)
{
    didChangeBackForwardList = true;
}

static void setPageLoaderClient(WKPageRef page)
{
    WKPageLoaderClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.didFinishLoadForFrame = didFinishLoadForFrame;
    loaderClient.didChangeBackForwardList = didChangeBackForwardListForPage;

    WKPageSetPageLoaderClient(page, &loaderClient.base);
}

static WKRetainPtr<WKDataRef> createSessionStateData(WKContextRef context)
{
    PlatformWebView webView(context);
    setPageLoaderClient(webView.page());

    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple", "html")).get());
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    auto sessionState = adoptWK(static_cast<WKSessionStateRef>(WKPageCopySessionState(webView.page(), reinterpret_cast<void*>(1), nullptr)));
    return adoptWK(WKSessionStateCopyData(sessionState.get()));
}

TEST(WebKit, RestoreSessionStateWithoutNavigation)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreate());

    PlatformWebView webView(context.get());
    setPageLoaderClient(webView.page());

    WKRetainPtr<WKDataRef> data = createSessionStateData(context.get());
    EXPECT_NOT_NULL(data);

    auto sessionState = adoptWK(WKSessionStateCreateFromData(data.get()));
    WKPageRestoreFromSessionStateWithoutNavigation(webView.page(), sessionState.get());

    Util::run(&didChangeBackForwardList);

    WKRetainPtr<WKURLRef> committedURL = adoptWK(WKPageCopyCommittedURL(webView.page()));
    EXPECT_NULL(committedURL.get());

    auto backForwardList = WKPageGetBackForwardList(webView.page());
    auto currentItem = WKBackForwardListGetCurrentItem(backForwardList);
    auto currentItemURL = adoptWK(WKBackForwardListItemCopyURL(currentItem));
    auto expectedURL = adoptWK(Util::createURLForResource("simple", "html"));
    EXPECT_NOT_NULL(expectedURL);
    EXPECT_TRUE(WKURLIsEqual(currentItemURL.get(), expectedURL.get()));
}

} // namespace TestWebKitAPI

#endif
