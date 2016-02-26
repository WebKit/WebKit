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

#import "WKWebsiteDataRecordPrivate.h"

#if WK_API_ENABLED

#import "APIWebsiteDataRecord.h"
#import "WKObject.h"
#import <wtf/OptionSet.h>

namespace WebKit {

inline WKWebsiteDataRecord *wrapper(API::WebsiteDataRecord& websiteDataRecord)
{
    ASSERT([websiteDataRecord.wrapper() isKindOfClass:[WKWebsiteDataRecord class]]);
    return (WKWebsiteDataRecord *)websiteDataRecord.wrapper();
}

static inline OptionSet<WebKit::WebsiteDataType> toWebsiteDataTypes(NSSet *wkWebsiteDataTypes)
{
    using WebsiteDataType = WebKit::WebsiteDataType;

    OptionSet<WebKit::WebsiteDataType> websiteDataTypes;

    if ([wkWebsiteDataTypes containsObject:WKWebsiteDataTypeCookies])
        websiteDataTypes |= WebsiteDataType::Cookies;
    if ([wkWebsiteDataTypes containsObject:WKWebsiteDataTypeDiskCache])
        websiteDataTypes |= WebsiteDataType::DiskCache;
    if ([wkWebsiteDataTypes containsObject:WKWebsiteDataTypeMemoryCache])
        websiteDataTypes |= WebsiteDataType::MemoryCache;
    if ([wkWebsiteDataTypes containsObject:WKWebsiteDataTypeOfflineWebApplicationCache])
        websiteDataTypes |= WebsiteDataType::OfflineWebApplicationCache;
    if ([wkWebsiteDataTypes containsObject:WKWebsiteDataTypeSessionStorage])
        websiteDataTypes |= WebsiteDataType::SessionStorage;
    if ([wkWebsiteDataTypes containsObject:WKWebsiteDataTypeLocalStorage])
        websiteDataTypes |= WebsiteDataType::LocalStorage;
    if ([wkWebsiteDataTypes containsObject:WKWebsiteDataTypeWebSQLDatabases])
        websiteDataTypes |= WebsiteDataType::WebSQLDatabases;
    if ([wkWebsiteDataTypes containsObject:WKWebsiteDataTypeIndexedDBDatabases])
        websiteDataTypes |= WebsiteDataType::IndexedDBDatabases;
    if ([wkWebsiteDataTypes containsObject:_WKWebsiteDataTypeHSTSCache])
        websiteDataTypes |= WebsiteDataType::HSTSCache;
    if ([wkWebsiteDataTypes containsObject:_WKWebsiteDataTypeMediaKeys])
        websiteDataTypes |= WebsiteDataType::MediaKeys;
    if ([wkWebsiteDataTypes containsObject:_WKWebsiteDataTypeSearchFieldRecentSearches])
        websiteDataTypes |= WebsiteDataType::SearchFieldRecentSearches;
#if ENABLE(NETSCAPE_PLUGIN_API)
    if ([wkWebsiteDataTypes containsObject:_WKWebsiteDataTypePlugInData])
        websiteDataTypes |= WebsiteDataType::PlugInData;
#endif

    return websiteDataTypes;
}

static inline RetainPtr<NSSet> toWKWebsiteDataTypes(OptionSet<WebKit::WebsiteDataType> websiteDataTypes)
{
//    using WebsiteDataTypes = WebKit::WebsiteDataType;

    auto wkWebsiteDataTypes = adoptNS([[NSMutableSet alloc] init]);

    if (websiteDataTypes.contains(WebsiteDataType::Cookies))
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeCookies];
    if (websiteDataTypes.contains(WebsiteDataType::DiskCache))
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeDiskCache];
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
    if (websiteDataTypes.contains(WebsiteDataType::HSTSCache))
        [wkWebsiteDataTypes addObject:_WKWebsiteDataTypeHSTSCache];
    if (websiteDataTypes.contains(WebsiteDataType::MediaKeys))
        [wkWebsiteDataTypes addObject:_WKWebsiteDataTypeMediaKeys];
    if (websiteDataTypes.contains(WebsiteDataType::SearchFieldRecentSearches))
        [wkWebsiteDataTypes addObject:_WKWebsiteDataTypeSearchFieldRecentSearches];
#if ENABLE(NETSCAPE_PLUGIN_API)
    if (websiteDataTypes.contains(WebsiteDataType::PlugInData))
        [wkWebsiteDataTypes addObject:_WKWebsiteDataTypePlugInData];
#endif

    return wkWebsiteDataTypes;
}

}

@interface WKWebsiteDataRecord () <WKObject> {
@package
    API::ObjectStorage<API::WebsiteDataRecord> _websiteDataRecord;
}
@end

#endif
