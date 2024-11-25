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
#import "BackgroundFetchChange.h"
#import "BackgroundFetchState.h"
#import "BaseBoardSPI.h"
#import "CompletionHandlerCallChecker.h"
#import "NetworkProcessProxy.h"
#import "RestrictedOpenerType.h"
#import "ShouldGrandfatherStatistics.h"
#import "UserNotificationsSPI.h"
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
#import "WebPushDaemonConstants.h"
#import "WebPushMessage.h"
#import "WebResourceLoadStatisticsStore.h"
#import "WebsiteDataFetchOption.h"
#import "_WKNotificationDataInternal.h"
#import "_WKResourceLoadStatisticsThirdPartyInternal.h"
#import "_WKWebPushActionInternal.h"
#import "_WKWebsiteDataStoreConfigurationInternal.h"
#import "_WKWebsiteDataStoreDelegate.h"
#import <WebCore/Credential.h>
#import <WebCore/RegistrableDomain.h>
#import <WebCore/ResourceResponse.h>
#import <WebCore/SerializedCryptoKeyWrap.h>
#import <WebCore/ServiceWorkerClientData.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebCorePersistentCoders.h>
#import <wtf/BlockPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/URL.h>
#import <wtf/Vector.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

#if HAVE(NW_PROXY_CONFIG)
#import <Network/Network.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#endif

@interface WKWebsiteDataStore (WKWebPushHandling)
- (void)_handleWebPushAction:(_WKWebPushAction *)webPushAction;
@end

class WebsiteDataStoreClient final : public WebKit::WebsiteDataStoreClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WebsiteDataStoreClient);
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
        , m_hasNavigateToNotificationActionURLSelector([m_delegate.get() respondsToSelector:@selector(websiteDataStore:navigateToNotificationActionURL:)])
        , m_hasGetDisplayedNotificationsSelector([m_delegate.get() respondsToSelector:@selector(websiteDataStore:getDisplayedNotificationsForWorkerOrigin:completionHandler:)])
        , m_hasRequestBackgroundFetchPermissionSelector([m_delegate.get() respondsToSelector:@selector(requestBackgroundFetchPermission:frameOrigin:decisionHandler:)])
        , m_hasNotifyBackgroundFetchChangeSelector([m_delegate.get() respondsToSelector:@selector(notifyBackgroundFetchChange:change:)])
        , m_hasWindowProxyPropertyAccessSelector([m_delegate.get() respondsToSelector:@selector(websiteDataStore:domain:didOpenDomainViaWindowOpen:withProperty:directly:)])
        , m_hasDidAllowPrivateTokenUsageByThirdPartyForTestingSelector([m_delegate.get() respondsToSelector:@selector(websiteDataStore:didAllowPrivateTokenUsageByThirdPartyForTesting:forResourceURL:)])
        , m_hasDidExceedMemoryFootprintThresholdSelector([m_delegate.get() respondsToSelector:@selector(websiteDataStore:domain:didExceedMemoryFootprintThreshold:withPageCount:processLifetime:inForeground:wasPrivateRelayed:canSuspend:)])
        , m_hasWebCryptoMasterKeySelector([m_delegate.get() respondsToSelector:@selector(webCryptoMasterKey:)])
    {
    }

