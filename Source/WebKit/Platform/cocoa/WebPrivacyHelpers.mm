/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "WebPrivacyHelpers.h"

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

#import "Logging.h"
#import <WebCore/DNS.h>
#import <WebCore/LinkDecorationFilteringData.h>
#import <WebCore/OrganizationStorageAccessPromptQuirk.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/NetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RobinHoodHashMap.h>
#import <wtf/cocoa/VectorCocoa.h>

#import <pal/cocoa/WebPrivacySoftLink.h>

#if HAVE(SYSTEM_SUPPORT_FOR_ADVANCED_PRIVACY_PROTECTIONS)
SOFT_LINK_LIBRARY_OPTIONAL(libnetwork)
SOFT_LINK_OPTIONAL(libnetwork, nw_context_set_tracker_lookup_callback, void, __cdecl, (nw_context_t, nw_context_tracker_lookup_callback_t))
#endif

namespace WebKit {

static bool canUseWebPrivacyFramework()
{
#if HAVE(SYSTEM_SUPPORT_FOR_ADVANCED_PRIVACY_PROTECTIONS)
    return PAL::isWebPrivacyFrameworkAvailable();
#else
    // On macOS Monterey where WebPrivacy is present as a staged framework, attempts to soft-link the framework may fail.
    // Instead of using dlopen, we instead check for the presence of `WPResources`; the call to dlopen is not necessary because
    // we weak-link against the framework, so the class should be present as long as the framework has been successfully linked.
    // FIXME: This workaround can be removed once we drop support for macOS Monterey, and we can use the standard soft-linking
    // helpers from WebPrivacySoftLink.
    static bool hasWPResourcesClass = [&] {
        return !!PAL::getWPResourcesClass();
    }();
    return hasWPResourcesClass;
#endif
}

static NSNotificationName resourceDataChangedNotificationName()
{
#if HAVE(SYSTEM_SUPPORT_FOR_ADVANCED_PRIVACY_PROTECTIONS)
    return PAL::get_WebPrivacy_WPResourceDataChangedNotificationName();
#else
    // FIXME: This workaround can be removed once we drop support for macOS Monterey, and we can use the standard soft-linking
    // helpers from WebPrivacySoftLink.
    return @"WPResourceDataChangedNotificationName";
#endif
}

static NSString *notificationUserInfoResourceTypeKey()
{
#if HAVE(SYSTEM_SUPPORT_FOR_ADVANCED_PRIVACY_PROTECTIONS)
    return PAL::get_WebPrivacy_WPNotificationUserInfoResourceTypeKey();
#else
    // FIXME: This workaround can be removed once we drop support for macOS Monterey, and we can use the standard soft-linking
    // helpers from WebPrivacySoftLink.
    return @"ResourceType";
#endif
}

} // namespace WebKit

@interface WKWebPrivacyNotificationListener : NSObject

@end

@implementation WKWebPrivacyNotificationListener {
    BlockPtr<void()> _linkFilteringDataCallback;
    BlockPtr<void()> _storageAccessPromptQuirksDataCallback;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    if (WebKit::canUseWebPrivacyFramework())
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didUpdate:) name:WebKit::resourceDataChangedNotificationName() object:nil];
    return self;
}

- (void)listenForLinkFilteringDataChanges:(void(^)())callback
{
    _linkFilteringDataCallback = callback;
}

- (void)listenForStorageAccessPromptQuirkChanges:(void(^)())callback
{
    _storageAccessPromptQuirksDataCallback = callback;
}

- (void)dealloc
{
    if (WebKit::canUseWebPrivacyFramework())
        [[NSNotificationCenter defaultCenter] removeObserver:self name:WebKit::resourceDataChangedNotificationName() object:nil];
    [super dealloc];
}

- (void)didUpdate:(NSNotification *)notification
{
    ASSERT(WebKit::canUseWebPrivacyFramework());
    auto type = dynamic_objc_cast<NSNumber>([notification.userInfo objectForKey:WebKit::notificationUserInfoResourceTypeKey()]);
    if (!type)
        return;

    if (_linkFilteringDataCallback && type.integerValue == WPResourceTypeLinkFilteringData)
        _linkFilteringDataCallback();

    if (_storageAccessPromptQuirksDataCallback && type.integerValue == WPResourceTypeStorageAccessPromptQuirksData)
        _storageAccessPromptQuirksDataCallback();
}

