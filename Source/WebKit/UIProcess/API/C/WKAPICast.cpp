/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
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
#include "WKAPICast.h"

namespace WebKit {

OptionSet<WebsiteDataType> toWebsiteDataTypes(WKWebsiteDataTypes websiteDataTypes)
{
    OptionSet<WebsiteDataType> result;

    if (websiteDataTypes & kWKWebsiteDataTypesCookies)
        result.add(WebsiteDataType::Cookies);
    if (websiteDataTypes & kWKWebsiteDataTypesDiskCache)
        result.add(WebsiteDataType::DiskCache);
    if (websiteDataTypes & kWKWebsiteDataTypesMemoryCache)
        result.add(WebsiteDataType::MemoryCache);
    if (websiteDataTypes & kWKWebsiteDataTypesOfflineWebApplicationCache)
        result.add(WebsiteDataType::OfflineWebApplicationCache);
    if (websiteDataTypes & kWKWebsiteDataTypesSessionStorage)
        result.add(WebsiteDataType::SessionStorage);
    if (websiteDataTypes & kWKWebsiteDataTypesLocalStorage)
        result.add(WebsiteDataType::LocalStorage);
    if (websiteDataTypes & kWKWebsiteDataTypesWebSQLDatabases)
        result.add(WebsiteDataType::WebSQLDatabases);
    if (websiteDataTypes & kWKWebsiteDataTypesIndexedDBDatabases)
        result.add(WebsiteDataType::IndexedDBDatabases);
    if (websiteDataTypes & kWKWebsiteDataTypesMediaKeys)
        result.add(WebsiteDataType::MediaKeys);
    if (websiteDataTypes & kWKWebsiteDataTypesHSTSCache)
        result.add(WebsiteDataType::HSTSCache);
    if (websiteDataTypes & kWKWebsiteDataTypesSearchFieldRecentSearches)
        result.add(WebsiteDataType::SearchFieldRecentSearches);
    if (websiteDataTypes & kWKWebsiteDataTypesResourceLoadStatistics)
        result.add(WebsiteDataType::ResourceLoadStatistics);
    if (websiteDataTypes & kWKWebsiteDataTypesCredentials)
        result.add(WebsiteDataType::Credentials);
#if ENABLE(SERVICE_WORKER)
    if (websiteDataTypes & kWKWebsiteDataTypesServiceWorkerRegistrations)
        result.add(WebsiteDataType::ServiceWorkerRegistrations);
#endif
    if (websiteDataTypes & kWKWebsiteDataTypesDOMCache)
        result.add(WebsiteDataType::DOMCache);
    if (websiteDataTypes & kWKWebsiteDataTypesDeviceIdHashSalt)
        result.add(WebsiteDataType::DeviceIdHashSalt);
    if (websiteDataTypes & kWKWebsiteDataTypesPrivateClickMeasurements)
        result.add(WebsiteDataType::PrivateClickMeasurements);
#if HAVE(ALTERNATIVE_SERVICE)
    if (websiteDataTypes & kWKWebsiteDataTypesAlternativeServices)
        result.add(WebsiteDataType::AlternativeServices);
#endif
    if (websiteDataTypes & kWKWebsiteDataTypesFileSystem)
        result.add(WebsiteDataType::FileSystem);
#if ENABLE(SERVICE_WORKER)
    if (websiteDataTypes & kWKWebsiteDataTypesBackgroundFetchStorage)
        result.add(WebsiteDataType::BackgroundFetchStorage);
#endif

    return result;
}

WKWebsiteDataTypes toAPI(const OptionSet<WebsiteDataType>& websiteDataTypes)
{
    WKWebsiteDataTypes result = 0;

    if (websiteDataTypes.contains(WebsiteDataType::Cookies))
        result |= kWKWebsiteDataTypesCookies;
    if (websiteDataTypes.contains(WebsiteDataType::DiskCache))
        result |= kWKWebsiteDataTypesDiskCache;
    if (websiteDataTypes.contains(WebsiteDataType::MemoryCache))
        result |= kWKWebsiteDataTypesMemoryCache;
    if (websiteDataTypes.contains(WebsiteDataType::OfflineWebApplicationCache))
        result |= kWKWebsiteDataTypesOfflineWebApplicationCache;
    if (websiteDataTypes.contains(WebsiteDataType::SessionStorage))
        result |= kWKWebsiteDataTypesSessionStorage;
    if (websiteDataTypes.contains(WebsiteDataType::LocalStorage))
        result |= kWKWebsiteDataTypesLocalStorage;
    if (websiteDataTypes.contains(WebsiteDataType::WebSQLDatabases))
        result |= kWKWebsiteDataTypesWebSQLDatabases;
    if (websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases))
        result |= kWKWebsiteDataTypesIndexedDBDatabases;
    if (websiteDataTypes.contains(WebsiteDataType::MediaKeys))
        result |= kWKWebsiteDataTypesMediaKeys;
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache))
        result |= kWKWebsiteDataTypesHSTSCache;
    if (websiteDataTypes.contains(WebsiteDataType::SearchFieldRecentSearches))
        result |= kWKWebsiteDataTypesSearchFieldRecentSearches;
    if (websiteDataTypes.contains(WebsiteDataType::ResourceLoadStatistics))
        result |= kWKWebsiteDataTypesResourceLoadStatistics;
    if (websiteDataTypes.contains(WebsiteDataType::Credentials))
        result |= kWKWebsiteDataTypesCredentials;
#if ENABLE(SERVICE_WORKER)
    if (websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations))
        result |= kWKWebsiteDataTypesServiceWorkerRegistrations;
#endif
    if (websiteDataTypes.contains(WebsiteDataType::DOMCache))
        result |= kWKWebsiteDataTypesDOMCache;
    if (websiteDataTypes.contains(WebsiteDataType::DeviceIdHashSalt))
        result |= kWKWebsiteDataTypesDeviceIdHashSalt;
    if (websiteDataTypes.contains(WebsiteDataType::PrivateClickMeasurements))
        result |= kWKWebsiteDataTypesPrivateClickMeasurements;
#if HAVE(ALTERNATIVE_SERVICE)
    if (websiteDataTypes.contains(WebsiteDataType::AlternativeServices))
        result |= kWKWebsiteDataTypesAlternativeServices;
#endif
    if (websiteDataTypes.contains(WebsiteDataType::FileSystem))
        result |= kWKWebsiteDataTypesFileSystem;
#if ENABLE(SERVICE_WORKER)
    if (websiteDataTypes.contains(WebsiteDataType::BackgroundFetchStorage))
        result |= kWKWebsiteDataTypesBackgroundFetchStorage;
#endif

    return result;
}

} // namespace WebKit
