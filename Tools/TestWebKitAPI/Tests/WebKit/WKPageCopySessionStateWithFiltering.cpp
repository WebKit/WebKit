/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

namespace TestWebKitAPI {

static bool didFinishLoad;
static WKRetainPtr<WKSessionStateRef> sessionStateWithFirstItemRemoved;
static WKRetainPtr<WKSessionStateRef> sessionStateWithAllItemsRemoved;

static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    didFinishLoad = true;
}

static void setPageLoaderClient(WKPageRef page)
{
    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishNavigation;

    WKPageSetPageNavigationClient(page, &loaderClient.base);
}

static bool filterFirstItemCallback(WKPageRef page, WKStringRef valueType, WKTypeRef value, void* context)
{
    if (!WKStringIsEqual(valueType, WKPageGetSessionBackForwardListItemValueType()))
        return true;

    ASSERT(WKGetTypeID(value) == WKBackForwardListItemGetTypeID());
    WKBackForwardListItemRef backForwardListItem = static_cast<WKBackForwardListItemRef>(value);

    WKRetainPtr<WKURLRef> url = adoptWK(WKBackForwardListItemCopyURL(backForwardListItem));
    WKRetainPtr<WKStringRef> path = adoptWK(WKURLCopyLastPathComponent(url.get()));

    return !WKStringIsEqualToUTF8CString(path.get(), "simple.html");
}

static bool filterAllItemsCallback(WKPageRef page, WKStringRef valueType, WKTypeRef value, void* context)
{
    return false;
}

static void createSessionStates(WKContextRef context)
{
    PlatformWebView webView(context);
    setPageLoaderClient(webView.page());

    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple", "html")).get());
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple2", "html")).get());
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple3", "html")).get());
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    WKPageGoBack(webView.page());
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    WKPageGoBack(webView.page());
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    // Should be back on simple.html at this point.

    sessionStateWithFirstItemRemoved = adoptWK(static_cast<WKSessionStateRef>(WKPageCopySessionState(webView.page(), reinterpret_cast<void*>(1), filterFirstItemCallback)));
    sessionStateWithAllItemsRemoved = adoptWK(static_cast<WKSessionStateRef>(WKPageCopySessionState(webView.page(), reinterpret_cast<void*>(1), filterAllItemsCallback)));
}

TEST(WebKit, WKPageCopySessionStateWithFiltering)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreateWithConfiguration(nullptr));

    createSessionStates(context.get());

    EXPECT_NOT_NULL(sessionStateWithFirstItemRemoved);
    PlatformWebView webView1(context.get());
    setPageLoaderClient(webView1.page());
    WKPageRestoreFromSessionState(webView1.page(), sessionStateWithFirstItemRemoved.get());
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    WKBackForwardListRef backForwardList1 = WKPageGetBackForwardList(webView1.page());
    EXPECT_EQ(0u, WKBackForwardListGetBackListCount(backForwardList1));
    EXPECT_EQ(1u, WKBackForwardListGetForwardListCount(backForwardList1));

    EXPECT_NOT_NULL(sessionStateWithAllItemsRemoved);
    PlatformWebView webView2(context.get());
    setPageLoaderClient(webView2.page());
    WKPageRestoreFromSessionState(webView2.page(), sessionStateWithAllItemsRemoved.get());
    // Because the session state ends up being empty, nothing is actually loaded.

    WKBackForwardListRef backForwardList2 = WKPageGetBackForwardList(webView2.page());
    EXPECT_EQ(0u, WKBackForwardListGetBackListCount(backForwardList2));
    EXPECT_EQ(0u, WKBackForwardListGetForwardListCount(backForwardList2));
}

} // namespace TestWebKitAPI

#endif