@end

namespace WebKit {

LinkDecorationFilteringController& LinkDecorationFilteringController::shared()
{
    static LinkDecorationFilteringController* sharedInstance = new LinkDecorationFilteringController;
    return *sharedInstance;
}

Ref<LinkDecorationFilteringDataObserver> LinkDecorationFilteringController::observeUpdates(Function<void()>&& callback)
{
    if (!m_notificationListener) {
        m_notificationListener = adoptNS([WKWebPrivacyNotificationListener new]);
        [m_notificationListener listenForLinkFilteringDataChanges:^{
            updateStrings([this] {
                for (auto& observer : m_observers)
                    observer.invokeCallback();
            });
        }];
    }
    auto observer = LinkDecorationFilteringDataObserver::create(WTFMove(callback));
    m_observers.add(observer.get());
    return observer;
}

void LinkDecorationFilteringController::setCachedStrings(Vector<WebCore::LinkDecorationFilteringData>&& strings)
{
    m_cachedStrings = WTFMove(strings);
    m_cachedStrings.shrinkToFit();
}

void LinkDecorationFilteringController::updateStrings(CompletionHandler<void()>&& callback)
{
    if (!WebKit::canUseWebPrivacyFramework()) {
        callback();
        return;
    }

    static NeverDestroyed<Vector<CompletionHandler<void()>, 1>> lookupCallbacks;
    lookupCallbacks->append(WTFMove(callback));
    if (lookupCallbacks->size() > 1)
        return;

    auto options = adoptNS([PAL::allocWPResourceRequestOptionsInstance() init]);
    [options setAfterUpdates:NO];

    [[PAL::getWPResourcesClass() sharedInstance] requestLinkFilteringData:options.get() completionHandler:^(WPLinkFilteringData *data, NSError *error) {
        Vector<WebCore::LinkDecorationFilteringData> result;
        if (error)
            RELEASE_LOG_ERROR(ResourceLoadStatistics, "Failed to request query parameters from WebPrivacy.");
        else {
            auto rules = [data rules];
            for (WPLinkFilteringRule *rule : rules)
                result.append(WebCore::LinkDecorationFilteringData { rule.domain, rule.queryParameter });
            setCachedStrings(WTFMove(result));
        }

        for (auto& callback : std::exchange(lookupCallbacks.get(), { }))
            callback();
    }];

}

using LinkFilteringRulesCallback = CompletionHandler<void(Vector<WebCore::LinkDecorationFilteringData>&&)>;
void requestLinkDecorationFilteringData(LinkFilteringRulesCallback&& callback)
{
    if (!WebKit::canUseWebPrivacyFramework()) {
        callback({ });
        return;
    }

    static BOOL canRequestAllowedQueryParameters = [] {
        return [PAL::getWPResourcesClass() instancesRespondToSelector:@selector(requestAllowedLinkFilteringData:completionHandler:)];
    }();

    if (!canRequestAllowedQueryParameters) {
        callback({ });
        return;
    }

    static NeverDestroyed<Vector<LinkFilteringRulesCallback, 1>> lookupCallbacks;
    lookupCallbacks->append(WTFMove(callback));
    if (lookupCallbacks->size() > 1)
        return;

    auto options = adoptNS([PAL::allocWPResourceRequestOptionsInstance() init]);
    [options setAfterUpdates:NO];

    [[PAL::getWPResourcesClass() sharedInstance] requestAllowedLinkFilteringData:options.get() completionHandler:^(WPLinkFilteringData *data, NSError *error) {
        Vector<WebCore::LinkDecorationFilteringData> result;
        if (error)
            RELEASE_LOG_ERROR(ResourceLoadStatistics, "Failed to request allowed query parameters from WebPrivacy.");
        else {
            auto rules = [data rules];
            for (WPLinkFilteringRule *rule : rules)
                result.append(WebCore::LinkDecorationFilteringData { rule.domain, rule.queryParameter });
        }

        auto callbacks = std::exchange(lookupCallbacks.get(), { });
        for (int i = callbacks.size() - 1; i >= 0; --i) {
            auto& callback = callbacks[i];
            if (i)
                callback(Vector { result });
            else
                callback(WTFMove(result));
        }
    }];
}

StorageAccessPromptQuirkController& StorageAccessPromptQuirkController::shared()
{
    static MainThreadNeverDestroyed<StorageAccessPromptQuirkController> sharedInstance;
    return sharedInstance.get();
}

Ref<StorageAccessPromptQuirkObserver> StorageAccessPromptQuirkController::observeUpdates(Function<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    if (!m_notificationListener) {
        m_notificationListener = adoptNS([WKWebPrivacyNotificationListener new]);
        [m_notificationListener listenForStorageAccessPromptQuirkChanges:^{
            updateQuirks([this] {
                m_observers.forEach([](auto& observer) {
                    observer.invokeCallback();
                });
            });
        }];
    }
    Ref observer = StorageAccessPromptQuirkObserver::create(WTFMove(completionHandler));
    m_observers.add(observer.get());
    return observer;
}

void StorageAccessPromptQuirkController::setCachedQuirks(Vector<WebCore::OrganizationStorageAccessPromptQuirk>&& quirks)
{
    m_cachedQuirks = WTFMove(quirks);
    m_cachedQuirks.shrinkToFit();
}

void StorageAccessPromptQuirkController::setCachedQuirksForTesting(Vector<WebCore::OrganizationStorageAccessPromptQuirk>&& quirks)
{
    setCachedQuirks(WTFMove(quirks));
    m_observers.forEach([](auto& observer) {
        observer.invokeCallback();
    });
}

static HashMap<WebCore::RegistrableDomain, Vector<WebCore::RegistrableDomain>> domainPairingsDictToMap(NSDictionary<NSString *, NSArray<NSString *> *> *domainPairings)
{
    HashMap<WebCore::RegistrableDomain, Vector<WebCore::RegistrableDomain>> map;
    auto* topDomains = domainPairings.allKeys;
    for (NSString *topDomain : topDomains) {
        Vector<WebCore::RegistrableDomain> subFrameDomains;
        for (NSString *subFrameDomain : [domainPairings objectForKey:topDomain])
            subFrameDomains.append(WebCore::RegistrableDomain::fromRawString(subFrameDomain));
        map.add(WebCore::RegistrableDomain::fromRawString(String { topDomain }), WTFMove(subFrameDomains));
    }
    return map;
}

void StorageAccessPromptQuirkController::updateQuirks(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    if (!WebKit::canUseWebPrivacyFramework() || ![PAL::getWPResourcesClass() instancesRespondToSelector:@selector(requestStorageAccessPromptQuirksData:completionHandler:)]) {
        completionHandler();
        return;
    }

    static MainThreadNeverDestroyed<Vector<CompletionHandler<void()>, 1>> lookupCompletionHandlers;
    lookupCompletionHandlers->append(WTFMove(completionHandler));
    if (lookupCompletionHandlers->size() > 1)
        return;

    RetainPtr options = adoptNS([PAL::allocWPResourceRequestOptionsInstance() init]);
    [options setAfterUpdates:NO];

    [[PAL::getWPResourcesClass() sharedInstance] requestStorageAccessPromptQuirksData:options.get() completionHandler:^(WPStorageAccessPromptQuirksData *data, NSError *error) {
        Vector<WebCore::OrganizationStorageAccessPromptQuirk> result;
        if (error)
            RELEASE_LOG_ERROR(ResourceLoadStatistics, "Failed to request storage access quirks from WebPrivacy.");
        else {
            auto quirks = [data quirks];
            for (WPStorageAccessPromptQuirk *quirk : quirks)
                result.append(WebCore::OrganizationStorageAccessPromptQuirk { quirk.name, domainPairingsDictToMap(quirk.domainPairings) });
            setCachedQuirks(WTFMove(result));
        }

        for (auto& completionHandler : std::exchange(lookupCompletionHandlers.get(), { }))
            completionHandler();
    }];

}

#if HAVE(SYSTEM_SUPPORT_FOR_ADVANCED_PRIVACY_PROTECTIONS)

inline static std::optional<WebCore::IPAddress> ipAddress(const struct sockaddr* address)
{
    if (address->sa_family == AF_INET)
        return WebCore::IPAddress { reinterpret_cast<const sockaddr_in*>(address)->sin_addr };

    if (address->sa_family == AF_INET6)
        return WebCore::IPAddress { reinterpret_cast<const sockaddr_in6*>(address)->sin6_addr };

    return std::nullopt;
}

inline static std::optional<WebCore::IPAddress> ipAddress(nw_endpoint_t endpoint)
{
    if (nw_endpoint_get_type(endpoint) != nw_endpoint_type_address)
        return std::nullopt;

    return ipAddress(nw_endpoint_get_address(endpoint));
}

inline static std::optional<const char*> hostname(nw_endpoint_t endpoint)
{
    if (nw_endpoint_get_type(endpoint) != nw_endpoint_type_host)
        return std::nullopt;

    return nw_endpoint_get_hostname(endpoint);
}

class TrackerAddressLookupInfo {
public:
    enum class CanBlock : bool { No, Yes };

