/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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
#import "NetworkProcess.h"

#import "CookieStorageUtilsCF.h"
#import "Logging.h"
#import "NetworkCache.h"
#import "NetworkProcessCreationParameters.h"
#import "NetworkResourceLoader.h"
#import "NetworkSessionCocoa.h"
#import "SandboxExtension.h"
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/PublicSuffix.h>
#import <WebCore/ResourceRequestCFNet.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/SecurityOrigin.h>
#import <WebCore/SecurityOriginData.h>
#import <WebCore/SocketStreamHandleImpl.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

namespace WebKit {

static void initializeNetworkSettings()
{
    static const unsigned preferredConnectionCount = 6;

    _CFNetworkHTTPConnectionCacheSetLimit(kHTTPLoadWidth, preferredConnectionCount);

    Boolean keyExistsAndHasValidFormat = false;
    Boolean prefValue = CFPreferencesGetAppBooleanValue(CFSTR("WebKitEnableHTTPPipelining"), kCFPreferencesCurrentApplication, &keyExistsAndHasValidFormat);
    if (keyExistsAndHasValidFormat)
        WebCore::ResourceRequest::setHTTPPipeliningEnabled(prefValue);

    if (WebCore::ResourceRequest::resourcePrioritiesEnabled()) {
        const unsigned fastLaneConnectionCount = 1;

        _CFNetworkHTTPConnectionCacheSetLimit(kHTTPPriorityNumLevels, WebCore::resourceLoadPriorityCount);
        _CFNetworkHTTPConnectionCacheSetLimit(kHTTPMinimumFastLanePriority, toPlatformRequestPriority(WebCore::ResourceLoadPriority::Medium));
        _CFNetworkHTTPConnectionCacheSetLimit(kHTTPNumFastLanes, fastLaneConnectionCount);
    }
}

void NetworkProcess::platformInitializeNetworkProcessCocoa(const NetworkProcessCreationParameters& parameters)
{
    WebCore::setApplicationBundleIdentifier(parameters.uiProcessBundleIdentifier);
    setApplicationSDKVersion(parameters.uiProcessSDKVersion);

#if PLATFORM(IOS_FAMILY)
    SandboxExtension::consumePermanently(parameters.cookieStorageDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.containerCachesDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.parentBundleDirectoryExtensionHandle);
#if ENABLE(INDEXED_DATABASE)
    SandboxExtension::consumePermanently(parameters.defaultDataStoreParameters.indexedDatabaseTempBlobDirectoryExtensionHandle);
#endif
#endif

    _CFNetworkSetATSContext(parameters.networkATSContext.get());

    m_uiProcessBundleIdentifier = parameters.uiProcessBundleIdentifier;
    
    initializeNetworkSettings();

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    setSharedHTTPCookieStorage(parameters.uiProcessCookieStorageIdentifier);
#endif

    // FIXME: Most of what this function does for cache size gets immediately overridden by setCacheModel().
    // - memory cache size passed from UI process is always ignored;
    // - disk cache size passed from UI process is effectively a minimum size.
    // One non-obvious constraint is that we need to use -setSharedURLCache: even in testing mode, to prevent creating a default one on disk later, when some other code touches the cache.

    m_cacheOptions = { NetworkCache::CacheOption::RegisterNotify };

    // Disable NSURLCache.
    auto urlCache(adoptNS([[NSURLCache alloc] initWithMemoryCapacity:0 diskCapacity:0 diskPath:nil]));
    [NSURLCache setSharedURLCache:urlCache.get()];
}

std::unique_ptr<WebCore::NetworkStorageSession> NetworkProcess::platformCreateDefaultStorageSession() const
{
    return makeUnique<WebCore::NetworkStorageSession>(PAL::SessionID::defaultSessionID());
}

RetainPtr<CFDataRef> NetworkProcess::sourceApplicationAuditData() const
{
#if USE(SOURCE_APPLICATION_AUDIT_DATA)
    ASSERT(parentProcessConnection());
    if (!parentProcessConnection())
        return nullptr;
    Optional<audit_token_t> auditToken = parentProcessConnection()->getAuditToken();
    if (!auditToken)
        return nullptr;
    return adoptCF(CFDataCreate(nullptr, (const UInt8*)&*auditToken, sizeof(*auditToken)));
#else
    return nullptr;
#endif
}

#if !HAVE(HSTS_STORAGE)
static void filterPreloadHSTSEntry(const void* key, const void* value, void* context)
{
    RELEASE_ASSERT(context);

    ASSERT(key);
    ASSERT(value);
    if (!key || !value)
        return;

    ASSERT(key != kCFNull);
    if (key == kCFNull)
        return;
    
    auto* hostnames = static_cast<HashSet<String>*>(context);
    auto val = static_cast<CFDictionaryRef>(value);
    if (CFDictionaryGetValue(val, _kCFNetworkHSTSPreloaded) != kCFBooleanTrue)
        hostnames->add((CFStringRef)key);
}
#endif

HashSet<String> NetworkProcess::hostNamesWithHSTSCache(PAL::SessionID sessionID) const
{
    HashSet<String> hostNames;
#if HAVE(HSTS_STORAGE)
    if (auto* networkSession = static_cast<NetworkSessionCocoa*>(this->networkSession(sessionID))) {
        for (NSString *host in networkSession->hstsStorage().nonPreloadedHosts)
            hostNames.add(host);
    }
#else
    if (auto* session = storageSession(sessionID)) {
        if (auto HSTSPolicies = adoptCF(_CFNetworkCopyHSTSPolicies(session->platformSession())))
            CFDictionaryApplyFunction(HSTSPolicies.get(), filterPreloadHSTSEntry, &hostNames);
    }
#endif
    return hostNames;
}

void NetworkProcess::deleteHSTSCacheForHostNames(PAL::SessionID sessionID, const Vector<String>& hostNames)
{
#if HAVE(HSTS_STORAGE)
    if (auto* networkSession = static_cast<NetworkSessionCocoa*>(this->networkSession(sessionID))) {
        for (auto& hostName : hostNames)
            [networkSession->hstsStorage() resetHSTSForHost:hostName];
    }
#else
    if (auto* session = storageSession(sessionID)) {
        for (auto& hostName : hostNames) {
            auto url = URL({ }, makeString("https://", hostName));
            _CFNetworkResetHSTS(url.createCFURL().get(), session->platformSession());
        }
    }
#endif
}

void NetworkProcess::clearHSTSCache(PAL::SessionID sessionID, WallTime modifiedSince)
{
    NSTimeInterval timeInterval = modifiedSince.secondsSinceEpoch().seconds();
    NSDate *date = [NSDate dateWithTimeIntervalSince1970:timeInterval];
#if HAVE(HSTS_STORAGE)
    if (auto* networkSession = static_cast<NetworkSessionCocoa*>(this->networkSession(sessionID)))
        [networkSession->hstsStorage() resetHSTSHostsSinceDate:date];
#else
    if (auto* session = storageSession(sessionID))
        _CFNetworkResetHSTSHostsSinceDate(session->platformSession(), (__bridge CFDateRef)date);
#endif
}

void NetworkProcess::clearDiskCache(WallTime modifiedSince, CompletionHandler<void()>&& completionHandler)
{
    if (!m_clearCacheDispatchGroup)
        m_clearCacheDispatchGroup = dispatch_group_create();

    auto group = m_clearCacheDispatchGroup;
    dispatch_group_async(group, dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = makeRef(*this), modifiedSince, completionHandler = WTFMove(completionHandler)] () mutable {
        auto aggregator = CallbackAggregator::create(WTFMove(completionHandler));
        forEachNetworkSession([modifiedSince, &aggregator](NetworkSession& session) {
            if (auto* cache = session.cache())
                cache->clear(modifiedSince, [aggregator] () { });
        });
    }).get());
}

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
void NetworkProcess::setSharedHTTPCookieStorage(const Vector<uint8_t>& identifier)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    [NSHTTPCookieStorage _setSharedHTTPCookieStorage:adoptNS([[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:cookieStorageFromIdentifyingData(identifier).get()]).get()];
}
#endif

