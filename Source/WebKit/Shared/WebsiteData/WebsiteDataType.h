/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/text/ASCIILiteral.h>

namespace WebKit {

enum class WebsiteDataType : uint32_t {
    Cookies = 1 << 0,
    DiskCache = 1 << 1,
    MemoryCache = 1 << 2,
    LegacyOfflineWebApplicationCachePlaceholder = 1 << 3,
    SessionStorage = 1 << 4,
    LocalStorage = 1 << 5,
    WebSQLDatabases = 1 << 6,
    IndexedDBDatabases = 1 << 7,
    MediaKeys = 1 << 8,
    HSTSCache = 1 << 9,
    SearchFieldRecentSearches = 1 << 10,
    ResourceLoadStatistics = 1 << 12,
    Credentials = 1 << 13,
    ServiceWorkerRegistrations = 1 << 14,
    DOMCache = 1 << 15,
    DeviceIdHashSalt = 1 << 16,
    PrivateClickMeasurements = 1 << 17,
#if HAVE(ALTERNATIVE_SERVICE)
    AlternativeServices = 1 << 18,
#endif
    FileSystem = 1 << 19,
    BackgroundFetchStorage = 1 << 20,
#if ENABLE(SCREEN_TIME)
    ScreenTime = 1 << 21,
#endif
    EnhancedSecurityRecord = 1 << 22,
};

inline ASCIILiteral toString(WebsiteDataType type)
{
    switch (type) {
    case WebsiteDataType::Cookies:
        return "Cookies"_s;
    case WebsiteDataType::DiskCache:
        return "DiskCache"_s;
    case WebsiteDataType::MemoryCache:
        return "MemoryCache"_s;
    case WebsiteDataType::LegacyOfflineWebApplicationCachePlaceholder:
        return "OfflineWebApplicationCache"_s;
    case WebsiteDataType::SessionStorage:
        return "SessionStorage"_s;
    case WebsiteDataType::LocalStorage:
        return "LocalStorage"_s;
    case WebsiteDataType::WebSQLDatabases:
        return "WebSQLDatabases"_s;
    case WebsiteDataType::IndexedDBDatabases:
        return "IndexedDBDatabases"_s;
    case WebsiteDataType::MediaKeys:
        return "MediaKeys"_s;
    case WebsiteDataType::HSTSCache:
        return "HSTSCache"_s;
    case WebsiteDataType::SearchFieldRecentSearches:
        return "SearchFieldRecentSearches"_s;
    case WebsiteDataType::ResourceLoadStatistics:
        return "ResourceLoadStatistics"_s;
    case WebsiteDataType::Credentials:
        return "Credentials"_s;
    case WebsiteDataType::ServiceWorkerRegistrations:
        return "ServiceWorkerRegistrations"_s;
    case WebsiteDataType::DOMCache:
        return "DOMCache"_s;
    case WebsiteDataType::DeviceIdHashSalt:
        return "DeviceIdHashSalt"_s;
    case WebsiteDataType::PrivateClickMeasurements:
        return "PrivateClickMeasurements"_s;
#if HAVE(ALTERNATIVE_SERVICE)
    case WebsiteDataType::AlternativeServices:
        return "AlternativeServices"_s;
#endif
    case WebsiteDataType::FileSystem:
        return "FileSystem"_s;
    case WebsiteDataType::BackgroundFetchStorage:
        return "BackgroundFetchStorage"_s;
#if ENABLE(SCREEN_TIME)
    case WebsiteDataType::ScreenTime:
        return "ScreenTime"_s;
#endif
    case WebsiteDataType::EnhancedSecurityRecord:
        return "EnhancedSecurityRecord"_s;
    default:
        break;
    }
    return "Unknown"_s;
}

} // namespace WebKit

namespace WTF {

template<> struct EnumTraitsForPersistence<WebKit::WebsiteDataType> {
    using values = EnumValues<
        WebKit::WebsiteDataType,
        WebKit::WebsiteDataType::Cookies,
        WebKit::WebsiteDataType::DiskCache,
        WebKit::WebsiteDataType::MemoryCache,
        WebKit::WebsiteDataType::LegacyOfflineWebApplicationCachePlaceholder,
        WebKit::WebsiteDataType::SessionStorage,
        WebKit::WebsiteDataType::LocalStorage,
        WebKit::WebsiteDataType::WebSQLDatabases,
        WebKit::WebsiteDataType::IndexedDBDatabases,
        WebKit::WebsiteDataType::MediaKeys,
        WebKit::WebsiteDataType::HSTSCache,
        WebKit::WebsiteDataType::SearchFieldRecentSearches,
        WebKit::WebsiteDataType::ResourceLoadStatistics,
        WebKit::WebsiteDataType::Credentials,
        WebKit::WebsiteDataType::ServiceWorkerRegistrations,
        WebKit::WebsiteDataType::DOMCache,
        WebKit::WebsiteDataType::DeviceIdHashSalt,
        WebKit::WebsiteDataType::PrivateClickMeasurements,
#if HAVE(ALTERNATIVE_SERVICE)
        WebKit::WebsiteDataType::AlternativeServices,
#endif
        WebKit::WebsiteDataType::FileSystem,
        WebKit::WebsiteDataType::BackgroundFetchStorage,
#if ENABLE(SCREEN_TIME)
        WebKit::WebsiteDataType::ScreenTime,
#endif
        WebKit::WebsiteDataType::EnhancedSecurityRecord
    >;
};

} // namespace WTF