private:
    void webCryptoMasterKey(CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&& completionHandler) final
    {
        if (!m_hasWebCryptoMasterKeySelector || !m_delegate)
            return completionHandler(std::nullopt);

        auto checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.getAutoreleased(), @selector(webCryptoMasterKey:));
        [m_delegate webCryptoMasterKey:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (NSData *result) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            if (!result)
                return completionHandler(std::nullopt);
            completionHandler(makeVector(result));
        }).get()];
    }

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

    void reportServiceWorkerConsoleMessage(const URL&, const WebCore::SecurityOriginData&, MessageSource, MessageLevel, const String& message, unsigned long)
    {
        if (![m_delegate.get() respondsToSelector:@selector(websiteDataStore:reportServiceWorkerConsoleMessage:)])
            return;
        [m_delegate.getAutoreleased() websiteDataStore:m_dataStore.getAutoreleased() reportServiceWorkerConsoleMessage:message];
    }

    bool showNotification(const WebCore::NotificationData& data) final
    {
        if (!m_hasShowNotificationSelector || !m_delegate || !m_dataStore)
            return false;

        RetainPtr<_WKNotificationData> notification = adoptNS([[_WKNotificationData alloc] _initWithCoreData:data]);
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
            auto originString = WebCore::SecurityOriginData::fromURL(URL(key)).toString();
            if (originString.isEmpty()) {
                RELEASE_LOG(Push, "[WKWebsiteDataStoreDelegate notificationPermissionsForWebsiteDataStore:] returned a URL string that could not be parsed into a security origin. Skipping.");
                continue;
            }
            result.set(originString, value.boolValue);
        }

        return result;
    }

    bool hasGetDisplayedNotifications() const
    {
        return m_hasGetDisplayedNotificationsSelector;
    }

    void getDisplayedNotifications(const WebCore::SecurityOriginData& origin, CompletionHandler<void(Vector<WebCore::NotificationData>&&)>&& completionHandler) final
    {
        if (!m_hasGetDisplayedNotificationsSelector || !m_delegate || !m_dataStore) {
            completionHandler({ });
            return;
        }

        auto apiOrigin = API::SecurityOrigin::create(origin);
        auto delegateCompletionHandler = makeBlockPtr([completionHandler = WTFMove(completionHandler)] (NSArray<NSDictionary *> *notifications) mutable {
            Vector<WebCore::NotificationData> notificationDatas;
            for (id notificationDictionary in notifications) {
                auto notification = WebCore::NotificationData::fromDictionary(notificationDictionary);
                RELEASE_ASSERT_WITH_MESSAGE(notification, "getDisplayedNotificationsForWorkerOrigin: Invalid notification dictionary passed back to WebKit");
                notificationDatas.append(*notification);
            }
            completionHandler(WTFMove(notificationDatas));
        });

        [m_delegate.getAutoreleased() websiteDataStore:m_dataStore.getAutoreleased() getDisplayedNotificationsForWorkerOrigin:wrapper(apiOrigin.get()) completionHandler:delegateCompletionHandler.get()];
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

    void navigationToNotificationActionURL(const URL& url) final
    {
        if (!m_hasNavigateToNotificationActionURLSelector || !m_delegate || !m_dataStore)
            return;

        [m_delegate.getAutoreleased() websiteDataStore:m_dataStore.getAutoreleased() navigateToNotificationActionURL:(NSURL *)url];
    }


    void requestBackgroundFetchPermission(const WebCore::SecurityOriginData& topOrigin, const WebCore::SecurityOriginData& frameOrigin, CompletionHandler<void(bool)>&& completionHandler) final
    {
        if (!m_hasRequestBackgroundFetchPermissionSelector) {
            completionHandler(false);
            return;
        }

        auto checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.getAutoreleased(), @selector(requestBackgroundFetchPermission: frameOrigin: decisionHandler:));
        auto decisionHandler = makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](bool result) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(result);
        });

        URL mainFrameURL { topOrigin.toString() };
        URL frameURL { frameOrigin.toString() };

        [m_delegate.getAutoreleased() requestBackgroundFetchPermission:mainFrameURL frameOrigin:frameURL decisionHandler:decisionHandler.get()];
    }

    void notifyBackgroundFetchChange(const String& backgroundFetchIdentifier, WebKit::BackgroundFetchChange backgroundFetchChange) final
    {
        if (!m_hasNotifyBackgroundFetchChangeSelector)
            return;

        WKBackgroundFetchChange change;
        switch (backgroundFetchChange) {
        case WebKit::BackgroundFetchChange::Addition:
            change = WKBackgroundFetchChangeAddition;
            break;
        case WebKit::BackgroundFetchChange::Removal:
            change = WKBackgroundFetchChangeRemoval;
            break;
        case WebKit::BackgroundFetchChange::Update:
            change = WKBackgroundFetchChangeUpdate;
            break;
        }
        [m_delegate.getAutoreleased() notifyBackgroundFetchChange:backgroundFetchIdentifier change:change];
    }

    void didAccessWindowProxyProperty(const WebCore::RegistrableDomain& parentDomain, const WebCore::RegistrableDomain& childDomain, WebCore::WindowProxyProperty property, bool directlyAccessedProperty) final
    {
        if (!m_hasWindowProxyPropertyAccessSelector)
            return;

        WKWindowProxyProperty windowProxyProperty;
        switch (property) {
        case WebCore::WindowProxyProperty::PostMessage:
            windowProxyProperty = WKWindowProxyPropertyPostMessage;
            break;
        case WebCore::WindowProxyProperty::Closed:
            windowProxyProperty = WKWindowProxyPropertyClosed;
            break;
        default:
            windowProxyProperty = WKWindowProxyPropertyOther;
        }

        [m_delegate.getAutoreleased() websiteDataStore:m_dataStore.getAutoreleased() domain:parentDomain.string() didOpenDomainViaWindowOpen:childDomain.string() withProperty:windowProxyProperty directly:directlyAccessedProperty];
    }

    void didAllowPrivateTokenUsageByThirdPartyForTesting(bool wasAllowed, WTF::URL&& resourceURL) final
    {
        if (!m_hasDidAllowPrivateTokenUsageByThirdPartyForTestingSelector)
            return;

        [m_delegate.getAutoreleased() websiteDataStore:m_dataStore.getAutoreleased() didAllowPrivateTokenUsageByThirdPartyForTesting:wasAllowed forResourceURL:resourceURL];
    }

    void didExceedMemoryFootprintThreshold(size_t footprint, const String& domain, unsigned pageCount, Seconds processLifetime, bool inForeground, WebCore::WasPrivateRelayed wasPrivateRelayed, CanSuspend canSuspend)
    {
        if (!m_hasDidExceedMemoryFootprintThresholdSelector)
            return;

        [m_delegate.getAutoreleased() websiteDataStore:m_dataStore.getAutoreleased() domain:(NSString *)domain didExceedMemoryFootprintThreshold:footprint withPageCount:pageCount processLifetime:processLifetime.seconds() inForeground:inForeground wasPrivateRelayed:wasPrivateRelayed == WebCore::WasPrivateRelayed::Yes canSuspend:canSuspend == CanSuspend::Yes];
    }

    WeakObjCPtr<WKWebsiteDataStore> m_dataStore;
    WeakObjCPtr<id <_WKWebsiteDataStoreDelegate> > m_delegate;
    bool m_hasRequestStorageSpaceSelector { false };
    bool m_hasAuthenticationChallengeSelector { false };
    bool m_hasOpenWindowSelector { false };
    bool m_hasShowNotificationSelector { false };
    bool m_hasNotificationPermissionsSelector { false };
    bool m_hasWorkerUpdatedAppBadgeSelector { false };
    bool m_hasNavigateToNotificationActionURLSelector { false };
    bool m_hasGetDisplayedNotificationsSelector { false };
    bool m_hasRequestBackgroundFetchPermissionSelector { false };
    bool m_hasNotifyBackgroundFetchChangeSelector { false };
    bool m_hasWindowProxyPropertyAccessSelector { false };
    bool m_hasDidAllowPrivateTokenUsageByThirdPartyForTestingSelector { false };
    bool m_hasDidExceedMemoryFootprintThresholdSelector { false };
    bool m_hasWebCryptoMasterKeySelector { false };
};

