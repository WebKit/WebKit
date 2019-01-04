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

#include <WebKit/WKWebsiteDataStoreRef.h>

namespace TestWebKitAPI {

TEST(WebKit, WKPageConfigurationEmpty)
{
    WKRetainPtr<WKPageConfigurationRef> configuration = adoptWK(WKPageConfigurationCreate());

    ASSERT_NULL(WKPageConfigurationGetContext(configuration.get()));
    ASSERT_NULL(WKPageConfigurationGetUserContentController(configuration.get()));
    ASSERT_NULL(WKPageConfigurationGetPageGroup(configuration.get()));
    ASSERT_NULL(WKPageConfigurationGetPreferences(configuration.get()));
    ASSERT_NULL(WKPageConfigurationGetRelatedPage(configuration.get()));
}

static bool didFinishLoad;

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

TEST(WebKit, WKPageConfigurationBasic)
{
    WKRetainPtr<WKPageConfigurationRef> configuration = adoptWK(WKPageConfigurationCreate());
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    WKPageConfigurationSetContext(configuration.get(), context.get());
    
    PlatformWebView webView(configuration.get());
    setPageLoaderClient(webView.page());
    
    WKRetainPtr<WKPageConfigurationRef> copiedConfiguration = adoptWK(WKPageCopyPageConfiguration(webView.page()));
    ASSERT_EQ(context.get(), WKPageConfigurationGetContext(copiedConfiguration.get()));

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("simple", "html"));
    WKPageLoadURL(webView.page(), url.get());

    didFinishLoad = false;
    Util::run(&didFinishLoad);
}

TEST(WebKit, WKPageConfigurationBasicWithDataStore)
{
    WKRetainPtr<WKPageConfigurationRef> configuration = adoptWK(WKPageConfigurationCreate());
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    WKPageConfigurationSetContext(configuration.get(), context.get());
    WKRetainPtr<WKWebsiteDataStoreRef> websiteDataStore = WKWebsiteDataStoreGetDefaultDataStore();
    WKPageConfigurationSetWebsiteDataStore(configuration.get(), websiteDataStore.get());
    
    PlatformWebView webView(configuration.get());
    setPageLoaderClient(webView.page());

    WKRetainPtr<WKPageConfigurationRef> copiedConfiguration = adoptWK(WKPageCopyPageConfiguration(webView.page()));
    ASSERT_EQ(context.get(), WKPageConfigurationGetContext(copiedConfiguration.get()));
    ASSERT_EQ(WKWebsiteDataStoreGetDefaultDataStore(), WKPageConfigurationGetWebsiteDataStore(copiedConfiguration.get()));

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("simple", "html"));
    WKPageLoadURL(webView.page(), url.get());

    didFinishLoad = false;
    Util::run(&didFinishLoad);
}

TEST(WebKit, WKPageConfigurationBasicWithNonPersistentDataStore)
{
    WKRetainPtr<WKPageConfigurationRef> configuration = adoptWK(WKPageConfigurationCreate());
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    WKPageConfigurationSetContext(configuration.get(), context.get());
    WKRetainPtr<WKWebsiteDataStoreRef> websiteDataStore = adoptWK(WKWebsiteDataStoreCreateNonPersistentDataStore());
    WKPageConfigurationSetWebsiteDataStore(configuration.get(), websiteDataStore.get());
    
    PlatformWebView webView(configuration.get());
    setPageLoaderClient(webView.page());

    WKRetainPtr<WKPageConfigurationRef> copiedConfiguration = adoptWK(WKPageCopyPageConfiguration(webView.page()));
    ASSERT_EQ(context.get(), WKPageConfigurationGetContext(copiedConfiguration.get()));
    ASSERT_EQ(websiteDataStore.get(), WKPageConfigurationGetWebsiteDataStore(copiedConfiguration.get()));

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("simple", "html"));
    WKPageLoadURL(webView.page(), url.get());

    didFinishLoad = false;
    Util::run(&didFinishLoad);
}

} // namespace TestWebKitAPI

#endif
