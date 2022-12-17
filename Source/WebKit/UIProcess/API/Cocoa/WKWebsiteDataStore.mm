/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#import "APIString.h"
#import "AuthenticationChallengeDispositionCocoa.h"
#import "CompletionHandlerCallChecker.h"
#import "NetworkProcessProxy.h"
#import "ShouldGrandfatherStatistics.h"
#import "WKError.h"
#import "WKHTTPCookieStoreInternal.h"
#import "WKNSArray.h"
#import "WKNSURLAuthenticationChallenge.h"
#import "WKSecurityOriginInternal.h"
#import "WKWebViewInternal.h"
#import "WKWebsiteDataRecordInternal.h"
#import "WebNotification.h"
#import "WebNotificationManagerProxy.h"
#import "WebPageProxy.h"
#import "WebPushMessage.h"
#import "WebResourceLoadStatisticsStore.h"
#import "WebsiteDataFetchOption.h"
#import "_WKNotificationDataInternal.h"
#import "_WKResourceLoadStatisticsThirdPartyInternal.h"
#import "_WKWebsiteDataStoreConfigurationInternal.h"
#import "_WKWebsiteDataStoreDelegate.h"
#import <WebCore/Credential.h>
#import <WebCore/RegistrationDatabase.h>
#import <WebCore/ServiceWorkerClientData.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/BlockPtr.h>
#import <wtf/URL.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

class WebsiteDataStoreClient final : public WebKit::WebsiteDataStoreClient {
public:
    WebsiteDataStoreClient(WKWebsiteDataStore *dataStore, id<_WKWebsiteDataStoreDelegate> delegate)
        : m_dataStore(dataStore)
        , m_delegate(delegate)
        , m_hasRequestStorageSpaceSelector([m_delegate.get() respondsToSelector:@selector(requestStorageSpace: frameOrigin: quota: currentSize: spaceRequired: decisionHandler:)])
        , m_hasAuthenticationChallengeSelector([m_delegate.get() respondsToSelector:@selector(didReceiveAuthenticationChallenge: completionHandler:)])
        , m_hasOpenWindowSelector([m_delegate.get() respondsToSelector:@selector(websiteDataStore:openWindow:fromServiceWorkerOrigin:completionHandler:)])
        , m_hasShowNotificationSelector([m_delegate.get() respondsToSelector:@selector(websiteDataStore:showNotification:)])
        , m_hasNotificationPermissionsSelector([m_delegate.get() respondsToSelector:@selector(notificationPermissionsForWebsiteDataStore:)])
        , m_hasWorkerUpdatedAppBadgeSelector([m_delegate.get() respondsToSelector:@selector(websiteDataStore:workerOrigin:updatedAppBadge:)])
    {
    }

private:
    void requestStorageSpace(const WebCore::SecurityOriginData& topOrigin, const WebCore::SecurityOriginData& frameOrigin, uint64_t quota, uint64_t currentSize, uint64_t spaceRequired, CompletionHandler<void(std::optional<uint64_t>)>&& completionHandler) final
    {
        if (!m_hasRequestStorageSpaceSelector || !m_delegate) {
            completionHandler({ });
            return;
        }

        auto checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.getAutoreleased(), @selector(requestStorageSpace: frameOrigin: quota: currentSize: spaceRequired: decisionHandler:));
        auto decisionHandler = makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](unsigned long long quota) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(quota);
        });

        URL mainFrameURL { topOrigin.toString() };
        URL frameURL { frameOrigin.toString() };

        [m_delegate.getAutoreleased() requestStorageSpace:mainFrameURL frameOrigin:frameURL quota:quota currentSize:currentSize spaceRequired:spaceRequired decisionHandler:decisionHandler.get()];
    }

    void didReceiveAuthenticationChallenge(Ref<WebKit::AuthenticationChallengeProxy>&& challenge) final
    {
        if (!m_hasAuthenticationChallengeSelector || !m_delegate) {
            challenge->listener().completeChallenge(WebKit::AuthenticationChallengeDisposition::PerformDefaultHandling);
            return;
        }

        auto nsURLChallenge = wrapper(challenge);
        auto checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.getAutoreleased(), @selector(didReceiveAuthenticationChallenge: completionHandler:));
        auto completionHandler = makeBlockPtr([challenge = WTFMove(challenge), checker = WTFMove(checker)](NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            challenge->listener().completeChallenge(WebKit::toAuthenticationChallengeDisposition(disposition), WebCore::Credential(credential));
        });

        [m_delegate.getAutoreleased() didReceiveAuthenticationChallenge:nsURLChallenge completionHandler:completionHandler.get()];
    }

    void openWindowFromServiceWorker(const String& url, const WebCore::SecurityOriginData& serviceWorkerOrigin, CompletionHandler<void(WebKit::WebPageProxy*)>&& callback)
    {
        if (!m_hasOpenWindowSelector || !m_delegate || !m_dataStore) {
            callback({ });
            return;
        }

        auto checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.getAutoreleased(), @selector(websiteDataStore:openWindow:fromServiceWorkerOrigin:completionHandler:));
        auto completionHandler = makeBlockPtr([callback = WTFMove(callback), checker = WTFMove(checker)](WKWebView *newWebView) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();

            callback(newWebView._page.get());
        });

        auto nsURL = (NSURL *)URL { url };
        auto apiOrigin = API::SecurityOrigin::create(serviceWorkerOrigin);
        [m_delegate.getAutoreleased() websiteDataStore:m_dataStore.getAutoreleased() openWindow:nsURL fromServiceWorkerOrigin:wrapper(apiOrigin.get()) completionHandler:completionHandler.get()];
    }

    bool showNotification(const WebCore::NotificationData& data) final
    {
        if (!m_hasShowNotificationSelector || !m_delegate || !m_dataStore)
            return false;

        RetainPtr<_WKNotificationData> notification = adoptNS([[_WKNotificationData alloc] initWithCoreData:data dataStore:m_dataStore.getAutoreleased()]);
        [m_delegate.getAutoreleased() websiteDataStore:m_dataStore.getAutoreleased() showNotification:notification.get()];
        return true;
    }

    HashMap<WTF::String, bool> notificationPermissions() final
    {
        if (!m_hasNotificationPermissionsSelector || !m_delegate || !m_dataStore)
            return { };

        HashMap<WTF::String, bool> result;
        NSDictionary<NSString *, NSNumber *> *permissions = [m_delegate.getAutoreleased() notificationPermissionsForWebsiteDataStore:m_dataStore.getAutoreleased()];
        for (NSString *key in permissions) {
            NSNumber *value = permissions[key];
            auto origin = WebCore::SecurityOriginData::fromURL(URL(key));
            result.set(origin.toString(), value.boolValue);
        }

        return result;
    }

    void workerUpdatedAppBadge(const WebCore::SecurityOriginData& origin, std::optional<uint64_t> badge) final
    {
        if (!m_hasWorkerUpdatedAppBadgeSelector || !m_delegate || !m_dataStore)
            return;

        auto apiOrigin = API::SecurityOrigin::create(origin);
        NSNumber *nsBadge = nil;
        if (badge)
            nsBadge = @(*badge);

        [m_delegate.getAutoreleased() websiteDataStore:m_dataStore.getAutoreleased() workerOrigin:wrapper(apiOrigin.get()) updatedAppBadge:(NSNumber *)nsBadge];
    }

    WeakObjCPtr<WKWebsiteDataStore> m_dataStore;
    WeakObjCPtr<id <_WKWebsiteDataStoreDelegate> > m_delegate;
    bool m_hasRequestStorageSpaceSelector { false };
    bool m_hasAuthenticationChallengeSelector { false };
    bool m_hasOpenWindowSelector { false };
    bool m_hasShowNotificationSelector { false };
    bool m_hasNotificationPermissionsSelector { false };
    bool m_hasWorkerUpdatedAppBadgeSelector { false };
};