#if PLATFORM(IOS)

@interface _WKWebsiteDataStoreBSActionHandler : NSObject <_UIApplicationBSActionHandler> {
    BlockPtr<WKWebsiteDataStore *(_WKWebPushAction *)> _webPushActionHandler;
}
+ (_WKWebsiteDataStoreBSActionHandler *)shared;
- (void)setWebPushActionHandler:(WKWebsiteDataStore *(^)(_WKWebPushAction *action))handler;
- (BOOL)handleNotificationResponse:(UNNotificationResponse *)response;
@end

@interface _WKWebsiteDataStoreNotificationCenterDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

@implementation _WKWebsiteDataStoreNotificationCenterDelegate

- (void)userNotificationCenter:(UNUserNotificationCenter *)center didReceiveNotificationResponse:(UNNotificationResponse *)response withCompletionHandler:(void (^)())completionHandler
{
#if PLATFORM(IOS)
    [WKWebsiteDataStore handleNotificationResponse:response];
#endif
    completionHandler();
}

@end

#endif

@implementation WKWebsiteDataStore {
    RetainPtr<NSArray> _proxyConfigurations;
}

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

+ (WKWebsiteDataStore *)defaultDataStore
{
    return wrapper(WebKit::WebsiteDataStore::defaultDataStore()).autorelease();
}

