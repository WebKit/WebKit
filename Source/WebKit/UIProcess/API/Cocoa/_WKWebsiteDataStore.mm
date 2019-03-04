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

#import "config.h"
#import "_WKWebsiteDataStoreInternal.h"

#import <wtf/RetainPtr.h>

typedef NS_OPTIONS(NSUInteger, _WKWebsiteDataTypes) {
    _WKWebsiteDataTypeCookies = 1 << 0,
    _WKWebsiteDataTypeDiskCache = 1 << 1,
    _WKWebsiteDataTypeMemoryCache = 1 << 2,
    _WKWebsiteDataTypeOfflineWebApplicationCache = 1 << 3,

    _WKWebsiteDataTypeLocalStorage = 1 << 4,
    _WKWebsiteDataTypeWebSQLDatabases = 1 << 5,
};

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
@implementation _WKWebsiteDataStore
IGNORE_WARNINGS_END

- (instancetype)initWithDataStore:(WKWebsiteDataStore *)dataStore
{
    if (!(self = [super init]))
        return nil;

    _dataStore = dataStore;

    return self;
}

static RetainPtr<NSSet> toWKWebsiteDataTypes(_WKWebsiteDataTypes websiteDataTypes)
{
    auto wkWebsiteDataTypes = adoptNS([[NSMutableSet alloc] init]);

    if (websiteDataTypes & _WKWebsiteDataTypeCookies)
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeCookies];
    if (websiteDataTypes & _WKWebsiteDataTypeDiskCache)
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeDiskCache];
    if (websiteDataTypes & _WKWebsiteDataTypeMemoryCache)
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeMemoryCache];
    if (websiteDataTypes & _WKWebsiteDataTypeOfflineWebApplicationCache)
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeOfflineWebApplicationCache];
    if (websiteDataTypes & _WKWebsiteDataTypeLocalStorage)
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeLocalStorage];
    if (websiteDataTypes & _WKWebsiteDataTypeWebSQLDatabases)
        [wkWebsiteDataTypes addObject:WKWebsiteDataTypeWebSQLDatabases];

    return wkWebsiteDataTypes;
}

+ (_WKWebsiteDataStore *)defaultDataStore
{
    return adoptNS([[_WKWebsiteDataStore alloc] initWithDataStore:[WKWebsiteDataStore defaultDataStore]]).autorelease();
}

+ (_WKWebsiteDataStore *)nonPersistentDataStore
{
    return adoptNS([[_WKWebsiteDataStore alloc] initWithDataStore:[WKWebsiteDataStore nonPersistentDataStore]]).autorelease();
}

- (BOOL)isNonPersistent
{
    return ![_dataStore isPersistent];
}

- (void)fetchDataRecordsOfTypes:(WKWebsiteDataTypes)websiteDataTypes completionHandler:(void (^)(NSArray *))completionHandler
{
    [_dataStore fetchDataRecordsOfTypes:toWKWebsiteDataTypes(websiteDataTypes).get() completionHandler:completionHandler];
}

- (void)removeDataOfTypes:(WKWebsiteDataTypes)websiteDataTypes forDataRecords:(NSArray *)dataRecords completionHandler:(void (^)(void))completionHandler
{
    [_dataStore removeDataOfTypes:toWKWebsiteDataTypes(websiteDataTypes).get() forDataRecords:dataRecords completionHandler:completionHandler];
}

- (void)removeDataOfTypes:(WKWebsiteDataTypes)websiteDataTypes modifiedSince:(NSDate *)date completionHandler:(void (^)(void))completionHandler
{
    [_dataStore removeDataOfTypes:toWKWebsiteDataTypes(websiteDataTypes).get() modifiedSince:date completionHandler:completionHandler];
}

@end

ALLOW_DEPRECATED_DECLARATIONS_END