@implementation WKWebsiteDataStore

+ (WKWebsiteDataStore *)defaultDataStore
{
    return wrapper(WebKit::WebsiteDataStore::defaultDataStore());
}

+ (WKWebsiteDataStore *)nonPersistentDataStore
{
    return wrapper(WebKit::WebsiteDataStore::createNonPersistent());
}

- (instancetype)init
{
    if (linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::WKWebsiteDataStoreInitReturningNil))
        [NSException raise:NSGenericException format:@"Calling [WKWebsiteDataStore init] is not supported."];
    
    if (!(self = [super init]))
        return nil;

    RELEASE_LOG_ERROR(Storage, "Application is calling [WKWebsiteDataStore init], which is not supported");
    API::Object::constructInWrapper<WebKit::WebsiteDataStore>(self, WebKit::WebsiteDataStoreConfiguration::create(WebKit::IsPersistent::No), PAL::SessionID::generateEphemeralSessionID());

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKWebsiteDataStore.class, self))
        return;

    _websiteDataStore->WebKit::WebsiteDataStore::~WebsiteDataStore();

    [super dealloc];
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if ([coder decodeBoolForKey:@"isDefaultDataStore"])
        return [[WKWebsiteDataStore defaultDataStore] retain];
    return [[WKWebsiteDataStore nonPersistentDataStore] retain];
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
    static NeverDestroyed<RetainPtr<NSSet>> allWebsiteDataTypes;
    dispatch_once(&onceToken, ^{
        allWebsiteDataTypes.get() = adoptNS([[NSSet alloc] initWithArray:@[ WKWebsiteDataTypeDiskCache, WKWebsiteDataTypeFetchCache, WKWebsiteDataTypeMemoryCache, WKWebsiteDataTypeOfflineWebApplicationCache, WKWebsiteDataTypeCookies, WKWebsiteDataTypeSessionStorage, WKWebsiteDataTypeLocalStorage, WKWebsiteDataTypeIndexedDBDatabases, WKWebsiteDataTypeServiceWorkerRegistrations, WKWebsiteDataTypeWebSQLDatabases, WKWebsiteDataTypeFileSystem ]]);
    });

    return allWebsiteDataTypes.get().get();
}

