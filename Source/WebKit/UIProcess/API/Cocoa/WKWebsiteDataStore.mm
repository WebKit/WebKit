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
#import <wtf/CallbackAggregator.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/URL.h>
#import <wtf/Vector.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/persistence/PersistentDecoder.h>
#import <wtf/persistence/PersistentEncoder.h>

#if HAVE(NW_PROXY_CONFIG)
#import <Network/Network.h>
#endif

#if ENABLE(SCREEN_TIME)
#import <pal/cocoa/ScreenTimeSoftLink.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#endif

static Ref<WebKit::WebsiteDataStore> protectedWebsiteDataStore(WKWebsiteDataStore *dataStore)
{
    return *dataStore->_websiteDataStore;
}

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

        auto checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.get().get(), @selector(webCryptoMasterKey:));
        [m_delegate.get() webCryptoMasterKey:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (NSData *result) mutable {
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

        auto checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.get().get(), @selector(requestStorageSpace: frameOrigin: quota: currentSize: spaceRequired: decisionHandler:));
        auto decisionHandler = makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](unsigned long long quota) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(quota);
        });

        URL mainFrameURL { topOrigin.toString() };
        URL frameURL { frameOrigin.toString() };

        [m_delegate.get() requestStorageSpace:mainFrameURL.createNSURL().get() frameOrigin:frameURL.createNSURL().get() quota:quota currentSize:currentSize spaceRequired:spaceRequired decisionHandler:decisionHandler.get()];
    }

    void didReceiveAuthenticationChallenge(Ref<WebKit::AuthenticationChallengeProxy>&& challenge) final
    {
        if (!m_hasAuthenticationChallengeSelector || !m_delegate) {
            challenge->listener().completeChallenge(WebKit::AuthenticationChallengeDisposition::PerformDefaultHandling);
            return;
        }

        RetainPtr nsURLChallenge = wrapper(challenge);
        auto checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.get().get(), @selector(didReceiveAuthenticationChallenge: completionHandler:));
        auto completionHandler = makeBlockPtr([challenge = WTFMove(challenge), checker = WTFMove(checker)](NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            challenge->listener().completeChallenge(WebKit::toAuthenticationChallengeDisposition(disposition), WebCore::Credential(credential));
        });

        [m_delegate.get() didReceiveAuthenticationChallenge:nsURLChallenge.get() completionHandler:completionHandler.get()];
    }

    void openWindowFromServiceWorker(const String& url, const WebCore::SecurityOriginData& serviceWorkerOrigin, CompletionHandler<void(WebKit::WebPageProxy*)>&& callback)
    {
        if (!m_hasOpenWindowSelector || !m_delegate || !m_dataStore) {
            callback(nullptr);
            return;
        }

        auto checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.get().get(), @selector(websiteDataStore:openWindow:fromServiceWorkerOrigin:completionHandler:));
        auto completionHandler = makeBlockPtr([callback = WTFMove(callback), checker = WTFMove(checker)](WKWebView *newWebView) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();

            callback(RefPtr { newWebView._page.get() }.get());
        });

        RetainPtr nsURL = URL { url }.createNSURL();
        auto apiOrigin = API::SecurityOrigin::create(serviceWorkerOrigin);
        [m_delegate.get() websiteDataStore:m_dataStore.get().get() openWindow:nsURL.get() fromServiceWorkerOrigin:protectedWrapper(apiOrigin.get()).get() completionHandler:completionHandler.get()];
    }

    void reportServiceWorkerConsoleMessage(const URL&, const WebCore::SecurityOriginData&, MessageSource, MessageLevel, const String& message, unsigned long)
    {
        if (![m_delegate.get() respondsToSelector:@selector(websiteDataStore:reportServiceWorkerConsoleMessage:)])
            return;
        [m_delegate.get() websiteDataStore:m_dataStore.get().get() reportServiceWorkerConsoleMessage:message.createNSString().get()];
    }

    bool showNotification(const WebCore::NotificationData& data) final
    {
        if (!m_hasShowNotificationSelector || !m_delegate || !m_dataStore)
            return false;

        RetainPtr<_WKNotificationData> notification = adoptNS([[_WKNotificationData alloc] _initWithCoreData:data]);
        [m_delegate.get() websiteDataStore:m_dataStore.get().get() showNotification:notification.get()];
        return true;
    }

    HashMap<WTF::String, bool> notificationPermissions() final
    {
        if (!m_hasNotificationPermissionsSelector || !m_delegate || !m_dataStore)
            return { };

        HashMap<WTF::String, bool> result;
        RetainPtr permissions = [m_delegate.get() notificationPermissionsForWebsiteDataStore:m_dataStore.get().get()];
        for (NSString *key in permissions.get()) {
            RetainPtr<NSNumber> value = permissions.get()[key];
            auto originString = WebCore::SecurityOriginData::fromURL(URL(key)).toString();
            if (originString.isEmpty()) {
                RELEASE_LOG(Push, "[WKWebsiteDataStoreDelegate notificationPermissionsForWebsiteDataStore:] returned a URL string that could not be parsed into a security origin. Skipping.");
                continue;
            }
            result.set(originString, value.get().boolValue);
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

        [m_delegate.get() websiteDataStore:m_dataStore.get().get() getDisplayedNotificationsForWorkerOrigin:protectedWrapper(apiOrigin.get()).get() completionHandler:delegateCompletionHandler.get()];
    }

    void workerUpdatedAppBadge(const WebCore::SecurityOriginData& origin, std::optional<uint64_t> badge) final
    {
        if (!m_hasWorkerUpdatedAppBadgeSelector || !m_delegate || !m_dataStore)
            return;

        auto apiOrigin = API::SecurityOrigin::create(origin);
        RetainPtr<NSNumber> nsBadge;
        if (badge)
            nsBadge = @(*badge);

        [m_delegate.get() websiteDataStore:m_dataStore.get().get() workerOrigin:protectedWrapper(apiOrigin.get()).get() updatedAppBadge:nsBadge.get()];
    }

    void navigationToNotificationActionURL(const URL& url) final
    {
        if (!m_hasNavigateToNotificationActionURLSelector || !m_delegate || !m_dataStore)
            return;

        [m_delegate.get() websiteDataStore:m_dataStore.get().get() navigateToNotificationActionURL:url.createNSURL().get()];
    }


    void requestBackgroundFetchPermission(const WebCore::SecurityOriginData& topOrigin, const WebCore::SecurityOriginData& frameOrigin, CompletionHandler<void(bool)>&& completionHandler) final
    {
        if (!m_hasRequestBackgroundFetchPermissionSelector) {
            completionHandler(false);
            return;
        }

        auto checker = WebKit::CompletionHandlerCallChecker::create(m_delegate.get().get(), @selector(requestBackgroundFetchPermission: frameOrigin: decisionHandler:));
        auto decisionHandler = makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](bool result) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(result);
        });

        URL mainFrameURL { topOrigin.toString() };
        URL frameURL { frameOrigin.toString() };

        [m_delegate.get() requestBackgroundFetchPermission:mainFrameURL.createNSURL().get() frameOrigin:frameURL.createNSURL().get() decisionHandler:decisionHandler.get()];
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
        [m_delegate.get() notifyBackgroundFetchChange:backgroundFetchIdentifier.createNSString().get() change:change];
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

        [m_delegate.get() websiteDataStore:m_dataStore.get().get() domain:parentDomain.string().createNSString().get() didOpenDomainViaWindowOpen:childDomain.string().createNSString().get() withProperty:windowProxyProperty directly:directlyAccessedProperty];
    }

    void didAllowPrivateTokenUsageByThirdPartyForTesting(bool wasAllowed, WTF::URL&& resourceURL) final
    {
        if (!m_hasDidAllowPrivateTokenUsageByThirdPartyForTestingSelector)
            return;

        [m_delegate.get() websiteDataStore:m_dataStore.get().get() didAllowPrivateTokenUsageByThirdPartyForTesting:wasAllowed forResourceURL:resourceURL.createNSURL().get()];
    }

    void didExceedMemoryFootprintThreshold(size_t footprint, const String& domain, unsigned pageCount, Seconds processLifetime, bool inForeground, WebCore::WasPrivateRelayed wasPrivateRelayed, CanSuspend canSuspend)
    {
        if (!m_hasDidExceedMemoryFootprintThresholdSelector)
            return;

        [m_delegate.get() websiteDataStore:m_dataStore.get().get() domain:domain.createNSString().get() didExceedMemoryFootprintThreshold:footprint withPageCount:pageCount processLifetime:processLifetime.seconds() inForeground:inForeground wasPrivateRelayed:wasPrivateRelayed == WebCore::WasPrivateRelayed::Yes canSuspend:canSuspend == CanSuspend::Yes];
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

    SUPPRESS_UNRETAINED_ARG _websiteDataStore->WebKit::WebsiteDataStore::~WebsiteDataStore();

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
    static NeverDestroyed<RetainPtr<NSSet>> allWebsiteDataTypes = [] {
        RetainPtr allWebsiteDataTypes = adoptNS([[NSSet alloc] initWithArray:@[
            WKWebsiteDataTypeDiskCache,
            WKWebsiteDataTypeFetchCache,
            WKWebsiteDataTypeMemoryCache,
            WKWebsiteDataTypeCookies,
            WKWebsiteDataTypeSessionStorage,
            WKWebsiteDataTypeLocalStorage,
            WKWebsiteDataTypeIndexedDBDatabases,
            WKWebsiteDataTypeServiceWorkerRegistrations,
            WKWebsiteDataTypeWebSQLDatabases,
            WKWebsiteDataTypeFileSystem,
            WKWebsiteDataTypeSearchFieldRecentSearches,
            WKWebsiteDataTypeMediaKeys,
#if ENABLE(SCREEN_TIME)
            WKWebsiteDataTypeScreenTime,
#endif
            WKWebsiteDataTypeHashSalt ]]);
        return allWebsiteDataTypes;
    }();

    return allWebsiteDataTypes.get().get();
}