+ (WKWebsiteDataStore *)nonPersistentDataStore
{
    return wrapper(WebKit::WebsiteDataStore::createNonPersistent()).autorelease();
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
        allWebsiteDataTypes.get() = adoptNS([[NSSet alloc] initWithArray:@[ WKWebsiteDataTypeDiskCache, WKWebsiteDataTypeFetchCache, WKWebsiteDataTypeMemoryCache, WKWebsiteDataTypeOfflineWebApplicationCache, WKWebsiteDataTypeCookies, WKWebsiteDataTypeSessionStorage, WKWebsiteDataTypeLocalStorage, WKWebsiteDataTypeIndexedDBDatabases, WKWebsiteDataTypeServiceWorkerRegistrations, WKWebsiteDataTypeWebSQLDatabases, WKWebsiteDataTypeFileSystem, WKWebsiteDataTypeSearchFieldRecentSearches, WKWebsiteDataTypeMediaKeys, WKWebsiteDataTypeHashSalt ]]);
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

- (NSUUID *)identifier
{
    return [self _identifier];
}

+ (WKWebsiteDataStore *)dataStoreForIdentifier:(NSUUID *)identifier
{
    if (!identifier)
        [NSException raise:NSInvalidArgumentException format:@"Identifier is nil"];

    auto uuid = WTF::UUID::fromNSUUID(identifier);
    if (!uuid || !uuid->isValid())
        [NSException raise:NSInvalidArgumentException format:@"Identifier (%s) is invalid for data store", String([identifier UUIDString]).utf8().data()];

    return wrapper(WebKit::WebsiteDataStore::dataStoreForIdentifier(*uuid)).autorelease();
}

+ (void)removeDataStoreForIdentifier:(NSUUID *)identifier completionHandler:(void(^)(NSError *))completionHandler
{
    [self _removeDataStoreWithIdentifier:identifier completionHandler:completionHandler];
}

+ (void)fetchAllDataStoreIdentifiers:(void(^)(NSArray<NSUUID *> *))completionHandler
{
    [self _fetchAllIdentifiers:completionHandler];
}

#if HAVE(NW_PROXY_CONFIG)
- (void)setProxyConfigurations:(NSArray<nw_proxy_config_t> *)proxyConfigurations
{
    _proxyConfigurations = adoptNS([proxyConfigurations copy]);
    if (!_proxyConfigurations || !_proxyConfigurations.get().count) {
        _websiteDataStore->clearProxyConfigData();
        return;
    }

    Vector<std::pair<Vector<uint8_t>, WTF::UUID>> configDataVector;
    for (nw_proxy_config_t proxyConfig in proxyConfigurations) {
        RetainPtr<NSData> agentData = adoptNS((NSData *)nw_proxy_config_copy_agent_data(proxyConfig));

        uuid_t proxyIdentifier;
        nw_proxy_config_get_identifier(proxyConfig, proxyIdentifier);

        configDataVector.append({ makeVector(agentData.get()), WTF::UUID(std::span<const uint8_t, 16> { proxyIdentifier }) });
    }

    _websiteDataStore->setProxyConfigData(WTFMove(configDataVector));
}

- (NSArray<nw_proxy_config_t> *)proxyConfigurations
{
    return _proxyConfigurations.get();
}

#endif // HAVE(NW_PROXY_CONFIG)

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
            _WKWebsiteDataTypeResourceLoadStatistics,
            _WKWebsiteDataTypeCredentials,
            _WKWebsiteDataTypeAdClickAttributions,
            _WKWebsiteDataTypePrivateClickMeasurements,
            _WKWebsiteDataTypeAlternativeServices
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
        return completionHandler([NSError errorWithDomain:@"WKWebSiteDataStore" code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey:@"Identifier is nil" }]);

    auto completionHandlerCopy = makeBlockPtr(completionHandler);
    auto uuid = WTF::UUID::fromNSUUID(identifier);
    if (!uuid)
        return completionHandler([NSError errorWithDomain:@"WKWebSiteDataStore" code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey:@"Identifier is invalid" }]);

    WebKit::WebsiteDataStore::removeDataStoreWithIdentifier(*uuid, [completionHandlerCopy](const String& errorString) {
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
    _websiteDataStore->resolveDirectoriesAsynchronously();

    return self;
}

- (void)_fetchDataRecordsOfTypes:(NSSet<NSString *> *)dataTypes withOptions:(_WKWebsiteDataStoreFetchOptions)options completionHandler:(void (^)(NSArray<WKWebsiteDataRecord *> *))completionHandler
{
    auto completionHandlerCopy = makeBlockPtr(completionHandler);

    OptionSet<WebKit::WebsiteDataFetchOption> fetchOptions;
    if (options & _WKWebsiteDataStoreFetchOptionComputeSizes)
        fetchOptions.add(WebKit::WebsiteDataFetchOption::ComputeSizes);

    _websiteDataStore->fetchData(WebKit::toWebsiteDataTypes(dataTypes), fetchOptions, [completionHandlerCopy = WTFMove(completionHandlerCopy)](auto websiteDataRecords) {
        auto elements = WTF::map(websiteDataRecords, [](auto& websiteDataRecord) -> RefPtr<API::Object> {
            return API::WebsiteDataRecord::create(WTFMove(websiteDataRecord));
        });

        completionHandlerCopy(wrapper(API::Array::create(WTFMove(elements))).get());
    });
}

- (BOOL)_resourceLoadStatisticsEnabled
{
    return _websiteDataStore->trackingPreventionEnabled();
}

- (void)_setResourceLoadStatisticsEnabled:(BOOL)enabled
{
    _websiteDataStore->setTrackingPreventionEnabled(enabled);
}

- (BOOL)_resourceLoadStatisticsDebugMode
{
    return _websiteDataStore->resourceLoadStatisticsDebugMode();
}

- (void)_setResourceLoadStatisticsDebugMode:(BOOL)enabled
{
    _websiteDataStore->setResourceLoadStatisticsDebugMode(enabled);
}

- (void)_setPrivateClickMeasurementDebugModeEnabled:(BOOL)enabled
{
    _websiteDataStore->setPrivateClickMeasurementDebugMode(enabled);
}

- (BOOL)_storageSiteValidationEnabled
{
    return _websiteDataStore->storageSiteValidationEnabled();
}

- (void)_setStorageSiteValidationEnabled:(BOOL)enabled
{
    _websiteDataStore->setStorageSiteValidationEnabled(enabled);
}

- (NSArray<NSURL *> *)_persistedSites
{
    auto urls = _websiteDataStore->persistedSiteURLs();
    RetainPtr result = adoptNS([[NSMutableArray alloc] initWithCapacity:urls.size()]);
    for (auto& url : urls)
        [result addObject:url];

    return result.autorelease();
}

- (void)_setPersistedSites:(NSArray<NSURL *> *)persistedSites
{
    HashSet<URL> urls;
    for (NSURL *site in persistedSites) {
        URL url { site };
        if (url.isValid())
            urls.add(WTFMove(url));
    }

    _websiteDataStore->setPersistedSiteURLs(WTFMove(urls));
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
    if (!_websiteDataStore->isPersistent())
        return;

    if (callback) {
        _websiteDataStore->setStatisticsTestingCallback([callback = makeBlockPtr(callback), self](const String& event) {
            callback(self, event);
        });
        return;
    }

    _websiteDataStore->setStatisticsTestingCallback(nullptr);
}

- (void)_setStorageAccessPromptQuirkForTesting:(NSString *)topFrameDomain withSubFrameDomains:(NSArray<NSString *> *)subFrameDomains withTriggerPages:(NSArray<NSString *> *)triggerPages completionHandler:(void(^)(void))completionHandler
{
    if (!_websiteDataStore->isPersistent())
        return;

    _websiteDataStore->setStorageAccessPromptQuirkForTesting(topFrameDomain, makeVector<String>(subFrameDomains), makeVector<String>(triggerPages), makeBlockPtr(completionHandler));
}

- (void)_grantStorageAccessForTesting:(NSString *)topFrameDomain withSubFrameDomains:(NSArray<NSString *> *)subFrameDomains completionHandler:(void(^)(void))completionHandler
{
    if (!_websiteDataStore->isPersistent()) {
        completionHandler();
        return;
    }

    _websiteDataStore->grantStorageAccessForTesting(WTFMove(topFrameDomain), makeVector<String>(subFrameDomains), makeBlockPtr(completionHandler));
}

- (void)_setResourceLoadStatisticsTimeAdvanceForTesting:(NSTimeInterval)time completionHandler:(void(^)(void))completionHandler
{
    _websiteDataStore->setResourceLoadStatisticsTimeAdvanceForTesting(Seconds(time), makeBlockPtr(completionHandler));
}

+ (void)_allowWebsiteDataRecordsForAllOrigins
{
    WebKit::WebsiteDataStore::allowWebsiteDataRecordsForAllOrigins();
}

- (void)_loadedSubresourceDomainsFor:(WKWebView *)webView completionHandler:(void (^)(NSArray<NSString *> *domains))completionHandler
{
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
        completionHandler(wrapper(API::Array::create(WTFMove(apiDomains))).get());
    });
}

- (void)_clearLoadedSubresourceDomainsFor:(WKWebView *)webView
{
    if (!webView)
        return;

    auto webPageProxy = [webView _page];
    if (!webPageProxy)
        return;

    webPageProxy->clearLoadedSubresourceDomains();
}

