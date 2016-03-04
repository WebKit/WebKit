/*
 * Copyright (C) 2014, 2016 Apple Inc. All rights reserved.
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
#import "WKWebsiteDataStoreInternal.h"

#if WK_API_ENABLED

#import "WKNSArray.h"
#import "WKWebsiteDataRecordInternal.h"
#import "WebsiteDataFetchOption.h"
#import <wtf/BlockPtr.h>

@implementation WKWebsiteDataStore

+ (WKWebsiteDataStore *)defaultDataStore
{
    return WebKit::wrapper(*API::WebsiteDataStore::defaultDataStore().get());
}

+ (WKWebsiteDataStore *)nonPersistentDataStore
{
    return [WebKit::wrapper(API::WebsiteDataStore::createNonPersistentDataStore().leakRef()) autorelease];
}

- (void)dealloc
{
    _websiteDataStore->API::WebsiteDataStore::~WebsiteDataStore();

    [super dealloc];
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (!(self = [super init]))
        return nil;

    RetainPtr<WKWebsiteDataStore> dataStore;
    if ([coder decodeBoolForKey:@"isDefaultDataStore"])
        dataStore = [WKWebsiteDataStore defaultDataStore];
    else
        dataStore = [WKWebsiteDataStore nonPersistentDataStore];

    [self release];

    return dataStore.leakRef();
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    if (self == [WKWebsiteDataStore defaultDataStore]) {
        [coder encodeBool:YES forKey:@"isDefaultDataStore"];
        return;
    }

    ASSERT(!self.persistent);
}

- (BOOL)isPersistent
{
    return _websiteDataStore->isPersistent();
}

+ (NSSet *)allWebsiteDataTypes
{
    static dispatch_once_t onceToken;
    static NSSet *allWebsiteDataTypes;
    dispatch_once(&onceToken, ^{
        allWebsiteDataTypes = [[NSSet alloc] initWithArray:@[ WKWebsiteDataTypeDiskCache, WKWebsiteDataTypeMemoryCache, WKWebsiteDataTypeOfflineWebApplicationCache, WKWebsiteDataTypeCookies, WKWebsiteDataTypeSessionStorage, WKWebsiteDataTypeLocalStorage, WKWebsiteDataTypeIndexedDBDatabases, WKWebsiteDataTypeWebSQLDatabases ]];
    });

    return allWebsiteDataTypes;
}

static std::chrono::system_clock::time_point toSystemClockTime(NSDate *date)
{
    ASSERT(date);
    using namespace std::chrono;

    return system_clock::time_point(duration_cast<system_clock::duration>(duration<double>(date.timeIntervalSince1970)));
}

- (void)fetchDataRecordsOfTypes:(NSSet *)dataTypes completionHandler:(void (^)(WK_ARRAY(WKWebsiteDataRecord *) *))completionHandler
{
    [self _fetchDataRecordsOfTypes:dataTypes withOptions:0 completionHandler:completionHandler];
}

- (void)removeDataOfTypes:(NSSet *)dataTypes modifiedSince:(NSDate *)date completionHandler:(void (^)(void))completionHandler
{
    auto completionHandlerCopy = makeBlockPtr(completionHandler);
    _websiteDataStore->websiteDataStore().removeData(WebKit::toWebsiteDataTypes(dataTypes), toSystemClockTime(date ? date : [NSDate distantPast]), [completionHandlerCopy] {
        completionHandlerCopy();
    });
}

static Vector<WebKit::WebsiteDataRecord> toWebsiteDataRecords(NSArray *dataRecords)
{
    Vector<WebKit::WebsiteDataRecord> result;

    for (WKWebsiteDataRecord *dataRecord in dataRecords)
        result.append(dataRecord->_websiteDataRecord->websiteDataRecord());

    return result;
}

- (void)removeDataOfTypes:(NSSet *)dataTypes forDataRecords:(NSArray *)dataRecords completionHandler:(void (^)(void))completionHandler
{
    auto completionHandlerCopy = makeBlockPtr(completionHandler);

    _websiteDataStore->websiteDataStore().removeData(WebKit::toWebsiteDataTypes(dataTypes), toWebsiteDataRecords(dataRecords), [completionHandlerCopy] {
        completionHandlerCopy();
    });
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_websiteDataStore;
}

@end

@implementation WKWebsiteDataStore (WKPrivate)

- (void)_fetchDataRecordsOfTypes:(WK_SET(NSString *) *)dataTypes withOptions:(_WKWebsiteDataStoreFetchOptions)options completionHandler:(void (^)(WK_ARRAY(WKWebsiteDataRecord *) *))completionHandler
{
    auto completionHandlerCopy = makeBlockPtr(completionHandler);

    OptionSet<WebKit::WebsiteDataFetchOption> fetchOptions;
    if (options & _WKWebsiteDataStoreFetchOptionComputeSizes)
        fetchOptions |= WebKit::WebsiteDataFetchOption::ComputeSizes;

    _websiteDataStore->websiteDataStore().fetchData(WebKit::toWebsiteDataTypes(dataTypes), fetchOptions, [completionHandlerCopy](Vector<WebKit::WebsiteDataRecord> websiteDataRecords) {
        Vector<RefPtr<API::Object>> elements;
        elements.reserveInitialCapacity(websiteDataRecords.size());

        for (auto& websiteDataRecord : websiteDataRecords)
            elements.uncheckedAppend(API::WebsiteDataRecord::create(WTFMove(websiteDataRecord)));

        completionHandlerCopy(wrapper(API::Array::create(WTFMove(elements))));
    });
}

- (BOOL)_resourceLoadStatisticsEnabled
{
    return _websiteDataStore->websiteDataStore().resourceLoadStatisticsEnabled();
}

- (void)_setResourceLoadStatisticsEnabled:(BOOL)enabled
{
    _websiteDataStore->websiteDataStore().setResourceLoadStatisticsEnabled(enabled);
}

@end

#endif // WK_API_ENABLED