    TrackerAddressLookupInfo(WebCore::IPAddress&& network, unsigned netMaskLength, String&& owner, String&& host, CanBlock canBlock)
        : m_network { WTFMove(network) }
        , m_netMaskLength { netMaskLength }
        , m_owner { owner.utf8() }
        , m_host { host.utf8() }
        , m_canBlock { canBlock }
    {
    }

    TrackerAddressLookupInfo(WPNetworkAddressRange *range)
        : m_network { ipAddress(range.address).value() }
        , m_netMaskLength { static_cast<unsigned>(range.netMaskLength) }
        , m_owner { range.owner.UTF8String }
        , m_host { range.host.UTF8String }
        , m_canBlock { CanBlock::Yes } // FIXME: Grab this from WPNetworkAddressRange as well, once it's available.
    {
    }

    TrackerAddressLookupInfo() = default;

    const CString& owner() const { return m_owner; }
    const CString& host() const { return m_host; }

    CanBlock canBlock() const { return m_canBlock; }

    static void populateIfNeeded()
    {
        static std::once_flag onceFlag;
        std::call_once(onceFlag, [&] {
            if (!WebKit::canUseWebPrivacyFramework())
                return;

            auto options = adoptNS([PAL::allocWPResourceRequestOptionsInstance() init]);
            [options setAfterUpdates:YES];

            [[PAL::getWPResourcesClass() sharedInstance] requestTrackerNetworkAddresses:options.get() completionHandler:^(NSArray<WPNetworkAddressRange *> *ranges, NSError *error) {
                if (error) {
                    RELEASE_LOG_ERROR(ResourceLoadStatistics, "Failed to request tracking IP addresses from WebPrivacy");
                    return;
                }

                version4List().clear();
                version6List().clear();

                for (WPNetworkAddressRange *range in ranges) {
                    switch (range.version) {
                    case WPNetworkAddressVersion4:
                        version4List().append({ range });
                        break;
                    case WPNetworkAddressVersion6:
                        version6List().append({ range });
                        break;
                    default:
                        RELEASE_LOG_ERROR(ResourceLoadStatistics, "Skipped invalid tracking IP address: %@", range);
                        break;
                    }
                }

                version4List().shrinkToFit();
                version6List().shrinkToFit();
            }];
        });
    }