- (WKHTTPCookieStore *)httpCookieStore
{
    return wrapper(_websiteDataStore->cookieStore());
}

static WallTime toSystemClockTime(NSDate *date)
{
    ASSERT(date);
    return WallTime::fromRawSeconds(date.timeIntervalSince1970);
}

- (void)fetchDataRecordsOfTypes:(NSSet *)dataTypes completionHandler:(void (^)(NSArray<WKWebsiteDataRecord *> *))completionHandler
{
    [self _fetchDataRecordsOfTypes:dataTypes withOptions:0 completionHandler:completionHandler];
}

- (void)removeDataOfTypes:(NSSet *)dataTypes modifiedSince:(NSDate *)date completionHandler:(void (^)(void))completionHandler
{
    auto completionHandlerCopy = makeBlockPtr(completionHandler);
    _websiteDataStore->removeData(WebKit::toWebsiteDataTypes(dataTypes), toSystemClockTime(date ? date : [NSDate distantPast]), [completionHandlerCopy] {
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

    _websiteDataStore->removeData(WebKit::toWebsiteDataTypes(dataTypes), toWebsiteDataRecords(dataRecords), [completionHandlerCopy] {
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

+ (NSSet<NSString *> *)_allWebsiteDataTypesIncludingPrivate
{
    static dispatch_once_t onceToken;
    static NeverDestroyed<RetainPtr<NSSet>> allWebsiteDataTypes;
    dispatch_once(&onceToken, ^ {
        auto *privateTypes = @[
            _WKWebsiteDataTypeHSTSCache,
            _WKWebsiteDataTypeMediaKeys,
            _WKWebsiteDataTypeSearchFieldRecentSearches,
            _WKWebsiteDataTypeResourceLoadStatistics,
            _WKWebsiteDataTypeCredentials,
            _WKWebsiteDataTypeAdClickAttributions,
            _WKWebsiteDataTypePrivateClickMeasurements,
            _WKWebsiteDataTypeAlternativeServices
#if !TARGET_OS_IPHONE
            , _WKWebsiteDataTypePlugInData
#endif
        ];

        allWebsiteDataTypes.get() = [[self allWebsiteDataTypes] setByAddingObjectsFromArray:privateTypes];
    });

    return allWebsiteDataTypes.get().get();
}

+ (BOOL)_defaultDataStoreExists
{
    return WebKit::WebsiteDataStore::defaultDataStoreExists();
}

+ (void)_deleteDefaultDataStoreForTesting
{
    return WebKit::WebsiteDataStore::deleteDefaultDataStoreForTesting();
}

+ (void)_fetchAllIdentifiers:(void(^)(NSArray<NSUUID *> *))completionHandler
{
    auto completionHandlerCopy = makeBlockPtr(completionHandler);
    WebKit::WebsiteDataStore::fetchAllDataStoreIdentifiers([completionHandlerCopy](auto&& identifiers) {
        auto result = adoptNS([[NSMutableArray alloc] initWithCapacity:identifiers.size()]);
        for (auto identifier : identifiers)
            [result addObject:(NSUUID *)identifier];

        completionHandlerCopy(result.autorelease());
    });
}

+ (void)_removeDataStoreWithIdentifier:(NSUUID *)identifier completionHandler:(void(^)(NSError* error))completionHandler
{
    if (!identifier)
        [NSException raise:NSInvalidArgumentException format:@"Identifier cannot be nil"];

    auto completionHandlerCopy = makeBlockPtr(completionHandler);
    WebKit::WebsiteDataStore::removeDataStoreWithIdentifier(UUID(identifier), [completionHandlerCopy](const String& errorString) {
        if (errorString.isEmpty())
            return completionHandlerCopy(nil);

        completionHandlerCopy([NSError errorWithDomain:@"WKWebSiteDataStore" code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey:(NSString *)errorString }]);
    });
}

- (instancetype)_initWithConfiguration:(_WKWebsiteDataStoreConfiguration *)configuration
{
    if (!(self = [super init]))
        return nil;

    auto sessionID = configuration.isPersistent ? PAL::SessionID::generatePersistentSessionID() : PAL::SessionID::generateEphemeralSessionID();
    API::Object::constructInWrapper<WebKit::WebsiteDataStore>(self, configuration->_configuration->copy(), sessionID);

    return self;
}

- (void)_fetchDataRecordsOfTypes:(NSSet<NSString *> *)dataTypes withOptions:(_WKWebsiteDataStoreFetchOptions)options completionHandler:(void (^)(NSArray<WKWebsiteDataRecord *> *))completionHandler
{
    auto completionHandlerCopy = makeBlockPtr(completionHandler);

    OptionSet<WebKit::WebsiteDataFetchOption> fetchOptions;
    if (options & _WKWebsiteDataStoreFetchOptionComputeSizes)
        fetchOptions.add(WebKit::WebsiteDataFetchOption::ComputeSizes);

    _websiteDataStore->fetchData(WebKit::toWebsiteDataTypes(dataTypes), fetchOptions, [completionHandlerCopy = WTFMove(completionHandlerCopy)](auto websiteDataRecords) {
        Vector<RefPtr<API::Object>> elements;
        elements.reserveInitialCapacity(websiteDataRecords.size());

        for (auto& websiteDataRecord : websiteDataRecords)
            elements.uncheckedAppend(API::WebsiteDataRecord::create(WTFMove(websiteDataRecord)));

        completionHandlerCopy(wrapper(API::Array::create(WTFMove(elements))));
    });
}

- (BOOL)_resourceLoadStatisticsEnabled
{
    return _websiteDataStore->trackingPreventionEnabled();
}

- (void)_setResourceLoadStatisticsEnabled:(BOOL)enabled
{
    _websiteDataStore->useExplicitTrackingPreventionState();
    _websiteDataStore->setTrackingPreventionEnabled(enabled);
}

- (BOOL)_resourceLoadStatisticsDebugMode
{
#if ENABLE(TRACKING_PREVENTION)
    return _websiteDataStore->resourceLoadStatisticsDebugMode();
#else
    return NO;
#endif
}

- (void)_setResourceLoadStatisticsDebugMode:(BOOL)enabled
{
#if ENABLE(TRACKING_PREVENTION)
    _websiteDataStore->setResourceLoadStatisticsDebugMode(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

- (NSUInteger)_perOriginStorageQuota
{
    return 0;
}

- (void)_setPerOriginStorageQuota:(NSUInteger)size
{
}

- (void)_setBoundInterfaceIdentifier:(NSString *)identifier
{
}

- (NSString *)_boundInterfaceIdentifier
{
    return nil;
}

- (void)_setAllowsCellularAccess:(BOOL)allows
{
}

- (BOOL)_allowsCellularAccess
{
    return YES;
}

- (void)_setProxyConfiguration:(NSDictionary *)configuration
{
}

- (void)_setAllowsTLSFallback:(BOOL)allows
{
}

- (BOOL)_allowsTLSFallback
{
    return NO;
}

- (NSDictionary *)_proxyConfiguration
{
    return nil;
}

- (void)_setResourceLoadStatisticsTestingCallback:(void (^)(WKWebsiteDataStore *, NSString *))callback
{
#if ENABLE(TRACKING_PREVENTION)
    if (!_websiteDataStore->isPersistent())
        return;

    if (callback) {
        _websiteDataStore->setStatisticsTestingCallback([callback = makeBlockPtr(callback), self](const String& event) {
            callback(self, event);
        });
        return;
    }

    _websiteDataStore->setStatisticsTestingCallback(nullptr);
#endif
}

- (void)_setResourceLoadStatisticsTimeAdvanceForTesting:(NSTimeInterval)time completionHandler:(void(^)(void))completionHandler
{
#if ENABLE(TRACKING_PREVENTION)
    _websiteDataStore->setResourceLoadStatisticsTimeAdvanceForTesting(Seconds(time), makeBlockPtr(completionHandler));
#endif
}

+ (void)_allowWebsiteDataRecordsForAllOrigins
{
    WebKit::WebsiteDataStore::allowWebsiteDataRecordsForAllOrigins();
}

- (void)_loadedSubresourceDomainsFor:(WKWebView *)webView completionHandler:(void (^)(NSArray<NSString *> *domains))completionHandler
{
#if ENABLE(TRACKING_PREVENTION)
    if (!webView) {
        completionHandler(nil);
        return;
    }

    auto webPageProxy = [webView _page];
    if (!webPageProxy) {
        completionHandler(nil);
        return;
    }
    
    webPageProxy->getLoadedSubresourceDomains([completionHandler = makeBlockPtr(completionHandler)] (Vector<WebCore::RegistrableDomain>&& loadedSubresourceDomains) {
        Vector<RefPtr<API::Object>> apiDomains = WTF::map(loadedSubresourceDomains, [](auto& domain) {
            return RefPtr<API::Object>(API::String::create(String(domain.string())));
        });
        completionHandler(wrapper(API::Array::create(WTFMove(apiDomains))));
    });
#else
    completionHandler(nil);
#endif
}

- (void)_clearLoadedSubresourceDomainsFor:(WKWebView *)webView
{
#if ENABLE(TRACKING_PREVENTION)
    if (!webView)
        return;

    auto webPageProxy = [webView _page];
    if (!webPageProxy)
        return;

    webPageProxy->clearLoadedSubresourceDomains();
#endif
}


- (void)_getAllStorageAccessEntriesFor:(WKWebView *)webView completionHandler:(void (^)(NSArray<NSString *> *domains))completionHandler
{
    if (!webView) {
        completionHandler({ });
        return;
    }

    auto webPageProxy = [webView _page];
    if (!webPageProxy) {
        completionHandler({ });
        return;
    }

#if ENABLE(TRACKING_PREVENTION)
    _websiteDataStore->getAllStorageAccessEntries(webPageProxy->identifier(), [completionHandler = makeBlockPtr(completionHandler)](auto domains) {
        Vector<RefPtr<API::Object>> apiDomains;
        apiDomains.reserveInitialCapacity(domains.size());
        for (auto& domain : domains)
            apiDomains.uncheckedAppend(API::String::create(domain));
        completionHandler(wrapper(API::Array::create(WTFMove(apiDomains))));
    });
#else
    completionHandler({ });
#endif
}

- (void)_scheduleCookieBlockingUpdate:(void (^)(void))completionHandler
{
#if ENABLE(TRACKING_PREVENTION)
    _websiteDataStore->scheduleCookieBlockingUpdate([completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
#else
    completionHandler();
#endif
}

- (void)_logUserInteraction:(NSURL *)domain completionHandler:(void (^)(void))completionHandler
{
#if ENABLE(TRACKING_PREVENTION)
    _websiteDataStore->logUserInteraction(domain, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
#else
    completionHandler();
#endif
}

- (void)_setPrevalentDomain:(NSURL *)domain completionHandler:(void (^)(void))completionHandler
{
#if ENABLE(TRACKING_PREVENTION)
    _websiteDataStore->setPrevalentResource(URL(domain), [completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
#else
    completionHandler();
#endif
}

- (void)_getIsPrevalentDomain:(NSURL *)domain completionHandler:(void (^)(BOOL))completionHandler
{
#if ENABLE(TRACKING_PREVENTION)
    _websiteDataStore->isPrevalentResource(URL(domain), [completionHandler = makeBlockPtr(completionHandler)](bool enabled) {
        completionHandler(enabled);
    });
#else
    completionHandler(NO);
#endif
}

- (void)_clearPrevalentDomain:(NSURL *)domain completionHandler:(void (^)(void))completionHandler
{
#if ENABLE(TRACKING_PREVENTION)
    _websiteDataStore->clearPrevalentResource(URL(domain), [completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
#else
    completionHandler();
#endif
}

- (void)_clearResourceLoadStatistics:(void (^)(void))completionHandler
{
#if ENABLE(TRACKING_PREVENTION)
    _websiteDataStore->scheduleClearInMemoryAndPersistent(WebKit::ShouldGrandfatherStatistics::No, [completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
#else
    completionHandler();
#endif
}

- (void)_closeDatabases:(void (^)(void))completionHandler
{
    _websiteDataStore->closeDatabases([completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
}

- (void)_getResourceLoadStatisticsDataSummary:(void (^)(NSArray<_WKResourceLoadStatisticsThirdParty *> *))completionHandler
{
#if ENABLE(TRACKING_PREVENTION)
    _websiteDataStore->getResourceLoadStatisticsDataSummary([completionHandler = makeBlockPtr(completionHandler)] (auto&& thirdPartyDomains) {
        completionHandler(createNSArray(thirdPartyDomains, [] (auto&& domain) {
            return wrapper(API::ResourceLoadStatisticsThirdParty::create(WTFMove(domain)));
        }).get());
    });
#else
    completionHandler(nil);
#endif
}

+ (void)_setCachedProcessSuspensionDelayForTesting:(double)delayInSeconds
{
    WebKit::WebsiteDataStore::setCachedProcessSuspensionDelayForTesting(Seconds(delayInSeconds));
}

- (void)_isRelationshipOnlyInDatabaseOnce:(NSURL *)firstPartyURL thirdParty:(NSURL *)thirdPartyURL completionHandler:(void (^)(BOOL))completionHandler
{
#if ENABLE(TRACKING_PREVENTION)
    _websiteDataStore->isRelationshipOnlyInDatabaseOnce(thirdPartyURL, firstPartyURL, [completionHandler = makeBlockPtr(completionHandler)] (bool result) {
        completionHandler(result);
    });
#else
    completionHandler(NO);
#endif
}

- (void)_isRegisteredAsSubresourceUnderFirstParty:(NSURL *)firstPartyURL thirdParty:(NSURL *)thirdPartyURL completionHandler:(void (^)(BOOL))completionHandler
{
#if ENABLE(TRACKING_PREVENTION)
    _websiteDataStore->isRegisteredAsSubresourceUnder(thirdPartyURL, firstPartyURL, [completionHandler = makeBlockPtr(completionHandler)](bool enabled) {
        completionHandler(enabled);
    });
#else
    completionHandler(NO);
#endif
}

- (void)_statisticsDatabaseHasAllTables:(void (^)(BOOL))completionHandler
{
#if ENABLE(TRACKING_PREVENTION)
    _websiteDataStore->statisticsDatabaseHasAllTables([completionHandler = makeBlockPtr(completionHandler)](bool hasAllTables) {
        completionHandler(hasAllTables);
    });
#else
    completionHandler(NO);
#endif
}

- (void)_processStatisticsAndDataRecords:(void (^)(void))completionHandler
{
#if ENABLE(TRACKING_PREVENTION)
    _websiteDataStore->scheduleStatisticsAndDataRecordsProcessing([completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
#else
    completionHandler();
#endif
}

- (void)_setThirdPartyCookieBlockingMode:(BOOL)enabled onlyOnSitesWithoutUserInteraction:(BOOL)onlyOnSitesWithoutUserInteraction completionHandler:(void (^)(void))completionHandler
{
#if ENABLE(TRACKING_PREVENTION)
    _websiteDataStore->setResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(enabled, onlyOnSitesWithoutUserInteraction, [completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
#else
    completionHandler();
#endif
}

- (bool)_hasRegisteredServiceWorker
{
#if ENABLE(SERVICE_WORKER)
    return FileSystem::fileExists(WebCore::serviceWorkerRegistrationDatabaseFilename(_websiteDataStore->configuration().serviceWorkerRegistrationDirectory()));
#else
    return NO;
#endif
}

- (void)_renameOrigin:(NSURL *)oldName to:(NSURL *)newName forDataOfTypes:(NSSet<NSString *> *)dataTypes completionHandler:(void (^)(void))completionHandler
{
    if (!dataTypes.count)
        return completionHandler();

    NSSet *supportedTypes = [NSSet setWithObjects:WKWebsiteDataTypeLocalStorage, WKWebsiteDataTypeIndexedDBDatabases, nil];
    if (![dataTypes isSubsetOfSet:supportedTypes])
        [NSException raise:NSInvalidArgumentException format:@"_renameOrigin can only be called with WKWebsiteDataTypeLocalStorage and WKWebsiteDataTypeIndexedDBDatabases right now."];

    _websiteDataStore->renameOriginInWebsiteData(oldName, newName, WebKit::toWebsiteDataTypes(dataTypes), [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (BOOL)_networkProcessHasEntitlementForTesting:(NSString *)entitlement
{
    return _websiteDataStore->networkProcessHasEntitlementForTesting(entitlement);
}

- (id <_WKWebsiteDataStoreDelegate>)_delegate
{
    return _delegate.get().get();
}

- (void)set_delegate:(id <_WKWebsiteDataStoreDelegate>)delegate
{
    _delegate = delegate;
    _websiteDataStore->setClient(makeUniqueRef<WebsiteDataStoreClient>(self, delegate));
}

- (_WKWebsiteDataStoreConfiguration *)_configuration
{
    return wrapper(_websiteDataStore->configuration().copy());
}

+ (WKNotificationManagerRef)_sharedServiceWorkerNotificationManager
{
    LOG(Push, "Accessing _sharedServiceWorkerNotificationManager");
    return WebKit::toAPI(&WebKit::WebNotificationManagerProxy::sharedServiceWorkerManager());
}

- (void)_allowTLSCertificateChain:(NSArray *)certificateChain forHost:(NSString *)host
{
    _websiteDataStore->allowSpecificHTTPSCertificateForHost(WebCore::CertificateInfo(WebCore::CertificateInfo::secTrustFromCertificateChain((__bridge CFArrayRef)certificateChain)), host);
}

- (void)_trustServerForLocalPCMTesting:(SecTrustRef)serverTrust
{
    _websiteDataStore->allowTLSCertificateChainForLocalPCMTesting(WebCore::CertificateInfo(serverTrust));
}

- (void)_setPrivateClickMeasurementDebugModeEnabledForTesting:(BOOL)enabled
{
    _websiteDataStore->setPrivateClickMeasurementDebugMode(enabled);
}

- (void)_appBoundDomains:(void (^)(NSArray<NSString *> *))completionHandler
{
#if ENABLE(APP_BOUND_DOMAINS)
    _websiteDataStore->getAppBoundDomains([completionHandler = makeBlockPtr(completionHandler)](auto& domains) mutable {
        Vector<RefPtr<API::Object>> apiDomains;
        apiDomains.reserveInitialCapacity(domains.size());
        for (auto& domain : domains)
            apiDomains.uncheckedAppend(API::String::create(domain.string()));
        completionHandler(wrapper(API::Array::create(WTFMove(apiDomains))));
    });
#else
    completionHandler({ });
#endif
}

- (void)_appBoundSchemes:(void (^)(NSArray<NSString *> *))completionHandler
{
#if ENABLE(APP_BOUND_DOMAINS)
    _websiteDataStore->getAppBoundSchemes([completionHandler = makeBlockPtr(completionHandler)](auto& schemes) mutable {
        Vector<RefPtr<API::Object>> apiSchemes;
        apiSchemes.reserveInitialCapacity(schemes.size());
        for (auto& scheme : schemes)
            apiSchemes.uncheckedAppend(API::String::create(scheme));
        completionHandler(wrapper(API::Array::create(WTFMove(apiSchemes))));
    });
#else
    completionHandler({ });
#endif
}

- (void)_terminateNetworkProcess
{
    _websiteDataStore->terminateNetworkProcess();
}

- (void)_sendNetworkProcessPrepareToSuspend:(void(^)(void))completionHandler
{
    _websiteDataStore->sendNetworkProcessPrepareToSuspendForTesting([completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_sendNetworkProcessWillSuspendImminently
{
    _websiteDataStore->sendNetworkProcessWillSuspendImminentlyForTesting();

}

- (void)_sendNetworkProcessDidResume
{
    _websiteDataStore->sendNetworkProcessDidResume();
}

- (void)_synthesizeAppIsBackground:(BOOL)background
{
    _websiteDataStore->networkProcess().synthesizeAppIsBackground(background);
}

- (pid_t)_networkProcessIdentifier
{
    return _websiteDataStore->networkProcess().processIdentifier();
}

+ (void)_makeNextNetworkProcessLaunchFailForTesting
{
    WebKit::WebsiteDataStore::makeNextNetworkProcessLaunchFailForTesting();
}

+ (void)_setNetworkProcessSuspensionAllowedForTesting:(BOOL)allowed
{
    WebKit::NetworkProcessProxy::setSuspensionAllowedForTesting(allowed);
}

- (BOOL)_networkProcessExists
{
    return !!_websiteDataStore->networkProcessIfExists();
}

+ (BOOL)_defaultNetworkProcessExists
{
    return !!WebKit::NetworkProcessProxy::defaultNetworkProcess();
}

- (void)_countNonDefaultSessionSets:(void(^)(size_t))completionHandler
{
    _websiteDataStore->countNonDefaultSessionSets([completionHandler = makeBlockPtr(completionHandler)] (size_t count) {
        completionHandler(count);
    });
}

-(bool)_hasServiceWorkerBackgroundActivityForTesting
{
    return _websiteDataStore->hasServiceWorkerBackgroundActivityForTesting();
}

-(void)_getPendingPushMessages:(void(^)(NSArray<NSDictionary *> *))completionHandler
{
    RELEASE_LOG(Push, "Getting pending push messages");

#if ENABLE(SERVICE_WORKER)
    _websiteDataStore->networkProcess().getPendingPushMessages(_websiteDataStore->sessionID(), [completionHandler = makeBlockPtr(completionHandler)] (const Vector<WebKit::WebPushMessage>& messages) {
        auto result = adoptNS([[NSMutableArray alloc] initWithCapacity:messages.size()]);
        for (auto& message : messages)
            [result addObject:message.toDictionary()];

        RELEASE_LOG(Push, "Giving application %zu pending push messages", messages.size());
        completionHandler(result.get());
    });
#endif
}

-(void)_processPushMessage:(NSDictionary *)pushMessageDictionary completionHandler:(void(^)(bool wasProcessed))completionHandler
{
#if ENABLE(SERVICE_WORKER)
    auto pushMessage = WebKit::WebPushMessage::fromDictionary(pushMessageDictionary);
    if (!pushMessage) {
        RELEASE_LOG_ERROR(Push, "Asked to handle an invalid push message");
        completionHandler(false);
        return;
    }

    RELEASE_LOG(Push, "Sending push message for scope %" PRIVATE_LOG_STRING " to network process to handle", pushMessage->registrationURL.string().utf8().data());
    _websiteDataStore->networkProcess().processPushMessage(_websiteDataStore->sessionID(), *pushMessage, [completionHandler = makeBlockPtr(completionHandler)] (bool wasProcessed) {
        RELEASE_LOG(Push, "Push message processing complete. Callback result: %d", wasProcessed);
        completionHandler(wasProcessed);
    });
#endif
}

-(void)_processPersistentNotificationClick:(NSDictionary *)notificationDictionaryRepresentation completionHandler:(void(^)(bool))completionHandler
{
#if ENABLE(SERVICE_WORKER)
    auto notificationData = WebCore::NotificationData::fromDictionary(notificationDictionaryRepresentation);
    if (!notificationData) {
        RELEASE_LOG_ERROR(Push, "Asked to handle a persistent notification click with an invalid notification dictionary representation");
        completionHandler(false);
        return;
    }

    RELEASE_LOG(Push, "Sending persistent notification click from origin %" PRIVATE_LOG_STRING " to network process to handle", notificationData->originString.utf8().data());

    notificationData->sourceSession = _websiteDataStore->sessionID();
    _websiteDataStore->networkProcess().processNotificationEvent(*notificationData, WebCore::NotificationEventType::Click, [completionHandler = makeBlockPtr(completionHandler)] (bool wasProcessed) {
        RELEASE_LOG(Push, "Notification click event processing complete. Callback result: %d", wasProcessed);
        completionHandler(wasProcessed);
    });
#endif
}

-(void)_processPersistentNotificationClose:(NSDictionary *)notificationDictionaryRepresentation completionHandler:(void(^)(bool))completionHandler
{
#if ENABLE(SERVICE_WORKER)
    auto notificationData = WebCore::NotificationData::fromDictionary(notificationDictionaryRepresentation);
    if (!notificationData) {
        RELEASE_LOG_ERROR(Push, "Asked to handle a persistent notification click with an invalid notification dictionary representation");
        completionHandler(false);
        return;
    }

    RELEASE_LOG(Push, "Sending persistent notification close from origin %" PRIVATE_LOG_STRING " to network process to handle", notificationData->originString.utf8().data());

    _websiteDataStore->networkProcess().processNotificationEvent(*notificationData, WebCore::NotificationEventType::Close, [completionHandler = makeBlockPtr(completionHandler)] (bool wasProcessed) {
        RELEASE_LOG(Push, "Notification close event processing complete. Callback result: %d", wasProcessed);
        completionHandler(wasProcessed);
    });
#endif
}

-(void)_deletePushAndNotificationRegistration:(WKSecurityOrigin *)securityOrigin completionHandler:(void(^)(NSError *))completionHandler
{
    auto completionHandlerCopy = makeBlockPtr(completionHandler);
    _websiteDataStore->networkProcess().deletePushAndNotificationRegistration(_websiteDataStore->sessionID(), securityOrigin->_securityOrigin->securityOrigin(), [completionHandlerCopy](const String& errorString) {
        if (errorString.isEmpty()) {
            completionHandlerCopy(nil);
            return;
        }
        completionHandlerCopy([NSError errorWithDomain:@"WKWebSiteDataStore" code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey:(NSString *)errorString }]);
    });
}

-(void)_getOriginsWithPushAndNotificationPermissions:(void(^)(NSSet<WKSecurityOrigin *> *))completionHandler
{
    auto completionHandlerCopy = makeBlockPtr(completionHandler);
    _websiteDataStore->networkProcess().getOriginsWithPushAndNotificationPermissions(_websiteDataStore->sessionID(), [completionHandlerCopy](const Vector<WebCore::SecurityOriginData>& origins) {
        auto set = adoptNS([[NSMutableSet alloc] initWithCapacity:origins.size()]);
        for (auto& origin : origins) {
            auto apiOrigin = API::SecurityOrigin::create(origin);
            [set addObject:wrapper(apiOrigin.get())];
        }
        completionHandlerCopy(set.get());
    });
}

-(void)_scopeURL:(NSURL *)scopeURL hasPushSubscriptionForTesting:(void(^)(BOOL))completionHandler
{
    auto completionHandlerCopy = makeBlockPtr(completionHandler);
    _websiteDataStore->networkProcess()
        .hasPushSubscriptionForTesting(_websiteDataStore->sessionID(), scopeURL, [completionHandlerCopy](bool result) {
            completionHandlerCopy(result);
        });
}

-(void)_originDirectoryForTesting:(NSURL *)origin topOrigin:(NSURL *)topOrigin type:(NSString *)dataType completionHandler:(void(^)(NSString *))completionHandler
{
    auto websiteDataType = WebKit::toWebsiteDataType(dataType);
    if (!websiteDataType)
        return completionHandler(nil);

    auto completionHandlerCopy = makeBlockPtr(completionHandler);
    _websiteDataStore->originDirectoryForTesting(origin, topOrigin, *websiteDataType, [completionHandlerCopy = WTFMove(completionHandlerCopy)](auto result) {
        completionHandlerCopy(result);
    });
}

-(void)_setBackupExclusionPeriodForTesting:(double)seconds completionHandler:(void(^)(void))completionHandler
{
#if PLATFORM(IOS_FAMILY)
    _websiteDataStore->setBackupExclusionPeriodForTesting(Seconds(seconds), [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
#else
    UNUSED_PARAM(seconds);
    completionHandler();
#endif
}


@end
