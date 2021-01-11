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

#include "WebContext.h"

#include "StringUtils.h"
#include <WebKit/WKContextConfigurationPlayStation.h>
#include <WebKit/WKContextPrivate.h>
#include <WebKit/WKPreferencesRef.h>
#include <WebKit/WKPreferencesRefPrivate.h>
#include <WebKit/WKWebsiteDataStoreConfigurationRef.h>
#include <WebKit/WKWebsiteDataStoreRef.h>
#include <WebKit/WKWebsiteDataStoreRefCurl.h>

#define MAKE_PROCESS_PATH(x) "/app0/" #x ".self"

std::weak_ptr<WebContext> WebContext::s_instance;

std::shared_ptr<WebContext> WebContext::singleton()
{
    auto ret = s_instance.lock();
    if (!ret) {
        ret = std::shared_ptr<WebContext>(new WebContext());
        s_instance = ret;
    }
    return s_instance.lock();
}

WebContext::WebContext()
{
    WKRetainPtr<WKContextConfigurationRef> contextConfiguration = adoptWK(WKContextConfigurationCreate());
    WKRetainPtr<WKStringRef> webProcessPath = adoptWK(WKStringCreateWithUTF8CString(MAKE_PROCESS_PATH(WebProcess)));
    WKRetainPtr<WKStringRef> networkProcessPath = adoptWK(WKStringCreateWithUTF8CString(MAKE_PROCESS_PATH(NetworkProcess)));
    WKRetainPtr<WKStringRef> applicationCacheDirectory = adoptWK(WKStringCreateWithUTF8CString("/data/minibrowser/ApplicationCache"));
    WKRetainPtr<WKStringRef> networkCacheDirectory = adoptWK(WKStringCreateWithUTF8CString("/data/minibrowser/NetworkCache"));
    WKRetainPtr<WKStringRef> indexedDBDirectory = adoptWK(WKStringCreateWithUTF8CString("/data/minibrowser/IndexedDB"));
    WKRetainPtr<WKStringRef> localStorageDirectory = adoptWK(WKStringCreateWithUTF8CString("/data/minibrowser/LocalStorage"));
    WKRetainPtr<WKStringRef> webSQLDirectory = adoptWK(WKStringCreateWithUTF8CString("/data/minibrowser/WebSQL"));
    WKRetainPtr<WKStringRef> cacheStorageDirectory = adoptWK(WKStringCreateWithUTF8CString("/data/minibrowser/CacheStroage"));
    WKRetainPtr<WKStringRef> mediaKeyStorageDirectory = adoptWK(WKStringCreateWithUTF8CString("/data/minibrowser/MediaKey"));
    WKRetainPtr<WKStringRef> resourceLoadStatisticsDirectory = adoptWK(WKStringCreateWithUTF8CString("/data/minibrowser/ResourceLoadStatistics"));
    WKRetainPtr<WKStringRef> serviceWorkerRegistrationDirectory = adoptWK(WKStringCreateWithUTF8CString("/data/minibrowser/ServiceWorkerRegistration"));

    WKContextConfigurationSetUserId(contextConfiguration.get(), -1);
    WKContextConfigurationSetWebProcessPath(contextConfiguration.get(), webProcessPath.get());
    WKContextConfigurationSetNetworkProcessPath(contextConfiguration.get(), networkProcessPath.get());

    auto dataStoreConfiguration = adoptWK(WKWebsiteDataStoreConfigurationCreate());
    WKWebsiteDataStoreConfigurationSetApplicationCacheDirectory(dataStoreConfiguration.get(), applicationCacheDirectory.get());
    WKWebsiteDataStoreConfigurationSetNetworkCacheDirectory(dataStoreConfiguration.get(), networkCacheDirectory.get());
    WKWebsiteDataStoreConfigurationSetIndexedDBDatabaseDirectory(dataStoreConfiguration.get(), indexedDBDirectory.get());
    WKWebsiteDataStoreConfigurationSetLocalStorageDirectory(dataStoreConfiguration.get(), localStorageDirectory.get());
    WKWebsiteDataStoreConfigurationSetWebSQLDatabaseDirectory(dataStoreConfiguration.get(), webSQLDirectory.get());
    WKWebsiteDataStoreConfigurationSetCacheStorageDirectory(dataStoreConfiguration.get(), cacheStorageDirectory.get());
    WKWebsiteDataStoreConfigurationSetMediaKeysStorageDirectory(dataStoreConfiguration.get(), mediaKeyStorageDirectory.get());
    WKWebsiteDataStoreConfigurationSetResourceLoadStatisticsDirectory(dataStoreConfiguration.get(), resourceLoadStatisticsDirectory.get());
    WKWebsiteDataStoreConfigurationSetServiceWorkerRegistrationDirectory(dataStoreConfiguration.get(), serviceWorkerRegistrationDirectory.get());
    WKWebsiteDataStoreConfigurationSetNetworkCacheSpeculativeValidationEnabled(dataStoreConfiguration.get(), true);
    WKWebsiteDataStoreConfigurationSetTestingSessionEnabled(dataStoreConfiguration.get(), true);
    WKWebsiteDataStoreConfigurationSetStaleWhileRevalidateEnabled(dataStoreConfiguration.get(), true);
    m_websiteDataStore = WKWebsiteDataStoreCreateWithConfiguration(dataStoreConfiguration.get());

    m_context = adoptWK(WKContextCreateWithConfiguration(contextConfiguration.get()));
    WKContextSetPrimaryWebsiteDataStore(m_context.get(), m_websiteDataStore.get());
    WKContextSetUsesSingleWebProcess(m_context.get(), true);

    WKRetainPtr<WKStringRef> groupName = adoptWK(WKStringCreateWithUTF8CString("Default"));
    m_pageGroup = adoptWK(WKPageGroupCreateWithIdentifier(groupName.get()));

    m_preferencesMaster = adoptWK(WKPreferencesCreate());
    WKPreferencesSetFullScreenEnabled(m_preferencesMaster.get(), true);
    WKPreferencesSetDNSPrefetchingEnabled(m_preferencesMaster.get(), true);
    WKPreferencesSetNeedsSiteSpecificQuirks(m_preferencesMaster.get(), false);
}

WebContext::~WebContext()
{
}

void WebContext::addWindow(WebViewWindow* window)
{
    m_windows.push_back(window);
}

void WebContext::removeWindow(WebViewWindow* window)
{
    m_windows.remove(window);
}