- (WKHTTPCookieStore *)httpCookieStore
{
    return wrapper(protectedWebsiteDataStore(self)->cookieStore());
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
    protectedWebsiteDataStore(self)->removeData(WebKit::toWebsiteDataTypes(dataTypes), toSystemClockTime(date ? date : [NSDate distantPast]), [completionHandlerCopy] {
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

    protectedWebsiteDataStore(self)->removeData(WebKit::toWebsiteDataTypes(dataTypes), toWebsiteDataRecords(dataRecords), [completionHandlerCopy] {
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
        protectedWebsiteDataStore(self)->clearProxyConfigData();
        return;
    }

    Vector<std::pair<Vector<uint8_t>, std::optional<WTF::UUID>>> configDataVector;
    for (nw_proxy_config_t proxyConfig in proxyConfigurations) {
        RetainPtr<NSData> agentData = adoptNS((NSData *)nw_proxy_config_copy_agent_data(proxyConfig));

        uuid_t proxyIdentifier;
        nw_proxy_config_get_identifier(proxyConfig, proxyIdentifier);

        WTF::UUID uuid { std::span<const uint8_t, 16> { proxyIdentifier } };
        configDataVector.append({ makeVector(agentData.get()), uuid.isValid() ? std::optional { uuid } : std::nullopt });
    }

    protectedWebsiteDataStore(self)->setProxyConfigData(WTFMove(configDataVector));
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

struct WKWebsiteData {
    std::optional<HashMap<WebCore::ClientOrigin, HashMap<String, String>>> localStorage;
};

- (void)fetchDataOfTypes:(NSSet<NSString *> *)dataTypes completionHandler:(void(^)(NSData *, NSError *))completionHandler
{
    Vector<WebKit::WebsiteDataType> dataTypesToEncode;
    for (NSString *dataType in dataTypes) {
        if ([dataType isEqualToString:WKWebsiteDataTypeLocalStorage]) {
            dataTypesToEncode.append(WebKit::WebsiteDataType::LocalStorage);
            continue;
        }

        NSString *unsupportedPrefix = @"This API does not support fetching: ";
        RetainPtr<NSString> unsupportedMessage = [unsupportedPrefix stringByAppendingString:dataType];
        NSDictionary *userInfo = @{ NSLocalizedDescriptionKey : unsupportedMessage.get(), };

        completionHandler(nullptr, adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:userInfo]).get());

        return;
    }

    auto data = Box<WKWebsiteData>::create();

    Ref callbackAggregator = CallbackAggregator::create([completionHandler = makeBlockPtr(completionHandler), dataTypesToEncode = WTFMove(dataTypesToEncode), data] {
        WTF::Persistence::Encoder encoder;
        constexpr unsigned currentWKWebsiteDataSerializationVersion = 1;
        encoder << currentWKWebsiteDataSerializationVersion;
        encoder << dataTypesToEncode;

        for (auto& dataTypeToEncode : dataTypesToEncode) {
            switch (dataTypeToEncode) {
            case WebKit::WebsiteDataType::LocalStorage:
                if (!data->localStorage) {
                    NSDictionary *userInfo = @{ NSLocalizedDescriptionKey : @"Unknown error occurred while fetching data.", };
                    completionHandler(nullptr, adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:userInfo]).get());

                    return;
                }

                encoder << data->localStorage.value();
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
        }

        completionHandler(toNSData(encoder.span()).get(), nullptr);
    });

    if ([dataTypes containsObject:WKWebsiteDataTypeLocalStorage]) {
        protectedWebsiteDataStore(self)->fetchLocalStorage([callbackAggregator, data](auto&& localStorage) {
            data->localStorage = WTFMove(localStorage);
        });
    }
}

- (void)restoreData:(NSData *)data completionHandler:(void(^)(NSError *))completionHandler
{
    WTF::Persistence::Decoder decoder(span(data));

    std::optional<unsigned> currentWKWebsiteDataSerializationVersion;
    decoder >> currentWKWebsiteDataSerializationVersion;

    if (!currentWKWebsiteDataSerializationVersion) {
        NSDictionary *userInfo = @{ NSLocalizedDescriptionKey : @"Version number is missing.", };
        completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:userInfo]).get());

        return;
    }

    std::optional<Vector<WebKit::WebsiteDataType>> encodedDataTypes;
    decoder >> encodedDataTypes;

    if (!encodedDataTypes) {
        NSDictionary *userInfo = @{ NSLocalizedDescriptionKey : @"List of encoded data types is missing.", };
        completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:userInfo]).get());

        return;
    }

    auto error = Box<RetainPtr<NSError>>::create(nil);

    Ref callbackAggregator = CallbackAggregator::create([completionHandler = makeBlockPtr(completionHandler), error] {
        if (*error)
            completionHandler(error->get());
        else
            completionHandler(nullptr);
    });

    for (auto& encodedDataType : *encodedDataTypes) {
        switch (encodedDataType) {
        case WebKit::WebsiteDataType::LocalStorage: {
            std::optional<HashMap<WebCore::ClientOrigin, HashMap<String, String>>> localStorage;
            decoder >> localStorage;

            if (!localStorage) {
                NSDictionary *userInfo = @{ NSLocalizedDescriptionKey : @"Encoded local storage data is missing.", };
                *error = adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:userInfo]).get();

                return;
            }

            if (!localStorage->isEmpty()) {
                protectedWebsiteDataStore(self)->restoreLocalStorage(WTFMove(*localStorage), [callbackAggregator, error](bool restoreSucceeded) {
                    if (!restoreSucceeded) {
                        NSDictionary *userInfo = @{ NSLocalizedDescriptionKey : @"Unknown error occurred while restoring data.", };
                        *error = adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:userInfo]).get();
                    }
                });
            }

            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
}