- (void)_scheduleCookieBlockingUpdate:(void (^)(void))completionHandler
{
    _websiteDataStore->scheduleCookieBlockingUpdate([completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
}

- (void)_logUserInteraction:(NSURL *)domain completionHandler:(void (^)(void))completionHandler
{
    _websiteDataStore->logUserInteraction(domain, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setPrevalentDomain:(NSURL *)domain completionHandler:(void (^)(void))completionHandler
{
    _websiteDataStore->setPrevalentResource(URL(domain), [completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
}

- (void)_getIsPrevalentDomain:(NSURL *)domain completionHandler:(void (^)(BOOL))completionHandler
{
    _websiteDataStore->isPrevalentResource(URL(domain), [completionHandler = makeBlockPtr(completionHandler)](bool enabled) {
        completionHandler(enabled);
    });
}

- (void)_clearPrevalentDomain:(NSURL *)domain completionHandler:(void (^)(void))completionHandler
{
    _websiteDataStore->clearPrevalentResource(URL(domain), [completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
}

- (void)_clearResourceLoadStatistics:(void (^)(void))completionHandler
{
    _websiteDataStore->scheduleClearInMemoryAndPersistent(WebKit::ShouldGrandfatherStatistics::No, [completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
}

- (void)_closeDatabases:(void (^)(void))completionHandler
{
    _websiteDataStore->closeDatabases([completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
}

- (void)_getResourceLoadStatisticsDataSummary:(void (^)(NSArray<_WKResourceLoadStatisticsThirdParty *> *))completionHandler
{
    _websiteDataStore->getResourceLoadStatisticsDataSummary([completionHandler = makeBlockPtr(completionHandler)] (Vector<WebKit::ITPThirdPartyData>&& thirdPartyDomains) {
        completionHandler(createNSArray(WTFMove(thirdPartyDomains), [] (WebKit::ITPThirdPartyData&& domain) {
            return wrapper(API::ResourceLoadStatisticsThirdParty::create(WTFMove(domain)));
        }).get());
    });
}

+ (void)_setCachedProcessSuspensionDelayForTesting:(double)delayInSeconds
{
    WebKit::WebsiteDataStore::setCachedProcessSuspensionDelayForTesting(Seconds(delayInSeconds));
}

- (void)_isRelationshipOnlyInDatabaseOnce:(NSURL *)firstPartyURL thirdParty:(NSURL *)thirdPartyURL completionHandler:(void (^)(BOOL))completionHandler
{
    _websiteDataStore->isRelationshipOnlyInDatabaseOnce(thirdPartyURL, firstPartyURL, [completionHandler = makeBlockPtr(completionHandler)] (bool result) {
        completionHandler(result);
    });
}

- (void)_isRegisteredAsSubresourceUnderFirstParty:(NSURL *)firstPartyURL thirdParty:(NSURL *)thirdPartyURL completionHandler:(void (^)(BOOL))completionHandler
{
    _websiteDataStore->isRegisteredAsSubresourceUnder(thirdPartyURL, firstPartyURL, [completionHandler = makeBlockPtr(completionHandler)](bool enabled) {
        completionHandler(enabled);
    });
}

- (void)_statisticsDatabaseHasAllTables:(void (^)(BOOL))completionHandler
{
    _websiteDataStore->statisticsDatabaseHasAllTables([completionHandler = makeBlockPtr(completionHandler)](bool hasAllTables) {
        completionHandler(hasAllTables);
    });
}

- (void)_processStatisticsAndDataRecords:(void (^)(void))completionHandler
{
    _websiteDataStore->scheduleStatisticsAndDataRecordsProcessing([completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
}

- (void)_setThirdPartyCookieBlockingMode:(BOOL)enabled onlyOnSitesWithoutUserInteraction:(BOOL)onlyOnSitesWithoutUserInteraction completionHandler:(void (^)(void))completionHandler
{
    _websiteDataStore->setResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(enabled, onlyOnSitesWithoutUserInteraction, [completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
}

- (void)_renameOrigin:(NSURL *)oldName to:(NSURL *)newName forDataOfTypes:(NSSet<NSString *> *)dataTypes completionHandler:(void (^)(void))completionHandler
{
    if (!dataTypes.count)
        return completionHandler();

    NSSet *supportedTypes = [NSSet setWithObjects:WKWebsiteDataTypeLocalStorage, WKWebsiteDataTypeIndexedDBDatabases, nil];
    if (![dataTypes isSubsetOfSet:supportedTypes])
        [NSException raise:NSInvalidArgumentException format:@"_renameOrigin can only be called with WKWebsiteDataTypeLocalStorage and WKWebsiteDataTypeIndexedDBDatabases right now."];

    auto oldOrigin = WebCore::SecurityOriginData::fromURLWithoutStrictOpaqueness(oldName);
    auto newOrigin = WebCore::SecurityOriginData::fromURLWithoutStrictOpaqueness(newName);
    _websiteDataStore->renameOriginInWebsiteData(WTFMove(oldOrigin), WTFMove(newOrigin), WebKit::toWebsiteDataTypes(dataTypes), [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (BOOL)_networkProcessHasEntitlementForTesting:(NSString *)entitlement
{
    return _websiteDataStore->networkProcessHasEntitlementForTesting(entitlement);
}

- (void)_setUserAgentStringQuirkForTesting:(NSString *)domain withUserAgent:(NSString *)userAgent completionHandler:(void (^)(void))completionHandler
{
    _websiteDataStore->setUserAgentStringQuirkForTesting(domain, userAgent, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setPrivateTokenIPCForTesting:(bool)enabled
{
    _websiteDataStore->setPrivateTokenIPCForTesting(enabled);
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
    return wrapper(_websiteDataStore->configuration().copy()).autorelease();
}

+ (WKNotificationManagerRef)_sharedServiceWorkerNotificationManager
{
    LOG(Push, "Accessing _sharedServiceWorkerNotificationManager");
    return WebKit::toAPI(&WebKit::WebNotificationManagerProxy::sharedServiceWorkerManager());
}

- (void)_allowTLSCertificateChain:(NSArray *)certificateChain forHost:(NSString *)host
{
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
        auto apiDomains = WTF::map(domains, [](auto& domain) -> RefPtr<API::Object> {
            return API::String::create(domain.string());
        });
        completionHandler(wrapper(API::Array::create(WTFMove(apiDomains))).get());
    });
#else
    completionHandler({ });
#endif
}

- (void)_appBoundSchemes:(void (^)(NSArray<NSString *> *))completionHandler
{
#if ENABLE(APP_BOUND_DOMAINS)
    _websiteDataStore->getAppBoundSchemes([completionHandler = makeBlockPtr(completionHandler)](auto& schemes) mutable {
        auto apiSchemes = WTF::map(schemes, [](auto& scheme) -> RefPtr<API::Object> {
            return API::String::create(scheme);
        });
        completionHandler(wrapper(API::Array::create(WTFMove(apiSchemes))).get());
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
    return _websiteDataStore->networkProcess().processID();
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

- (void)_getPendingPushMessage:(void(^)(NSDictionary *))completionHandler
{
    RELEASE_LOG(Push, "Getting pending push message");

    _websiteDataStore->networkProcess().getPendingPushMessage(_websiteDataStore->sessionID(), [completionHandler = makeBlockPtr(completionHandler)] (const auto& message) {
        RetainPtr<NSDictionary> result;
        if (message)
            result = message->toDictionary();
        RELEASE_LOG(Push, "Giving application %d pending push messages", result ? 1 : 0);
        completionHandler(result.get());
    });
}


-(void)_getPendingPushMessages:(void(^)(NSArray<NSDictionary *> *))completionHandler
{
    RELEASE_LOG(Push, "Getting pending push messages");

    _websiteDataStore->networkProcess().getPendingPushMessages(_websiteDataStore->sessionID(), [completionHandler = makeBlockPtr(completionHandler)] (const Vector<WebKit::WebPushMessage>& messages) {
        auto result = adoptNS([[NSMutableArray alloc] initWithCapacity:messages.size()]);
        for (auto& message : messages)
            [result addObject:message.toDictionary()];

        RELEASE_LOG(Push, "Giving application %zu pending push messages", messages.size());
        completionHandler(result.get());
    });
}

-(void)_processPushMessage:(NSDictionary *)pushMessageDictionary completionHandler:(void(^)(bool wasProcessed))completionHandler
{
    auto pushMessage = WebKit::WebPushMessage::fromDictionary(pushMessageDictionary);
    if (!pushMessage) {
        RELEASE_LOG_ERROR(Push, "Asked to handle an invalid push message");
        completionHandler(false);
        return;
    }

    _websiteDataStore->processPushMessage(WTFMove(*pushMessage), [completionHandler = makeBlockPtr(completionHandler)] (bool wasProcessed) {
        RELEASE_LOG(Push, "Push message processing complete. Callback result: %d", wasProcessed);
        completionHandler(wasProcessed);
    });
}

-(void)_processWebCorePersistentNotificationClick:(const WebCore::NotificationData&)constNotificationData completionHandler:(void(^)(bool))completionHandler
{
    WebCore::NotificationData notificationData = constNotificationData;

#if ENABLE(DECLARATIVE_WEB_PUSH)
    if (!notificationData.defaultActionURL.isEmpty() && _websiteDataStore->configuration().isDeclarativeWebPushEnabled()) {
        RELEASE_LOG(Push, "Sending persistent notification clicked with default action URL. Requesting navigation to it now.");

        _websiteDataStore->client().navigationToNotificationActionURL(notificationData.defaultActionURL);
        completionHandler(true);
        return;
    }
#endif

    RELEASE_LOG(Push, "Sending persistent notification click from origin %" SENSITIVE_LOG_STRING " to network process to handle", notificationData.originString.utf8().data());

    notificationData.sourceSession = _websiteDataStore->sessionID();
    _websiteDataStore->networkProcess().processNotificationEvent(notificationData, WebCore::NotificationEventType::Click, [completionHandler = makeBlockPtr(completionHandler)] (bool wasProcessed) {
        RELEASE_LOG(Push, "Notification click event processing complete. Callback result: %d", wasProcessed);
        completionHandler(wasProcessed);
    });
}

-(void)_processPersistentNotificationClick:(NSDictionary *)notificationDictionaryRepresentation completionHandler:(void(^)(bool))completionHandler
{
    auto notificationData = WebCore::NotificationData::fromDictionary(notificationDictionaryRepresentation);
    if (!notificationData) {
        RELEASE_LOG_ERROR(Push, "Asked to handle a persistent notification click with an invalid notification dictionary representation");
        completionHandler(false);
        return;
    }

    [self _processWebCorePersistentNotificationClick:*notificationData completionHandler:WTFMove(completionHandler)];
}

-(void)_processWebCorePersistentNotificationClose:(const WebCore::NotificationData&)notificationData completionHandler:(void(^)(bool))completionHandler
{
    RELEASE_LOG(Push, "Sending persistent notification close from origin %" SENSITIVE_LOG_STRING " to network process to handle", notificationData.originString.utf8().data());

    _websiteDataStore->networkProcess().processNotificationEvent(notificationData, WebCore::NotificationEventType::Close, [completionHandler = makeBlockPtr(completionHandler)] (bool wasProcessed) {
        RELEASE_LOG(Push, "Notification close event processing complete. Callback result: %d", wasProcessed);
        completionHandler(wasProcessed);
    });
}

-(void)_processPersistentNotificationClose:(NSDictionary *)notificationDictionaryRepresentation completionHandler:(void(^)(bool))completionHandler
{
    auto notificationData = WebCore::NotificationData::fromDictionary(notificationDictionaryRepresentation);
    if (!notificationData) {
        RELEASE_LOG_ERROR(Push, "Asked to handle a persistent notification click with an invalid notification dictionary representation");
        completionHandler(false);
        return;
    }

    [self _processWebCorePersistentNotificationClose:*notificationData completionHandler:WTFMove(completionHandler)];
}

-(void)_getAllBackgroundFetchIdentifiers:(void(^)(NSArray<NSString *> *identifiers))completionHandler
{
    _websiteDataStore->networkProcess().getAllBackgroundFetchIdentifiers(_websiteDataStore->sessionID(), [completionHandler = makeBlockPtr(completionHandler)] (auto identifiers) {
        auto result = adoptNS([[NSMutableArray alloc] initWithCapacity:identifiers.size()]);
        for (auto identifier : identifiers)
            [result addObject:(NSString *)identifier];
        completionHandler(result.autorelease());
    });
}

-(void)_getBackgroundFetchState:(NSString *) identifier completionHandler:(void(^)(NSDictionary *state))completionHandler
{
    _websiteDataStore->networkProcess().getBackgroundFetchState(_websiteDataStore->sessionID(), identifier, [completionHandler = makeBlockPtr(completionHandler)] (auto state) {
        completionHandler(state ? state->toDictionary() : nil);
    });
}

-(void)_abortBackgroundFetch:(NSString *) identifier completionHandler:(void(^)(void))completionHandler
{
    if (!completionHandler)
        completionHandler = [] { };

    _websiteDataStore->networkProcess().abortBackgroundFetch(_websiteDataStore->sessionID(), identifier, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}
-(void)_pauseBackgroundFetch:(NSString *) identifier completionHandler:(void(^)(void))completionHandler
{
    if (!completionHandler)
        completionHandler = [] { };

    _websiteDataStore->networkProcess().pauseBackgroundFetch(_websiteDataStore->sessionID(), identifier, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

-(void)_resumeBackgroundFetch:(NSString *) identifier completionHandler:(void(^)(void))completionHandler
{
    if (!completionHandler)
        completionHandler = [] { };

    _websiteDataStore->networkProcess().resumeBackgroundFetch(_websiteDataStore->sessionID(), identifier, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

-(void)_clickBackgroundFetch:(NSString *) identifier completionHandler:(void(^)(void))completionHandler
{
    if (!completionHandler)
        completionHandler = [] { };

    if (!completionHandler)
        completionHandler = [] { };

    _websiteDataStore->networkProcess().clickBackgroundFetch(_websiteDataStore->sessionID(), identifier, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

-(void)_storeServiceWorkerRegistrations:(void(^)(void))completionHandler
{
    _websiteDataStore->storeServiceWorkerRegistrations([completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

-(void)_setServiceWorkerOverridePreferences:(WKPreferences *)preferences
{
    _websiteDataStore->setServiceWorkerOverridePreferences(preferences ? preferences->_preferences.get() : nullptr);
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
    _websiteDataStore->originDirectoryForTesting(WebCore::ClientOrigin { WebCore::SecurityOriginData::fromURLWithoutStrictOpaqueness(topOrigin), WebCore::SecurityOriginData::fromURLWithoutStrictOpaqueness(origin) }, { *websiteDataType }, [completionHandlerCopy = WTFMove(completionHandlerCopy)](auto result) {
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

- (NSUUID *)_identifier
{
    auto identifier = _websiteDataStore->configuration().identifier();
    if (!identifier)
        return nil;

    return *identifier;
}

- (NSString *)_webPushPartition
{
    return _websiteDataStore->configuration().webPushPartitionString();
}

-(void)_setCompletionHandlerForRemovalFromNetworkProcess:(void(^)(NSError* error))completionHandler
{
    _websiteDataStore->setCompletionHandlerForRemovalFromNetworkProcess([completionHandlerCopy = makeBlockPtr(completionHandler)](auto errorMessage) {
        if (!errorMessage.isEmpty())
            return completionHandlerCopy([NSError errorWithDomain:@"WKWebSiteDataStore" code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey:errorMessage }]);

        return completionHandlerCopy(nil);
    });
}

- (void)_setRestrictedOpenerTypeForTesting:(_WKRestrictedOpenerType)openerType forDomain:(NSString *)domain
{
    _websiteDataStore->setRestrictedOpenerTypeForDomainForTesting(WebCore::RegistrableDomain::fromRawString(domain), static_cast<WebKit::RestrictedOpenerType>(openerType));
}

-(void)_getAppBadgeForTesting:(void(^)(NSNumber *))completionHandler
{
    _websiteDataStore->getAppBadgeForTesting([completionHandlerCopy = makeBlockPtr(completionHandler)] (std::optional<uint64_t> badge) {
        if (badge)
            completionHandlerCopy([NSNumber numberWithUnsignedLongLong:*badge]);
        else
            completionHandlerCopy(nil);
    });
}

+ (void)_setWebPushActionHandler:(WKWebsiteDataStore *(^)(_WKWebPushAction *))handler
{
#if PLATFORM(IOS)
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        [UIApplication.sharedApplication _registerBSActionHandler:_WKWebsiteDataStoreBSActionHandler.shared];

        if (!UNUserNotificationCenter.currentNotificationCenter.delegate) {
            static NeverDestroyed<RetainPtr<_WKWebsiteDataStoreNotificationCenterDelegate>> notificationDelegate = adoptNS([[_WKWebsiteDataStoreNotificationCenterDelegate alloc] init]);
            UNUserNotificationCenter.currentNotificationCenter.delegate = notificationDelegate.get().get();
        }
    });
    [_WKWebsiteDataStoreBSActionHandler.shared setWebPushActionHandler:handler];
#else
    // FIXME: Implement for macOS
    UNUSED_PARAM(handler);
#endif
}

+ (BOOL)handleNotificationResponse:(UNNotificationResponse *)response
{
#if PLATFORM(IOS)
    return [_WKWebsiteDataStoreBSActionHandler.shared handleNotificationResponse:response];
#else
    return NO;
#endif
}

- (void)_handleNextPushMessageWithCompletionHandler:(void(^)())completionHandler
{
    [self _getPendingPushMessage:^(NSDictionary *payload) {
        if (!payload) {
            completionHandler();
            return;
        }

        [self _processPushMessage:payload completionHandler:^(bool showedNotification) {
            [self _handleNextPushMessageWithCompletionHandler:completionHandler];
        }];
    }];
}

- (void)_handleWebPushAction:(_WKWebPushAction *)webPushAction
{
    UIBackgroundTaskIdentifier backgroundTaskIdentifier = [webPushAction beginBackgroundTaskForHandling];
    auto completionHandler = ^{
#if PLATFORM(IOS)
        [UIApplication.sharedApplication endBackgroundTask:backgroundTaskIdentifier];
#else
        UNUSED_PARAM(backgroundTaskIdentifier);
#endif
    };

    if ([webPushAction.type isEqualToString:_WKWebPushActionTypePushEvent])
        [self _handleNextPushMessageWithCompletionHandler:completionHandler];
    else if ([webPushAction.type isEqualToString:_WKWebPushActionTypeNotificationClick]) {
        RELEASE_ASSERT(webPushAction.coreNotificationData);
        [self _processWebCorePersistentNotificationClick:*webPushAction.coreNotificationData completionHandler:^(bool) {
            completionHandler();
        }];
    } else if ([webPushAction.type isEqualToString:_WKWebPushActionTypeNotificationClose]) {
        RELEASE_ASSERT(webPushAction.coreNotificationData);
        [self _processWebCorePersistentNotificationClose:*webPushAction.coreNotificationData completionHandler:^(bool) {
            completionHandler();
        }];
    } else {
        RELEASE_LOG_ERROR(Push, "Unhandled webPushAction: %@", webPushAction);
        completionHandler();
    }
}

- (void)_runningOrTerminatingServiceWorkerCountForTesting:(void(^)(NSUInteger))completionHandler
{
    auto completionHandlerCopy = makeBlockPtr(completionHandler);
    _websiteDataStore->runningOrTerminatingServiceWorkerCountForTesting([completionHandlerCopy = WTFMove(completionHandlerCopy)](auto result) {
        completionHandlerCopy(result);
    });
}

- (void)_fetchDataOfTypes:(NSSet<NSString *> *)dataTypes completionHandler:(WK_SWIFT_UI_ACTOR void (^)(NSData *))completionHandler
{
    if ([dataTypes containsObject:WKWebsiteDataTypeLocalStorage]) {
        _websiteDataStore->fetchLocalStorage([completionHandler = makeBlockPtr(completionHandler)](HashMap<WebCore::ClientOrigin, HashMap<String, String>>&& localStorage) {
            constexpr unsigned currentLocalStorageSerializationVersion = 1;

            WTF::Persistence::Encoder encoder;
            encoder << currentLocalStorageSerializationVersion;
            encoder << localStorage;
            completionHandler(toNSData(encoder.span()).get());
        });

        return;
    }

    completionHandler(nil);
}

@end

#if PLATFORM(IOS)

@implementation _WKWebsiteDataStoreBSActionHandler

+ (_WKWebsiteDataStoreBSActionHandler *)shared
{
    static NeverDestroyed<RetainPtr<_WKWebsiteDataStoreBSActionHandler>> shared = adoptNS([[_WKWebsiteDataStoreBSActionHandler alloc] init]);
    return shared.get().get();
}

- (void)setWebPushActionHandler:(WKWebsiteDataStore *(^)(_WKWebPushAction *))handler
{
    RELEASE_ASSERT(handler);
    _webPushActionHandler = handler;
}

- (BOOL)handleNotificationResponse:(UNNotificationResponse *)response
{
    RetainPtr<_WKWebPushAction> webPushAction = [_WKWebPushAction _webPushActionWithNotificationResponse:response];
    if (!webPushAction)
        return NO;

    dispatch_async(dispatch_get_main_queue(), ^{
        WKWebsiteDataStore *dataStore = _WKWebsiteDataStoreBSActionHandler.shared->_webPushActionHandler.get()(webPushAction.get());
        [dataStore _handleWebPushAction:webPushAction.get()];
    });

    return YES;
}

- (NSSet<BSAction *> *)_respondToApplicationActions:(NSSet<BSAction *> *)applicationActions fromTransitionContext:(FBSSceneTransitionContext *)transitionContext
{
    RetainPtr unhandled = adoptNS([[NSMutableSet alloc] init]);

    for (BSAction *action in applicationActions) {
        NSDictionary *object = [action.info objectForSetting:WebKit::WebPushD::pushActionSetting];
        _WKWebPushAction *pushAction = [_WKWebPushAction webPushActionWithDictionary:object];
        if (!pushAction) {
            [unhandled addObject:action];
            continue;
        }

        WKWebsiteDataStore *dataStoreForPushAction = _webPushActionHandler.get()(pushAction);
        if (dataStoreForPushAction) {
            [dataStoreForPushAction _handleWebPushAction:pushAction];
            if (action.canSendResponse)
                [action sendResponse:BSActionResponse.response];
        } else {
            RELEASE_LOG_ERROR(Push, "Unable to handle a _WKWebPushAction: Client did not return a valid WKWebsiteDataStore");
            if (action.canSendResponse)
                [action sendResponse:[BSActionResponse responseForError:[NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]]];
        }
    }

    return unhandled.autorelease();
}

@end

#endif // PLATFORM(IOS)
