/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKWebsiteDataRecordInternal.h"

#import "_WKWebsiteDataSizeInternal.h"
#import <WebCore/SecurityOriginData.h>
#import <wtf/HashSet.h>

NSString * const WKWebsiteDataTypeFetchCache = @"WKWebsiteDataTypeFetchCache";
NSString * const WKWebsiteDataTypeDiskCache = @"WKWebsiteDataTypeDiskCache";
NSString * const WKWebsiteDataTypeMemoryCache = @"WKWebsiteDataTypeMemoryCache";
NSString * const WKWebsiteDataTypeOfflineWebApplicationCache = @"WKWebsiteDataTypeOfflineWebApplicationCache";

NSString * const WKWebsiteDataTypeCookies = @"WKWebsiteDataTypeCookies";
NSString * const WKWebsiteDataTypeSessionStorage = @"WKWebsiteDataTypeSessionStorage";

NSString * const WKWebsiteDataTypeLocalStorage = @"WKWebsiteDataTypeLocalStorage";
NSString * const WKWebsiteDataTypeWebSQLDatabases = @"WKWebsiteDataTypeWebSQLDatabases";
NSString * const WKWebsiteDataTypeIndexedDBDatabases = @"WKWebsiteDataTypeIndexedDBDatabases";
NSString * const WKWebsiteDataTypeServiceWorkerRegistrations = @"WKWebsiteDataTypeServiceWorkerRegistrations";

NSString * const _WKWebsiteDataTypeMediaKeys = @"_WKWebsiteDataTypeMediaKeys";
NSString * const _WKWebsiteDataTypeHSTSCache = @"_WKWebsiteDataTypeHSTSCache";
NSString * const _WKWebsiteDataTypeSearchFieldRecentSearches = @"_WKWebsiteDataTypeSearchFieldRecentSearches";
NSString * const _WKWebsiteDataTypeResourceLoadStatistics = @"_WKWebsiteDataTypeResourceLoadStatistics";
NSString * const _WKWebsiteDataTypeCredentials = @"_WKWebsiteDataTypeCredentials";

#if PLATFORM(MAC)
NSString * const _WKWebsiteDataTypePlugInData = @"_WKWebsiteDataTypePlugInData";
#endif

@implementation WKWebsiteDataRecord

- (void)dealloc
{
    _websiteDataRecord->API::WebsiteDataRecord::~WebsiteDataRecord();

    [super dealloc];
}

static NSString *dataTypesToString(NSSet *dataTypes)
{
    auto array = adoptNS([[NSMutableArray alloc] init]);

    if ([dataTypes containsObject:WKWebsiteDataTypeDiskCache])
        [array addObject:@"Disk Cache"];
    if ([dataTypes containsObject:WKWebsiteDataTypeFetchCache])
        [array addObject:@"Fetch Cache"];
    if ([dataTypes containsObject:WKWebsiteDataTypeMemoryCache])
        [array addObject:@"Memory Cache"];
    if ([dataTypes containsObject:WKWebsiteDataTypeOfflineWebApplicationCache])
        [array addObject:@"Offline Web Application Cache"];
    if ([dataTypes containsObject:WKWebsiteDataTypeCookies])
        [array addObject:@"Cookies"];
    if ([dataTypes containsObject:WKWebsiteDataTypeSessionStorage])
        [array addObject:@"Session Storage"];
    if ([dataTypes containsObject:WKWebsiteDataTypeLocalStorage])
        [array addObject:@"Local Storage"];
    if ([dataTypes containsObject:WKWebsiteDataTypeWebSQLDatabases])
        [array addObject:@"Web SQL"];
    if ([dataTypes containsObject:WKWebsiteDataTypeIndexedDBDatabases])
        [array addObject:@"IndexedDB"];
    if ([dataTypes containsObject:WKWebsiteDataTypeServiceWorkerRegistrations])
        [array addObject:@"Service Worker Registrations"];
    if ([dataTypes containsObject:_WKWebsiteDataTypeHSTSCache])
        [array addObject:@"HSTS Cache"];
    if ([dataTypes containsObject:_WKWebsiteDataTypeMediaKeys])
        [array addObject:@"Media Keys"];
    if ([dataTypes containsObject:_WKWebsiteDataTypeSearchFieldRecentSearches])
        [array addObject:@"Search Field Recent Searches"];
#if PLATFORM(MAC)
    if ([dataTypes containsObject:_WKWebsiteDataTypePlugInData])
        [array addObject:@"Plug-in Data"];
#endif
    if ([dataTypes containsObject:_WKWebsiteDataTypeResourceLoadStatistics])
        [array addObject:@"Resource Load Statistics"];
    if ([dataTypes containsObject:_WKWebsiteDataTypeCredentials])
        [array addObject:@"Credentials"];

    return [array componentsJoinedByString:@", "];
}

- (NSString *)description
{
    auto result = adoptNS([[NSMutableString alloc] initWithFormat:@"<%@: %p; displayName = %@; dataTypes = { %@ }", NSStringFromClass(self.class), self, self.displayName, dataTypesToString(self.dataTypes)]);

    if (auto* dataSize = self._dataSize)
        [result appendFormat:@"; _dataSize = { %llu bytes }", dataSize.totalSize];

    [result appendString:@">"];
    return result.autorelease();
}

- (NSString *)displayName
{
    return _websiteDataRecord->websiteDataRecord().displayName;
}

- (NSSet *)dataTypes
{
    return WebKit::toWKWebsiteDataTypes(_websiteDataRecord->websiteDataRecord().types).autorelease();
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_websiteDataRecord;
}

@end

@implementation WKWebsiteDataRecord (WKPrivate)

- (_WKWebsiteDataSize *)_dataSize
{
    auto& size = _websiteDataRecord->websiteDataRecord().size;

    if (!size)
        return nil;

    return [[[_WKWebsiteDataSize alloc] initWithSize:*size] autorelease];
}

- (NSArray<NSString *> *)_originsStrings
{
    auto origins = _websiteDataRecord->websiteDataRecord().origins;
    NSMutableArray<NSString *> *array = [[NSMutableArray alloc] initWithCapacity:origins.size()];
    for (auto& origin : origins)
        [array addObject:(NSString *)origin.toString()];
    return [array autorelease];
}

@end