@end

@implementation WKWebsiteDataStore (WKPrivate)

+ (NSSet<NSString *> *)_allWebsiteDataTypesIncludingPrivate
{
    static NeverDestroyed<RetainPtr<NSSet>> allWebsiteDataTypes = [&] {
        auto *privateTypes = @[
            _WKWebsiteDataTypeHSTSCache,
            _WKWebsiteDataTypeResourceLoadStatistics,
            _WKWebsiteDataTypeCredentials,
            _WKWebsiteDataTypeAdClickAttributions,
            _WKWebsiteDataTypePrivateClickMeasurements,
            _WKWebsiteDataTypeAlternativeServices
        ];

        return [retainPtr([self allWebsiteDataTypes]) setByAddingObjectsFromArray:privateTypes];
    }();

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
            [result addObject:identifier.createNSUUID().get()];

        completionHandlerCopy(result.autorelease());
    });
}

+ (void)_removeDataStoreWithIdentifier:(NSUUID *)identifier completionHandler:(void(^)(NSError* error))completionHandler
{
    if (!identifier)
        return completionHandler(adoptNS([[NSError alloc] initWithDomain:@"WKWebSiteDataStore" code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey:@"Identifier is nil" }]).get());

    auto completionHandlerCopy = makeBlockPtr(completionHandler);
    auto uuid = WTF::UUID::fromNSUUID(identifier);
    if (!uuid)
        return completionHandler(adoptNS([[NSError alloc] initWithDomain:@"WKWebSiteDataStore" code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey:@"Identifier is invalid" }]).get());

    WebKit::WebsiteDataStore::removeDataStoreWithIdentifier(*uuid, [completionHandlerCopy](const String& errorString) {
        if (errorString.isEmpty())
            return completionHandlerCopy(nil);

        completionHandlerCopy(adoptNS([[NSError alloc] initWithDomain:@"WKWebSiteDataStore" code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey:errorString.createNSString().get() }]).get());
    });
}

