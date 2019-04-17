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
#include <WebKit/WKPreferencesRefPrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKWebsiteDataStoreRef.h>

namespace TestWebKitAPI {

static bool testDone;

static void didNavigateWithNavigationData(WKContextRef context, WKPageRef page, WKNavigationDataRef navigationData, WKFrameRef frame, const void* clientInfo)
{
    // This should never be called when navigating in Private Browsing.
    FAIL();
}

static void didSameDocumentNavigation(WKPageRef page, WKNavigationRef, WKSameDocumentNavigationType type, WKTypeRef userData, const void *clientInfo)
{
    testDone = true;
}

TEST(WebKit, EphemeralSessionPushStateNoHistoryCallback)
{
    auto configuration = adoptWK(WKPageConfigurationCreate());

    auto context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    WKPageConfigurationSetContext(configuration.get(), context.get());

    auto websiteDataStore = adoptWK(WKWebsiteDataStoreCreateNonPersistentDataStore());
    WKPageConfigurationSetWebsiteDataStore(configuration.get(), websiteDataStore.get());

    WKContextHistoryClientV0 historyClient;
    memset(&historyClient, 0, sizeof(historyClient));

    historyClient.base.version = 0;
    historyClient.didNavigateWithNavigationData = didNavigateWithNavigationData;

    WKContextSetHistoryClient(context.get(), &historyClient.base);

    PlatformWebView webView(configuration.get());

    WKPageNavigationClientV0 pageLoaderClient;
    memset(&pageLoaderClient, 0, sizeof(pageLoaderClient));

    pageLoaderClient.base.version = 0;
    pageLoaderClient.didSameDocumentNavigation = didSameDocumentNavigation;

    WKPageSetPageNavigationClient(webView.page(), &pageLoaderClient.base);

    WKRetainPtr<WKPreferencesRef> preferences = adoptWK(WKPreferencesCreate());
    WKPreferencesSetUniversalAccessFromFileURLsAllowed(preferences.get(), true);

    WKPageGroupRef pageGroup = WKPageGetPageGroup(webView.page());
    WKPageGroupSetPreferences(pageGroup, preferences.get());

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("push-state", "html"));
    WKPageLoadURL(webView.page(), url.get());

    Util::run(&testDone);
}

} // namespace TestWebKitAPI

#endif
