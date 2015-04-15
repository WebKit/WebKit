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

#import "WKWebsiteDataRecord.h"

#if WK_API_ENABLED

#import "APIWebsiteDataRecord.h"
#import "WKObject.h"

namespace WebKit {

inline WKWebsiteDataRecord *wrapper(API::WebsiteDataRecord& websiteDataRecord)
{
    ASSERT([websiteDataRecord.wrapper() isKindOfClass:[WKWebsiteDataRecord class]]);
    return (WKWebsiteDataRecord *)websiteDataRecord.wrapper();
}

static inline WebKit::WebsiteDataTypes toWebsiteDataTypes(WKWebsiteDataTypes wkWebsiteDataTypes)
{
    using WebsiteDataTypes = WebKit::WebsiteDataTypes;

    int websiteDataTypes = 0;

    if (wkWebsiteDataTypes & WKWebsiteDataTypeCookies)
        websiteDataTypes |= WebsiteDataTypes::WebsiteDataTypeCookies;
    if (wkWebsiteDataTypes & WKWebsiteDataTypeDiskCache)
        websiteDataTypes |= WebsiteDataTypes::WebsiteDataTypeDiskCache;
    if (wkWebsiteDataTypes & WKWebsiteDataTypeMemoryCache)
        websiteDataTypes |= WebsiteDataTypes::WebsiteDataTypeMemoryCache;
    if (wkWebsiteDataTypes & WKWebsiteDataTypeOfflineWebApplicationCache)
        websiteDataTypes |= WebsiteDataTypes::WebsiteDataTypeOfflineWebApplicationCache;
    if (wkWebsiteDataTypes & WKWebsiteDataTypeLocalStorage)
        websiteDataTypes |= WebsiteDataTypes::WebsiteDataTypeLocalStorage;
    if (wkWebsiteDataTypes & WKWebsiteDataTypeWebSQLDatabases)
        websiteDataTypes |= WebsiteDataTypes::WebsiteDataTypeWebSQLDatabases;

    return static_cast<WebsiteDataTypes>(websiteDataTypes);
}

static inline WKWebsiteDataTypes toWKWebsiteDataTypes(int websiteDataTypes)
{
    using WebsiteDataTypes = WebKit::WebsiteDataTypes;

    WKWebsiteDataTypes wkWebsiteDataTypes = 0;

    if (websiteDataTypes & WebsiteDataTypes::WebsiteDataTypeCookies)
        wkWebsiteDataTypes |= WKWebsiteDataTypeCookies;
    if (websiteDataTypes & WebsiteDataTypes::WebsiteDataTypeDiskCache)
        wkWebsiteDataTypes |= WKWebsiteDataTypeDiskCache;
    if (websiteDataTypes & WebsiteDataTypes::WebsiteDataTypeMemoryCache)
        wkWebsiteDataTypes |= WKWebsiteDataTypeMemoryCache;
    if (websiteDataTypes & WebsiteDataTypes::WebsiteDataTypeOfflineWebApplicationCache)
        wkWebsiteDataTypes |= WKWebsiteDataTypeOfflineWebApplicationCache;
    if (websiteDataTypes & WebsiteDataTypes::WebsiteDataTypeLocalStorage)
        wkWebsiteDataTypes |= WKWebsiteDataTypeLocalStorage;
    if (websiteDataTypes & WebsiteDataTypes::WebsiteDataTypeWebSQLDatabases)
        wkWebsiteDataTypes |= WKWebsiteDataTypeWebSQLDatabases;

    return wkWebsiteDataTypes;
}

}

@interface WKWebsiteDataRecord () <WKObject> {
@package
    API::ObjectStorage<API::WebsiteDataRecord> _websiteDataRecord;
}
@end

#endif