- (instancetype)_initWithConfiguration:(_WKWebsiteDataStoreConfiguration *)configuration
{
    if (!(self = [super init]))
        return nil;

    auto sessionID = configuration.isPersistent ? PAL::SessionID::generatePersistentSessionID() : PAL::SessionID::generateEphemeralSessionID();
    API::Object::constructInWrapper<WebKit::WebsiteDataStore>(self, Ref { *configuration->_configuration }->copy(), sessionID);
    protectedWebsiteDataStore(self)->resolveDirectoriesAsynchronously();

    return self;
}

- (void)_fetchDataRecordsOfTypes:(NSSet<NSString *> *)dataTypes withOptions:(_WKWebsiteDataStoreFetchOptions)options completionHandler:(void (^)(NSArray<WKWebsiteDataRecord *> *))completionHandler
{
    auto completionHandlerCopy = makeBlockPtr(completionHandler);

    OptionSet<WebKit::WebsiteDataFetchOption> fetchOptions;
    if (options & _WKWebsiteDataStoreFetchOptionComputeSizes)
        fetchOptions.add(WebKit::WebsiteDataFetchOption::ComputeSizes);

    protectedWebsiteDataStore(self)->fetchData(WebKit::toWebsiteDataTypes(dataTypes), fetchOptions, [completionHandlerCopy = WTFMove(completionHandlerCopy)](auto websiteDataRecords) {
        auto elements = WTF::map(websiteDataRecords, [](auto& websiteDataRecord) -> RefPtr<API::Object> {
            return API::WebsiteDataRecord::create(WTFMove(websiteDataRecord));
        });

        completionHandlerCopy(wrapper(API::Array::create(WTFMove(elements))).get());
    });
}

- (BOOL)_resourceLoadStatisticsEnabled
{
    return protectedWebsiteDataStore(self)->trackingPreventionEnabled();
}

- (void)_setResourceLoadStatisticsEnabled:(BOOL)enabled
{
    protectedWebsiteDataStore(self)->setTrackingPreventionEnabled(enabled);
}

- (BOOL)_resourceLoadStatisticsDebugMode
{
    return protectedWebsiteDataStore(self)->resourceLoadStatisticsDebugMode();
}

- (void)_setResourceLoadStatisticsDebugMode:(BOOL)enabled
{
    protectedWebsiteDataStore(self)->setResourceLoadStatisticsDebugMode(enabled);
}

- (void)_setPrivateClickMeasurementDebugModeEnabled:(BOOL)enabled
{
    protectedWebsiteDataStore(self)->setPrivateClickMeasurementDebugMode(enabled);
}

- (BOOL)_storageSiteValidationEnabled
{
    return _websiteDataStore->storageSiteValidationEnabled();
}

- (void)_setStorageSiteValidationEnabled:(BOOL)enabled
{
    protectedWebsiteDataStore(self)->setStorageSiteValidationEnabled(enabled);
}