void NetworkProcess::flushCookies(const PAL::SessionID& sessionID, CompletionHandler<void()>&& completionHandler)
{
    platformFlushCookies(sessionID, WTFMove(completionHandler));
}

#if HAVE(FOUNDATION_WITH_SAVE_COOKIES_WITH_COMPLETION_HANDLER)
static void saveCookies(NSHTTPCookieStorage *cookieStorage, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(cookieStorage);
    [cookieStorage _saveCookies:makeBlockPtr([completionHandler = WTFMove(completionHandler)]() mutable {
        // CFNetwork may call the completion block on a background queue, so we need to redispatch to the main thread.
        RunLoop::main().dispatch(WTFMove(completionHandler));
    }).get()];
}
#endif

void NetworkProcess::platformFlushCookies(const PAL::SessionID& sessionID, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
#if HAVE(FOUNDATION_WITH_SAVE_COOKIES_WITH_COMPLETION_HANDLER)
    if (auto* networkStorageSession = storageSession(sessionID))
        saveCookies(networkStorageSession->nsCookieStorage(), WTFMove(completionHandler));
    else
        completionHandler();
#else
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    _CFHTTPCookieStorageFlushCookieStores();
    ALLOW_DEPRECATED_DECLARATIONS_END
    completionHandler();
#endif
}

void NetworkProcess::platformProcessDidTransitionToBackground()
{
}

void NetworkProcess::platformProcessDidTransitionToForeground()
{
}

NetworkHTTPSUpgradeChecker& NetworkProcess::networkHTTPSUpgradeChecker()
{
    if (!m_networkHTTPSUpgradeChecker)
        m_networkHTTPSUpgradeChecker = makeUnique<NetworkHTTPSUpgradeChecker>();
    return *m_networkHTTPSUpgradeChecker;
}

} // namespace WebKit