    static const TrackerAddressLookupInfo* find(const WebCore::IPAddress& address)
    {
        auto& list = address.isIPv4() ? version4List() : version6List();
        if (list.isEmpty())
            return nullptr;

        size_t lower = 0;
        size_t upper = list.size() - 1;
        if (address < list[lower].m_network)
            upper = lower;
        else if (address > list[upper].m_network)
            lower = upper;
        else {
            while (upper - lower > 1) {
                auto middle = (lower + upper) / 2;
                switch (address.compare(list[middle].m_network)) {
                case WebCore::IPAddress::ComparisonResult::Equal:
                    return &list[middle];
                case WebCore::IPAddress::ComparisonResult::Less:
                    upper = middle;
                    break;
                case WebCore::IPAddress::ComparisonResult::Greater:
                    lower = middle;
                    break;
                case WebCore::IPAddress::ComparisonResult::CannotCompare:
                    ASSERT_NOT_REACHED();
                    return nullptr;
                }
            }
        }

        if (list[upper].contains(address))
            return &list[upper];

        if (upper != lower && list[lower].contains(address))
            return &list[lower];

        return nullptr;
    }

private:
    static Vector<TrackerAddressLookupInfo>& version4List()
    {
        static NeverDestroyed sharedList = [] {
            return Vector<TrackerAddressLookupInfo> { };
        }();
        return sharedList.get();
    }

    static Vector<TrackerAddressLookupInfo>& version6List()
    {
        static NeverDestroyed sharedList = [] {
            return Vector<TrackerAddressLookupInfo> { };
        }();
        return sharedList.get();
    }