- (NSArray<NSURL *> *)_persistedSites
{
    auto urls = _websiteDataStore->persistedSiteURLs();
    RetainPtr result = adoptNS([[NSMutableArray alloc] initWithCapacity:urls.size()]);
    for (auto& url : urls)
        [result addObject:url.createNSURL().get()];

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

    protectedWebsiteDataStore(self)->setPersistedSiteURLs(WTFMove(urls));
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
        protectedWebsiteDataStore(self)->setStatisticsTestingCallback([callback = makeBlockPtr(callback), strongSelf = retainPtr(self)](const String& event) {
            callback(strongSelf.get(), event.createNSString().get());
        });
        return;
    }

    protectedWebsiteDataStore(self)->setStatisticsTestingCallback(nullptr);
}

- (void)_setStorageAccessPromptQuirkForTesting:(NSString *)topFrameDomain withSubFrameDomains:(NSArray<NSString *> *)subFrameDomains withTriggerPages:(NSArray<NSString *> *)triggerPages completionHandler:(void(^)(void))completionHandler
{
    if (!_websiteDataStore->isPersistent()) {
        completionHandler();
        return;
    }

    protectedWebsiteDataStore(self)->setStorageAccessPromptQuirkForTesting(topFrameDomain, makeVector<String>(subFrameDomains), makeVector<String>(triggerPages), makeBlockPtr(completionHandler));
}

- (void)_grantStorageAccessForTesting:(NSString *)topFrameDomain withSubFrameDomains:(NSArray<NSString *> *)subFrameDomains completionHandler:(void(^)(void))completionHandler
{
    if (!_websiteDataStore->isPersistent()) {
        completionHandler();
        return;
    }

    protectedWebsiteDataStore(self)->grantStorageAccessForTesting(WTFMove(topFrameDomain), makeVector<String>(subFrameDomains), makeBlockPtr(completionHandler));
}

- (void)_setResourceLoadStatisticsTimeAdvanceForTesting:(NSTimeInterval)time completionHandler:(void(^)(void))completionHandler
{
    protectedWebsiteDataStore(self)->setResourceLoadStatisticsTimeAdvanceForTesting(Seconds(time), makeBlockPtr(completionHandler));
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

    RefPtr page = [webView _page].get();
    if (!page) {
        completionHandler(nil);
        return;
    }
    
    page->getLoadedSubresourceDomains([completionHandler = makeBlockPtr(completionHandler)] (Vector<WebCore::RegistrableDomain>&& loadedSubresourceDomains) {
        Vector<RefPtr<API::Object>> apiDomains = WTF::map(loadedSubresourceDomains, [](auto& domain) -> RefPtr<API::Object> {
            return API::String::create(String(domain.string()));
        });
        completionHandler(wrapper(API::Array::create(WTFMove(apiDomains))).get());
    });
}

- (void)_clearLoadedSubresourceDomainsFor:(WKWebView *)webView
{
    if (!webView)
        return;

    if (RefPtr page = [webView _page].get())
        page->clearLoadedSubresourceDomains();
}

