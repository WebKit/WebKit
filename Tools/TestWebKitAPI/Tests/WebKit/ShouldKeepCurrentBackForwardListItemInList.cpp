/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

// This test navigates from simple.html, to simple2.html, to simple3.html
// When navigating from simple2 to simple3, it disallows the simple2 back/forward list item from staying in the list
// It then navigates back from simple3, expecting to land at simple.

namespace TestWebKitAPI {

static bool finished = false;
static bool successfulSoFar = true;
static int navigationNumber = 0;

static bool itemURLLastComponentIsString(WKBackForwardListItemRef item, const char* string)
{
    WKRetainPtr<WKURLRef> url = adoptWK(WKBackForwardListItemCopyURL(item));
    WKRetainPtr<WKStringRef> path = adoptWK(WKURLCopyLastPathComponent(url.get()));

    return WKStringIsEqualToUTF8CString(path.get(), string);
}

static void didFinishLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef, const void*)
{
    // Only mark finished when the main frame loads
    if (!WKFrameIsMainFrame(frame))
        return;

    finished = true;
    navigationNumber++;

    WKBackForwardListRef list = WKPageGetBackForwardList(page);
    WKBackForwardListItemRef currentItem = WKBackForwardListGetCurrentItem(list);
    WKBackForwardListItemRef backItem = WKBackForwardListGetBackItem(list);
    WKBackForwardListItemRef forwardItem = WKBackForwardListGetForwardItem(list);
    unsigned forwardCount = WKBackForwardListGetForwardListCount(list);

    // This test should never have a forward list.
    if (forwardCount)
        successfulSoFar = false;

    if (navigationNumber == 1) {
        // We've only performed 1 navigation, we should only have a current item.
        if (!currentItem || !itemURLLastComponentIsString(currentItem, "simple.html"))
            successfulSoFar = false;
        if (backItem || forwardItem)
            successfulSoFar = false;
    } else if (navigationNumber == 2) {
        // On the second navigation, simple2 should be current and simple should be the back item.
        if (!currentItem || !itemURLLastComponentIsString(currentItem, "simple2.html"))
            successfulSoFar = false;
        if (!backItem || !itemURLLastComponentIsString(backItem, "simple.html"))
            successfulSoFar = false;
        if (forwardItem)
            successfulSoFar = false;
    } else if (navigationNumber == 3) {
        // On the third navigation the item for simple2 should have been removed.
        // So simple3 should be current and simple should still be the back item.
        if (!currentItem || !itemURLLastComponentIsString(currentItem, "simple3.html"))
            successfulSoFar = false;
        if (!backItem || !itemURLLastComponentIsString(backItem, "simple.html"))
            successfulSoFar = false;
        if (forwardItem)
            successfulSoFar = false;
    } else if (navigationNumber == 4) {
        // After the fourth navigation (which was a "back" navigation), the item for simple3 should have been removed.
        // So simple should be current and there should be no other items.
        if (!currentItem || !itemURLLastComponentIsString(currentItem, "simple.html"))
            successfulSoFar = false;
        if (backItem || forwardItem)
            successfulSoFar = false;
    }
}

static bool shouldKeepCurrentBackForwardListItemInList(WKPageRef page, WKBackForwardListItemRef item, const void*)
{
    // We make sure the item for "simple2.html" is removed when we navigate to "simple3.html"
    // We also want to make sure the item for "simple3.html" is removed when we go back to "simple.html"
    // So we only want to keep "simple.html"
    return itemURLLastComponentIsString(item, "simple.html");
}

static void setPageLoaderClient(WKPageRef page)
{
    WKPageLoaderClientV5 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 5;
    loaderClient.didFinishLoadForFrame = didFinishLoadForFrame;
    loaderClient.shouldKeepCurrentBackForwardListItemInList = shouldKeepCurrentBackForwardListItemInList;

    WKPageSetPageLoaderClient(page, &loaderClient.base);
}

TEST(WebKit, ShouldKeepCurrentBackForwardListItemInList)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreateWithConfiguration(nullptr));

    PlatformWebView webView(context.get());
    setPageLoaderClient(webView.page());

    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple", "html")).get());
    Util::run(&finished);
    EXPECT_EQ(successfulSoFar, true);

    finished = false;
    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple2", "html")).get());
    Util::run(&finished);
    EXPECT_EQ(successfulSoFar, true);

    finished = false;
    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple3", "html")).get());
    Util::run(&finished);
    EXPECT_EQ(successfulSoFar, true);

    finished = false;
    WKPageGoBack(webView.page());
    Util::run(&finished);

    EXPECT_EQ(successfulSoFar, true);
    EXPECT_EQ(navigationNumber, 4);
}

} // namespace TestWebKitAPI


#endif
