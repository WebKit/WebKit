/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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

#import "WKWebsiteDataRecordPrivate.h"

#import "APIWebsiteDataRecord.h"
#import "WKObject.h"
#import <wtf/OptionSet.h>

namespace WebKit {

template<> struct WrapperTraits<API::WebsiteDataRecord> {
    using WrapperClass = WKWebsiteDataRecord;
};

static inline std::optional<WebsiteDataType> toWebsiteDataType(NSString *websiteDataType)
{
    if ([websiteDataType isEqualToString:WKWebsiteDataTypeCookies])
        return WebsiteDataType::Cookies;
    if ([websiteDataType isEqualToString:WKWebsiteDataTypeFetchCache])
        return WebsiteDataType::DOMCache;
    if ([websiteDataType isEqualToString:WKWebsiteDataTypeDiskCache])
        return WebsiteDataType::DiskCache;
    if ([websiteDataType isEqualToString:WKWebsiteDataTypeMemoryCache])
        return WebsiteDataType::MemoryCache;
    if ([websiteDataType isEqualToString:WKWebsiteDataTypeOfflineWebApplicationCache])
        return WebsiteDataType::OfflineWebApplicationCache;
    if ([websiteDataType isEqualToString:WKWebsiteDataTypeSessionStorage])
        return WebsiteDataType::SessionStorage;
    if ([websiteDataType isEqualToString:WKWebsiteDataTypeLocalStorage])
        return WebsiteDataType::LocalStorage;
    if ([websiteDataType isEqualToString:WKWebsiteDataTypeWebSQLDatabases])
        return WebsiteDataType::WebSQLDatabases;
    if ([websiteDataType isEqualToString:WKWebsiteDataTypeIndexedDBDatabases])
        return WebsiteDataType::IndexedDBDatabases;
#if ENABLE(SERVICE_WORKER)
    if ([websiteDataType isEqualToString:WKWebsiteDataTypeServiceWorkerRegistrations])
        return WebsiteDataType::ServiceWorkerRegistrations;
#endif
    if ([websiteDataType isEqualToString:WKWebsiteDataTypeFileSystem])
        return WebsiteDataType::FileSystem;
    if ([websiteDataType isEqualToString:_WKWebsiteDataTypeHSTSCache])
        return WebsiteDataType::HSTSCache;
    if ([websiteDataType isEqualToString:_WKWebsiteDataTypeMediaKeys])
        return WebsiteDataType::MediaKeys;
    if ([websiteDataType isEqualToString:_WKWebsiteDataTypeSearchFieldRecentSearches])
        return WebsiteDataType::SearchFieldRecentSearches;
    if ([websiteDataType isEqualToString:_WKWebsiteDataTypeResourceLoadStatistics])
        return WebsiteDataType::ResourceLoadStatistics;
    if ([websiteDataType isEqualToString:_WKWebsiteDataTypeCredentials])
        return WebsiteDataType::Credentials;
    if ([websiteDataType isEqualToString:_WKWebsiteDataTypeAdClickAttributions])
        return WebsiteDataType::PrivateClickMeasurements;
    if ([websiteDataType isEqualToString:_WKWebsiteDataTypePrivateClickMeasurements])
        return WebsiteDataType::PrivateClickMeasurements;
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    if ([websiteDataType isEqualToString:_WKWebsiteDataTypeAlternativeServices])
        return WebsiteDataType::AlternativeServices;
#endif
    return std::nullopt;
}

static inline OptionSet<WebKit::WebsiteDataType> toWebsiteDataTypes(NSSet *websiteDataTypes)
{
    OptionSet<WebKit::WebsiteDataType> result;

    for (NSString *websiteDataType in websiteDataTypes) {
        if (auto dataType = toWebsiteDataType(websiteDataType))
            result.add(*dataType);
    }

    return result;
}

static inline RetainPtr<NSSet> toWKWebsiteDataTypes(OptionSet<WebKit::WebsiteDataType> websiteDataTypes)
{
    auto wkWebsiteDataTypes = adoptNS([[NSMutableSet alloc] init]);

    if (websiteDataTypes.contains(WebsiteDataType::Cookies))
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeCookies];
    if (websiteDataTypes.contains(WebsiteDataType::DiskCache))
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeDiskCache];
    if (websiteDataTypes.contains(WebsiteDataType::DOMCache))
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeFetchCache];
    if (websiteDataTypes.contains(WebsiteDataType::MemoryCache))
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeMemoryCache];
    if (websiteDataTypes.contains(WebsiteDataType::OfflineWebApplicationCache))
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeOfflineWebApplicationCache];
    if (websiteDataTypes.contains(WebsiteDataType::SessionStorage))
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeSessionStorage];
    if (websiteDataTypes.contains(WebsiteDataType::LocalStorage))
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeLocalStorage];
    if (websiteDataTypes.contains(WebsiteDataType::WebSQLDatabases))
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeWebSQLDatabases];
    if (websiteDataTypes.contains(WebsiteDataType::IndexedDBDatabases))
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeIndexedDBDatabases];
#if ENABLE(SERVICE_WORKER)
    if (websiteDataTypes.contains(WebsiteDataType::ServiceWorkerRegistrations))
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeServiceWorkerRegistrations];
#endif
    if (websiteDataTypes.contains(WebsiteDataType::FileSystem))
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeFileSystem];
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache))
        [wkWebsiteDataTypes addObject:_WKWebsiteDataTypeHSTSCache];
    if (websiteDataTypes.contains(WebsiteDataType::MediaKeys))
        [wkWebsiteDataTypes addObject:_WKWebsiteDataTypeMediaKeys];
    if (websiteDataTypes.contains(WebsiteDataType::SearchFieldRecentSearches))
        [wkWebsiteDataTypes addObject:_WKWebsiteDataTypeSearchFieldRecentSearches];
    if (websiteDataTypes.contains(WebsiteDataType::ResourceLoadStatistics))
        [wkWebsiteDataTypes addObject:_WKWebsiteDataTypeResourceLoadStatistics];
    if (websiteDataTypes.contains(WebsiteDataType::Credentials))
        [wkWebsiteDataTypes addObject:_WKWebsiteDataTypeCredentials];
    if (websiteDataTypes.contains(WebsiteDataType::PrivateClickMeasurements))
        [wkWebsiteDataTypes addObject:_WKWebsiteDataTypePrivateClickMeasurements];
#if HAVE(CFNETWORK_ALTERNATIVE_SERVICE)
    if (websiteDataTypes.contains(WebsiteDataType::AlternativeServices))
        [wkWebsiteDataTypes addObject:_WKWebsiteDataTypeAlternativeServices];
#endif

    return wkWebsiteDataTypes;
}

}

@interface WKWebsiteDataRecord () <WKObject> {
@package
    API::ObjectStorage<API::WebsiteDataRecord> _websiteDataRecord;
}
@end