- (void)_scheduleCookieBlockingUpdate:(void (^)(void))completionHandler
{
    protectedWebsiteDataStore(self)->scheduleCookieBlockingUpdate([completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
}

- (void)_logUserInteraction:(NSURL *)domain completionHandler:(void (^)(void))completionHandler
{
    protectedWebsiteDataStore(self)->logUserInteraction(domain, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setPrevalentDomain:(NSURL *)domain completionHandler:(void (^)(void))completionHandler
{
    protectedWebsiteDataStore(self)->setPrevalentResource(URL(domain), [completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
}

- (void)_getIsPrevalentDomain:(NSURL *)domain completionHandler:(void (^)(BOOL))completionHandler
{
    protectedWebsiteDataStore(self)->isPrevalentResource(URL(domain), [completionHandler = makeBlockPtr(completionHandler)](bool enabled) {
        completionHandler(enabled);
    });
}

- (void)_clearPrevalentDomain:(NSURL *)domain completionHandler:(void (^)(void))completionHandler
{
    protectedWebsiteDataStore(self)->clearPrevalentResource(URL(domain), [completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
}

- (void)_clearResourceLoadStatistics:(void (^)(void))completionHandler
{
    protectedWebsiteDataStore(self)->scheduleClearInMemoryAndPersistent(WebKit::ShouldGrandfatherStatistics::No, [completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
}

- (void)_closeDatabases:(void (^)(void))completionHandler
{
    protectedWebsiteDataStore(self)->closeDatabases([completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
}

- (void)_getResourceLoadStatisticsDataSummary:(void (^)(NSArray<_WKResourceLoadStatisticsThirdParty *> *))completionHandler
{
    protectedWebsiteDataStore(self)->getResourceLoadStatisticsDataSummary([completionHandler = makeBlockPtr(completionHandler)] (Vector<WebKit::ITPThirdPartyData>&& thirdPartyDomains) {
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
    protectedWebsiteDataStore(self)->isRelationshipOnlyInDatabaseOnce(thirdPartyURL, firstPartyURL, [completionHandler = makeBlockPtr(completionHandler)] (bool result) {
        completionHandler(result);
    });
}

- (void)_isRegisteredAsSubresourceUnderFirstParty:(NSURL *)firstPartyURL thirdParty:(NSURL *)thirdPartyURL completionHandler:(void (^)(BOOL))completionHandler
{
    protectedWebsiteDataStore(self)->isRegisteredAsSubresourceUnder(thirdPartyURL, firstPartyURL, [completionHandler = makeBlockPtr(completionHandler)](bool enabled) {
        completionHandler(enabled);
    });
}

- (void)_statisticsDatabaseHasAllTables:(void (^)(BOOL))completionHandler
{
    protectedWebsiteDataStore(self)->statisticsDatabaseHasAllTables([completionHandler = makeBlockPtr(completionHandler)](bool hasAllTables) {
        completionHandler(hasAllTables);
    });
}

- (void)_processStatisticsAndDataRecords:(void (^)(void))completionHandler
{
    protectedWebsiteDataStore(self)->scheduleStatisticsAndDataRecordsProcessing([completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
    });
}

- (void)_setThirdPartyCookieBlockingMode:(BOOL)enabled onlyOnSitesWithoutUserInteraction:(BOOL)onlyOnSitesWithoutUserInteraction completionHandler:(void (^)(void))completionHandler
{
    WebCore::ThirdPartyCookieBlockingMode blockingMode = WebCore::ThirdPartyCookieBlockingMode::OnlyAccordingToPerDomainPolicy;
    if (enabled)
        blockingMode = onlyOnSitesWithoutUserInteraction ? WebCore::ThirdPartyCookieBlockingMode::AllOnSitesWithoutUserInteraction : WebCore::ThirdPartyCookieBlockingMode::All;
    protectedWebsiteDataStore(self)->setResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(enabled, blockingMode, [completionHandler = makeBlockPtr(completionHandler)]() {
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
    protectedWebsiteDataStore(self)->renameOriginInWebsiteData(WTFMove(oldOrigin), WTFMove(newOrigin), WebKit::toWebsiteDataTypes(dataTypes), [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (BOOL)_networkProcessHasEntitlementForTesting:(NSString *)entitlement
{
    return protectedWebsiteDataStore(self)->networkProcessHasEntitlementForTesting(entitlement);
}

- (void)_setUserAgentStringQuirkForTesting:(NSString *)domain withUserAgent:(NSString *)userAgent completionHandler:(void (^)(void))completionHandler
{
    protectedWebsiteDataStore(self)->setUserAgentStringQuirkForTesting(domain, userAgent, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_setPrivateTokenIPCForTesting:(bool)enabled
{
    protectedWebsiteDataStore(self)->setPrivateTokenIPCForTesting(enabled);
}

- (id <_WKWebsiteDataStoreDelegate>)_delegate
{
    return _delegate.get().unsafeGet();
}

- (void)set_delegate:(id <_WKWebsiteDataStoreDelegate>)delegate
{
    _delegate = delegate;
    protectedWebsiteDataStore(self)->setClient(makeUniqueRef<WebsiteDataStoreClient>(self, delegate));
}

- (_WKWebsiteDataStoreConfiguration *)_configuration
{
    return wrapper(_websiteDataStore->configuration().copy()).autorelease();
}

+ (WKNotificationManagerRef)_sharedServiceWorkerNotificationManager
{
    LOG(Push, "Accessing _sharedServiceWorkerNotificationManager");
    return WebKit::toAPI(&WebKit::WebNotificationManagerProxy::serviceWorkerManagerSingleton());
}

- (void)_allowTLSCertificateChain:(NSArray *)certificateChain forHost:(NSString *)host
{
}

- (void)_trustServerForLocalPCMTesting:(SecTrustRef)serverTrust
{
    protectedWebsiteDataStore(self)->allowTLSCertificateChainForLocalPCMTesting(WebCore::CertificateInfo(serverTrust));
}

- (void)_setPrivateClickMeasurementDebugModeEnabledForTesting:(BOOL)enabled
{
    protectedWebsiteDataStore(self)->setPrivateClickMeasurementDebugMode(enabled);
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
    protectedWebsiteDataStore(self)->terminateNetworkProcess();
}

- (void)_sendNetworkProcessPrepareToSuspend:(void(^)(void))completionHandler
{
    protectedWebsiteDataStore(self)->sendNetworkProcessPrepareToSuspendForTesting([completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

- (void)_sendNetworkProcessWillSuspendImminently
{
    protectedWebsiteDataStore(self)->sendNetworkProcessWillSuspendImminentlyForTesting();

}

- (void)_sendNetworkProcessDidResume
{
    protectedWebsiteDataStore(self)->sendNetworkProcessDidResume();
}

- (void)_synthesizeAppIsBackground:(BOOL)background
{
    protectedWebsiteDataStore(self)->protectedNetworkProcess()->synthesizeAppIsBackground(background);
}

- (pid_t)_networkProcessIdentifier
{
    return protectedWebsiteDataStore(self)->networkProcess().processID();
}

+ (void)_makeNextNetworkProcessLaunchFailForTesting
{
    WebKit::WebsiteDataStore::makeNextNetworkProcessLaunchFailForTesting();
}

+ (void)_setNetworkProcessSuspensionAllowedForTesting:(BOOL)allowed
{
    WebKit::NetworkProcessProxy::setSuspensionAllowedForTesting(allowed);
}

- (void)_forceNetworkProcessToTaskSuspendForTesting
{
    if (RefPtr networkProcess = _websiteDataStore->networkProcessIfExists())
        networkProcess->protectedThrottler()->invalidateAllActivitiesAndDropAssertion();
}

- (BOOL)_networkProcessExists
{
    return !!_websiteDataStore->networkProcessIfExists();
}

+ (BOOL)_defaultNetworkProcessExists
{
    return !!WebKit::NetworkProcessProxy::defaultNetworkProcess();
}

- (void)_countNonDefaultSessionSets:(void(^)(uint64_t))completionHandler
{
    protectedWebsiteDataStore(self)->countNonDefaultSessionSets([completionHandler = makeBlockPtr(completionHandler)] (uint64_t count) {
        completionHandler(count);
    });
}

-(bool)_hasServiceWorkerBackgroundActivityForTesting
{
    return protectedWebsiteDataStore(self)->hasServiceWorkerBackgroundActivityForTesting();
}

- (void)_getPendingPushMessage:(void(^)(NSDictionary *))completionHandler
{
    RELEASE_LOG(Push, "Getting pending push message");

    protectedWebsiteDataStore(self)->protectedNetworkProcess()->getPendingPushMessage(_websiteDataStore->sessionID(), [completionHandler = makeBlockPtr(completionHandler)] (const auto& message) {
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

    protectedWebsiteDataStore(self)->protectedNetworkProcess()->getPendingPushMessages(_websiteDataStore->sessionID(), [completionHandler = makeBlockPtr(completionHandler)] (const Vector<WebKit::WebPushMessage>& messages) {
        auto result = adoptNS([[NSMutableArray alloc] initWithCapacity:messages.size()]);
        for (auto& message : messages)
            [result addObject:message.toDictionary().get()];

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

    protectedWebsiteDataStore(self)->processPushMessage(WTFMove(*pushMessage), [completionHandler = makeBlockPtr(completionHandler)] (bool wasProcessed) {
        RELEASE_LOG(Push, "Push message processing complete. Callback result: %d", wasProcessed);
        completionHandler(wasProcessed);
    });
}

-(void)_processWebCorePersistentNotificationClick:(const WebCore::NotificationData&)constNotificationData completionHandler:(void(^)(bool))completionHandler
{
    WebCore::NotificationData notificationData = constNotificationData;

#if ENABLE(DECLARATIVE_WEB_PUSH)
    if (!notificationData.navigateURL.isEmpty() && _websiteDataStore->configuration().isDeclarativeWebPushEnabled()) {
        RELEASE_LOG(Push, "Sending persistent notification clicked with default action URL. Requesting navigation to it now.");

        _websiteDataStore->client().navigationToNotificationActionURL(notificationData.navigateURL);
        completionHandler(true);
        return;
    }
#endif

    RELEASE_LOG(Push, "Sending persistent notification click from origin %" SENSITIVE_LOG_STRING " to network process to handle", notificationData.originString.utf8().data());

    notificationData.sourceSession = _websiteDataStore->sessionID();
    protectedWebsiteDataStore(self)->protectedNetworkProcess()->processNotificationEvent(notificationData, WebCore::NotificationEventType::Click, [completionHandler = makeBlockPtr(completionHandler)] (bool wasProcessed) {
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

    protectedWebsiteDataStore(self)->protectedNetworkProcess()->processNotificationEvent(notificationData, WebCore::NotificationEventType::Close, [completionHandler = makeBlockPtr(completionHandler)] (bool wasProcessed) {
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
    protectedWebsiteDataStore(self)->protectedNetworkProcess()->getAllBackgroundFetchIdentifiers(_websiteDataStore->sessionID(), [completionHandler = makeBlockPtr(completionHandler)] (auto identifiers) {
        auto result = adoptNS([[NSMutableArray alloc] initWithCapacity:identifiers.size()]);
        for (auto identifier : identifiers)
            [result addObject:identifier.createNSString().get()];
        completionHandler(result.autorelease());
    });
}

-(void)_getBackgroundFetchState:(NSString *) identifier completionHandler:(void(^)(NSDictionary *state))completionHandler
{
    protectedWebsiteDataStore(self)->protectedNetworkProcess()->getBackgroundFetchState(_websiteDataStore->sessionID(), identifier, [completionHandler = makeBlockPtr(completionHandler)] (auto state) {
        completionHandler(state ? state->toDictionary() : nil);
    });
}

-(void)_abortBackgroundFetch:(NSString *) identifier completionHandler:(void(^)(void))completionHandler
{
    if (!completionHandler)
        completionHandler = [] { };

    protectedWebsiteDataStore(self)->protectedNetworkProcess()->abortBackgroundFetch(_websiteDataStore->sessionID(), identifier, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}
-(void)_pauseBackgroundFetch:(NSString *) identifier completionHandler:(void(^)(void))completionHandler
{
    if (!completionHandler)
        completionHandler = [] { };

    protectedWebsiteDataStore(self)->protectedNetworkProcess()->pauseBackgroundFetch(_websiteDataStore->sessionID(), identifier, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

-(void)_resumeBackgroundFetch:(NSString *) identifier completionHandler:(void(^)(void))completionHandler
{
    if (!completionHandler)
        completionHandler = [] { };

    protectedWebsiteDataStore(self)->protectedNetworkProcess()->resumeBackgroundFetch(_websiteDataStore->sessionID(), identifier, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

-(void)_clickBackgroundFetch:(NSString *) identifier completionHandler:(void(^)(void))completionHandler
{
    if (!completionHandler)
        completionHandler = [] { };

    if (!completionHandler)
        completionHandler = [] { };

    protectedWebsiteDataStore(self)->protectedNetworkProcess()->clickBackgroundFetch(_websiteDataStore->sessionID(), identifier, [completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
    });
}

-(void)_storeServiceWorkerRegistrations:(void(^)(void))completionHandler
{
    protectedWebsiteDataStore(self)->storeServiceWorkerRegistrations([completionHandler = makeBlockPtr(completionHandler)] {
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
    protectedWebsiteDataStore(self)->protectedNetworkProcess()
        ->hasPushSubscriptionForTesting(_websiteDataStore->sessionID(), scopeURL, [completionHandlerCopy](bool result) {
            completionHandlerCopy(result);
        });
}

-(void)_originDirectoryForTesting:(NSURL *)origin topOrigin:(NSURL *)topOrigin type:(NSString *)dataType completionHandler:(void(^)(NSString *))completionHandler
{
    auto websiteDataType = WebKit::toWebsiteDataType(dataType);
    if (!websiteDataType)
        return completionHandler(nil);

    auto completionHandlerCopy = makeBlockPtr(completionHandler);
    protectedWebsiteDataStore(self)->originDirectoryForTesting(WebCore::ClientOrigin { WebCore::SecurityOriginData::fromURLWithoutStrictOpaqueness(topOrigin), WebCore::SecurityOriginData::fromURLWithoutStrictOpaqueness(origin) }, { *websiteDataType }, [completionHandlerCopy = WTFMove(completionHandlerCopy)](auto result) {
        completionHandlerCopy(result.createNSString().get());
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

    return identifier->createNSUUID().autorelease();
}

- (NSString *)_webPushPartition
{
    return _websiteDataStore->configuration().webPushPartitionString().createNSString().autorelease();
}

-(void)_setCompletionHandlerForRemovalFromNetworkProcess:(void(^)(NSError* error))completionHandler
{
    protectedWebsiteDataStore(self)->setCompletionHandlerForRemovalFromNetworkProcess([completionHandlerCopy = makeBlockPtr(completionHandler)](auto errorMessage) {
        if (!errorMessage.isEmpty())
            return completionHandlerCopy(adoptNS([[NSError alloc] initWithDomain:@"WKWebSiteDataStore" code:WKErrorUnknown userInfo:@{ NSLocalizedDescriptionKey:errorMessage.createNSString().get() }]).get());

        return completionHandlerCopy(nil);
    });
}

- (void)_setRestrictedOpenerTypeForTesting:(_WKRestrictedOpenerType)openerType forDomain:(NSString *)domain
{
    protectedWebsiteDataStore(self)->setRestrictedOpenerTypeForDomainForTesting(WebCore::RegistrableDomain::fromRawString(domain), static_cast<WebKit::RestrictedOpenerType>(openerType));
}

-(void)_getAppBadgeForTesting:(void(^)(NSNumber *))completionHandler
{
    protectedWebsiteDataStore(self)->getAppBadgeForTesting([completionHandlerCopy = makeBlockPtr(completionHandler)] (std::optional<uint64_t> badge) {
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
    protectedWebsiteDataStore(self)->runningOrTerminatingServiceWorkerCountForTesting([completionHandlerCopy = WTFMove(completionHandlerCopy)](auto result) {
        completionHandlerCopy(result);
    });
}

- (void)_fetchDataOfTypes:(NSSet<NSString *> *)dataTypes completionHandler:(void(^)(NSData *))completionHandler
{
    [self fetchDataOfTypes:dataTypes completionHandler:makeBlockPtr([completionHandler = makeBlockPtr(completionHandler)](NSData *data, NSError *error) {
        UNUSED_PARAM(error);
        completionHandler(data);
    }).get()];
}

- (void)_restoreData:(NSData *)data completionHandler:(void(^)(BOOL))completionHandler
{
    [self restoreData:data completionHandler:makeBlockPtr([completionHandler = makeBlockPtr(completionHandler)](NSError *error) {
        completionHandler(!error);
    }).get()];
}

- (void)_isStorageSuspendedForTesting:(void(^)(BOOL))completionHandler
{
    auto completionHandlerCopy = makeBlockPtr(completionHandler);
    protectedWebsiteDataStore(self)->isStorageSuspendedForTesting([completionHandlerCopy = WTFMove(completionHandlerCopy)](auto result) {
        completionHandlerCopy(result);
    });
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

    dispatch_async(mainDispatchQueueSingleton(), ^{
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
                [action sendResponse:[BSActionResponse responseForError:adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]).get()]];
        }
    }

    return unhandled.autorelease();
}

@end

#endif // PLATFORM(IOS)
