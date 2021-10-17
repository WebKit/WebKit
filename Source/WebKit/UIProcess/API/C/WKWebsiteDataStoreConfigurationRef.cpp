/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WKWebsiteDataStoreConfigurationRef.h"

#include "WKAPICast.h"
#include "WebsiteDataStoreConfiguration.h"

WKTypeID WKWebsiteDataStoreConfigurationGetTypeID()
{
    return WebKit::toAPI(WebKit::WebsiteDataStoreConfiguration::APIType);
}

WKWebsiteDataStoreConfigurationRef WKWebsiteDataStoreConfigurationCreate()
{
    auto configuration = WebKit::WebsiteDataStoreConfiguration::create(WebKit::IsPersistent::Yes);
    return toAPI(&configuration.leakRef());
}

WKStringRef WKWebsiteDataStoreConfigurationCopyApplicationCacheDirectory(WKWebsiteDataStoreConfigurationRef configuration)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(configuration)->applicationCacheDirectory());
}

void WKWebsiteDataStoreConfigurationSetApplicationCacheDirectory(WKWebsiteDataStoreConfigurationRef configuration, WKStringRef directory)
{
    WebKit::toImpl(configuration)->setApplicationCacheDirectory(WebKit::toImpl(directory)->string());
}

WKStringRef WKWebsiteDataStoreConfigurationCopyNetworkCacheDirectory(WKWebsiteDataStoreConfigurationRef configuration)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(configuration)->networkCacheDirectory());
}

void WKWebsiteDataStoreConfigurationSetNetworkCacheDirectory(WKWebsiteDataStoreConfigurationRef configuration, WKStringRef directory)
{
    WebKit::toImpl(configuration)->setNetworkCacheDirectory(WebKit::toImpl(directory)->string());
}

WKStringRef WKWebsiteDataStoreConfigurationCopyIndexedDBDatabaseDirectory(WKWebsiteDataStoreConfigurationRef configuration)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(configuration)->indexedDBDatabaseDirectory());
}

void WKWebsiteDataStoreConfigurationSetIndexedDBDatabaseDirectory(WKWebsiteDataStoreConfigurationRef configuration, WKStringRef directory)
{
    WebKit::toImpl(configuration)->setIndexedDBDatabaseDirectory(WebKit::toImpl(directory)->string());
}

WKStringRef WKWebsiteDataStoreConfigurationCopyLocalStorageDirectory(WKWebsiteDataStoreConfigurationRef configuration)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(configuration)->localStorageDirectory());
}

void WKWebsiteDataStoreConfigurationSetLocalStorageDirectory(WKWebsiteDataStoreConfigurationRef configuration, WKStringRef directory)
{
    WebKit::toImpl(configuration)->setLocalStorageDirectory(WebKit::toImpl(directory)->string());
}

WKStringRef WKWebsiteDataStoreConfigurationCopyWebSQLDatabaseDirectory(WKWebsiteDataStoreConfigurationRef configuration)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(configuration)->webSQLDatabaseDirectory());
}

void WKWebsiteDataStoreConfigurationSetWebSQLDatabaseDirectory(WKWebsiteDataStoreConfigurationRef configuration, WKStringRef directory)
{
    WebKit::toImpl(configuration)->setWebSQLDatabaseDirectory(WebKit::toImpl(directory)->string());
}

WKStringRef WKWebsiteDataStoreConfigurationCopyCacheStorageDirectory(WKWebsiteDataStoreConfigurationRef configuration)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(configuration)->cacheStorageDirectory());
}

void WKWebsiteDataStoreConfigurationSetCacheStorageDirectory(WKWebsiteDataStoreConfigurationRef configuration, WKStringRef directory)
{
    WebKit::toImpl(configuration)->setCacheStorageDirectory(WebKit::toImpl(directory)->string());
}

WKStringRef WKWebsiteDataStoreConfigurationCopyMediaKeysStorageDirectory(WKWebsiteDataStoreConfigurationRef configuration)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(configuration)->mediaKeysStorageDirectory());
}

void WKWebsiteDataStoreConfigurationSetMediaKeysStorageDirectory(WKWebsiteDataStoreConfigurationRef configuration, WKStringRef directory)
{
    WebKit::toImpl(configuration)->setMediaKeysStorageDirectory(WebKit::toImpl(directory)->string());
}

WKStringRef WKWebsiteDataStoreConfigurationCopyResourceLoadStatisticsDirectory(WKWebsiteDataStoreConfigurationRef configuration)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(configuration)->resourceLoadStatisticsDirectory());
}

void WKWebsiteDataStoreConfigurationSetResourceLoadStatisticsDirectory(WKWebsiteDataStoreConfigurationRef configuration, WKStringRef directory)
{
    WebKit::toImpl(configuration)->setResourceLoadStatisticsDirectory(WebKit::toImpl(directory)->string());
}

WKStringRef WKWebsiteDataStoreConfigurationCopyServiceWorkerRegistrationDirectory(WKWebsiteDataStoreConfigurationRef configuration)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(configuration)->serviceWorkerRegistrationDirectory());
}

void WKWebsiteDataStoreConfigurationSetServiceWorkerRegistrationDirectory(WKWebsiteDataStoreConfigurationRef configuration, WKStringRef directory)
{
    WebKit::toImpl(configuration)->setServiceWorkerRegistrationDirectory(WebKit::toImpl(directory)->string());
}

uint64_t WKWebsiteDataStoreConfigurationGetPerOriginStorageQuota(WKWebsiteDataStoreConfigurationRef configuration)
{
    return WebKit::toImpl(configuration)->perOriginStorageQuota();
}

void WKWebsiteDataStoreConfigurationSetPerOriginStorageQuota(WKWebsiteDataStoreConfigurationRef configuration, uint64_t quota)
{
    WebKit::toImpl(configuration)->setPerOriginStorageQuota(quota);
}

bool WKWebsiteDataStoreConfigurationGetNetworkCacheSpeculativeValidationEnabled(WKWebsiteDataStoreConfigurationRef configuration)
{
    return WebKit::toImpl(configuration)->networkCacheSpeculativeValidationEnabled();
}

void WKWebsiteDataStoreConfigurationSetNetworkCacheSpeculativeValidationEnabled(WKWebsiteDataStoreConfigurationRef configuration, bool enabled)
{
    WebKit::toImpl(configuration)->setNetworkCacheSpeculativeValidationEnabled(enabled);
}

bool WKWebsiteDataStoreConfigurationGetTestingSessionEnabled(WKWebsiteDataStoreConfigurationRef configuration)
{
    return WebKit::toImpl(configuration)->testingSessionEnabled();
}

void WKWebsiteDataStoreConfigurationSetTestingSessionEnabled(WKWebsiteDataStoreConfigurationRef configuration, bool enabled)
{
    WebKit::toImpl(configuration)->setTestingSessionEnabled(enabled);
}

bool WKWebsiteDataStoreConfigurationGetStaleWhileRevalidateEnabled(WKWebsiteDataStoreConfigurationRef configuration)
{
    return WebKit::toImpl(configuration)->staleWhileRevalidateEnabled();
}

void WKWebsiteDataStoreConfigurationSetStaleWhileRevalidateEnabled(WKWebsiteDataStoreConfigurationRef configuration, bool enabled)
{
    WebKit::toImpl(configuration)->setStaleWhileRevalidateEnabled(enabled);
}
