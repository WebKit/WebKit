/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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
#include <WebKit/WKPageConfigurationRef.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKWebsiteDataStoreConfigurationRef.h>
#include <WebKit/WKWebsiteDataStoreRef.h>
#include <wtf/FileSystem.h>

namespace TestWebKitAPI {

class CookieStorageFile : public testing::Test {
public:
    void SetUp() final
    {
        m_cookieStorageDirectory = FileSystem::createTemporaryDirectory();
    }

    void TearDown() final
    {
        FileSystem::deleteNonEmptyDirectory(m_cookieStorageDirectory);
    }

    String m_cookieStorageDirectory;
};

static void didFinishNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* context)
{
    *static_cast<bool*>(const_cast<void*>(context)) = true;
}

TEST_F(CookieStorageFile, CustomPath)
{
    bool didFinishLoad;
    auto cookieStorageFile = FileSystem::pathByAppendingComponent(m_cookieStorageDirectory, "cookie.jar.db"_s);

    auto context = adoptWK(WKContextCreateWithConfiguration(nullptr));

    auto websiteDataStoreConf = adoptWK(WKWebsiteDataStoreConfigurationCreate());
    WKWebsiteDataStoreConfigurationSetCookieStorageFile(websiteDataStoreConf.get(), Util::toWK(cookieStorageFile.utf8().data()).get());

    ASSERT_TRUE(WKStringIsEqualToUTF8CString(adoptWK(WKWebsiteDataStoreConfigurationCopyCookieStorageFile(websiteDataStoreConf.get())).get(), cookieStorageFile.utf8().data()));

    auto websiteDataStore = adoptWK(WKWebsiteDataStoreCreateWithConfiguration(websiteDataStoreConf.get()));

    auto pageConfiguration = adoptWK(WKPageConfigurationCreate());
    WKPageConfigurationSetContext(pageConfiguration.get(), context.get());
    WKPageConfigurationSetWebsiteDataStore(pageConfiguration.get(), websiteDataStore.get());

    PlatformWebView webView(pageConfiguration.get());

    WKPageNavigationClientV0 loaderClient { };
    loaderClient.base.version = 0;
    loaderClient.base.clientInfo = &didFinishLoad;
    loaderClient.didFinishNavigation = didFinishNavigation;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    ASSERT_FALSE(FileSystem::fileExists(cookieStorageFile));

    auto url = adoptWK(Util::createURLForResource("simple", "html"));
    WKPageLoadURL(webView.page(), url.get());

    didFinishLoad = false;
    Util::run(&didFinishLoad);

    ASSERT_TRUE(FileSystem::fileExists(cookieStorageFile));
}

} // namespace TestWebKitAPI