    bool contains(const WebCore::IPAddress& address) const
    {
        return m_network.matchingNetMaskLength(address) >= m_netMaskLength;
    }

    WebCore::IPAddress m_network { WTF::HashTableEmptyValue };
    unsigned m_netMaskLength { 0 };
    CString m_owner;
    CString m_host;
    CanBlock m_canBlock { CanBlock::No };
};

class TrackerDomainLookupInfo {
public:
    enum class CanBlock : bool { No, Yes };

    TrackerDomainLookupInfo(String&& owner, CanBlock canBlock)
        : m_owner { owner.utf8() }
        , m_canBlock { canBlock }
    {
    }

    TrackerDomainLookupInfo(WPTrackingDomain *domain)
        : m_owner { domain.owner.UTF8String }
        , m_canBlock { domain.canBlock ? CanBlock::Yes : CanBlock::No }
    {
    }

    TrackerDomainLookupInfo() = default;

    const CString& owner() const { return m_owner; }

    CanBlock canBlock() const { return m_canBlock; }

    static void populateIfNeeded()
    {
        static std::once_flag onceFlag;
        std::call_once(onceFlag, [&] {
            if (!WebKit::canUseWebPrivacyFramework())
                return;

            static BOOL canRequestTrackerDomainNames = [] {
                return [PAL::getWPResourcesClass() instancesRespondToSelector:@selector(requestTrackerDomainNamesData:completionHandler:)];
            }();

            if (!canRequestTrackerDomainNames)
                return;

            auto options = adoptNS([PAL::allocWPResourceRequestOptionsInstance() init]);
            [options setAfterUpdates:YES];
            [[PAL::getWPResourcesClass() sharedInstance] requestTrackerDomainNamesData:options.get() completionHandler:^(NSArray<WPTrackingDomain *> * domains, NSError *error) {
                if (error) {
                    RELEASE_LOG_ERROR(ResourceLoadStatistics, "Failed to request tracking domains from WebPrivacy");
                    return;
                }

                for (WPTrackingDomain *domain in domains)
                    list().set(String::fromLatin1([domain.host UTF8String]), TrackerDomainLookupInfo { domain });
            }];
        });
    }

    static const TrackerDomainLookupInfo find(String host)
    {
        if (!list().isValidKey(host))
            return { };
        return list().get(host);
    }

private:
    static MemoryCompactRobinHoodHashMap<String, TrackerDomainLookupInfo>& list()
    {
        static NeverDestroyed<MemoryCompactRobinHoodHashMap<String, TrackerDomainLookupInfo>> map;
        return map.get();
    }

    CString m_owner;
    CanBlock m_canBlock { CanBlock::No };
};

void configureForAdvancedPrivacyProtections(NSURLSession *session)
{
    static bool canSetTrackerLookupCallback = [&] {
        return [NSURLSession instancesRespondToSelector:@selector(_networkContext)];
    }();

    if (!canSetTrackerLookupCallback)
        return;

    auto context = session._networkContext;
    if (!context)
        return;

    TrackerAddressLookupInfo::populateIfNeeded();
    TrackerDomainLookupInfo::populateIfNeeded();

    auto* setTrackerLookupCallback = nw_context_set_tracker_lookup_callbackPtr();
    if (!setTrackerLookupCallback)
        return;

    setTrackerLookupCallback(context, ^(nw_endpoint_t endpoint, const char** hostName, const char** owner, bool* canBlock) {
        if (auto address = ipAddress(endpoint)) {
            if (auto* info = TrackerAddressLookupInfo::find(*address)) {
                *owner = info->owner().data();
                *hostName = info->host().data();
                *canBlock = info->canBlock() == TrackerAddressLookupInfo::CanBlock::Yes;
            }
        }

        if (auto host = hostname(endpoint)) {
            auto domain = WebCore::RegistrableDomain { URL { makeString("http://", String::fromLatin1(*host)) } };
            if (auto info = TrackerDomainLookupInfo::find(domain.string()); info.owner().length()) {
                *owner = info.owner().data();
                *hostName = *host;
                *canBlock = info.canBlock() == TrackerDomainLookupInfo::CanBlock::Yes;
            }
        }
    });
}

#else

void configureForAdvancedPrivacyProtections(NSURLSession *) { }

#endif

} // namespace WebKit

#endif // ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
