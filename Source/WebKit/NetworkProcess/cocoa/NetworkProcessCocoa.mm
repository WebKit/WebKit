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
#import "SessionTracker.h"
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/PublicSuffix.h>
#import <WebCore/ResourceRequestCFNet.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/SecurityOrigin.h>
#import <WebCore/SecurityOriginData.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/ProcessPrivilege.h>

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/NetworkProcessCocoaAdditions.mm>
#endif

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

        _CFNetworkHTTPConnectionCacheSetLimit(kHTTPPriorityNumLevels, toPlatformRequestPriority(WebCore::ResourceLoadPriority::Highest));
        _CFNetworkHTTPConnectionCacheSetLimit(kHTTPMinimumFastLanePriority, toPlatformRequestPriority(WebCore::ResourceLoadPriority::Medium));
        _CFNetworkHTTPConnectionCacheSetLimit(kHTTPNumFastLanes, fastLaneConnectionCount);
    }
}

void NetworkProcess::platformInitializeNetworkProcessCocoa(const NetworkProcessCreationParameters& parameters)
{
    WebCore::setApplicationBundleIdentifier(parameters.uiProcessBundleIdentifier);

#if PLATFORM(IOS)
    SandboxExtension::consumePermanently(parameters.cookieStorageDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.containerCachesDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.parentBundleDirectoryExtensionHandle);
#endif
    m_diskCacheDirectory = parameters.diskCacheDirectory;

    _CFNetworkSetATSContext(parameters.networkATSContext.get());

    SessionTracker::setIdentifierBase(parameters.uiProcessBundleIdentifier);

    NetworkSessionCocoa::setSourceApplicationAuditTokenData(sourceApplicationAuditData());
    NetworkSessionCocoa::setSourceApplicationBundleIdentifier(parameters.sourceApplicationBundleIdentifier);
    NetworkSessionCocoa::setSourceApplicationSecondaryIdentifier(parameters.sourceApplicationSecondaryIdentifier);
#if PLATFORM(IOS)
    NetworkSessionCocoa::setCTDataConnectionServiceType(parameters.ctDataConnectionServiceType);
#endif

    initializeNetworkSettings();

#if PLATFORM(COCOA)
    setSharedHTTPCookieStorage(parameters.uiProcessCookieStorageIdentifier);
#endif

    WebCore::NetworkStorageSession::setCookieStoragePartitioningEnabled(parameters.cookieStoragePartitioningEnabled);
    WebCore::NetworkStorageSession::setStorageAccessAPIEnabled(parameters.storageAccessAPIEnabled);

    // FIXME: Most of what this function does for cache size gets immediately overridden by setCacheModel().
    // - memory cache size passed from UI process is always ignored;
    // - disk cache size passed from UI process is effectively a minimum size.
    // One non-obvious constraint is that we need to use -setSharedURLCache: even in testing mode, to prevent creating a default one on disk later, when some other code touches the cache.

    ASSERT(!m_diskCacheIsDisabledForTesting);

    if (!parameters.cacheStorageDirectory.isNull()) {
        m_cacheStorageDirectory = parameters.cacheStorageDirectory;
        m_cacheStoragePerOriginQuota = parameters.cacheStoragePerOriginQuota;
        SandboxExtension::consumePermanently(parameters.cacheStorageDirectoryExtensionHandle);
    }

#if ENABLE(WIFI_ASSERTIONS)
    initializeWiFiAssertions(parameters);
#endif

    if (m_diskCacheDirectory.isNull())
        return;

    SandboxExtension::consumePermanently(parameters.diskCacheDirectoryExtensionHandle);
    OptionSet<NetworkCache::Cache::Option> cacheOptions { NetworkCache::Cache::Option::RegisterNotify };
    if (parameters.shouldEnableNetworkCacheEfficacyLogging)
        cacheOptions |= NetworkCache::Cache::Option::EfficacyLogging;
    if (parameters.shouldUseTestingNetworkSession)
        cacheOptions |= NetworkCache::Cache::Option::TestingMode;
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    if (parameters.shouldEnableNetworkCacheSpeculativeRevalidation)
        cacheOptions |= NetworkCache::Cache::Option::SpeculativeRevalidation;
#endif

    m_cache = NetworkCache::Cache::open(m_diskCacheDirectory, cacheOptions);
    if (!m_cache)
        RELEASE_LOG_ERROR(NetworkCache, "Failed to initialize the WebKit network disk cache");

    // Disable NSURLCache.
    auto urlCache(adoptNS([[NSURLCache alloc] initWithMemoryCapacity:0 diskCapacity:0 diskPath:nil]));
    [NSURLCache setSharedURLCache:urlCache.get()];
    return;
}

RetainPtr<CFDataRef> NetworkProcess::sourceApplicationAuditData() const
{
#if PLATFORM(IOS) && !ENABLE(MINIMAL_SIMULATOR)
    audit_token_t auditToken;
    ASSERT(parentProcessConnection());
    if (!parentProcessConnection() || !parentProcessConnection()->getAuditToken(auditToken))
        return nullptr;
    return adoptCF(CFDataCreate(nullptr, (const UInt8*)&auditToken, sizeof(auditToken)));
#else
    return nullptr;
#endif
}

void NetworkProcess::clearHSTSCache(WebCore::NetworkStorageSession& session, WallTime modifiedSince)
{
    NSTimeInterval timeInterval = modifiedSince.secondsSinceEpoch().seconds();
    NSDate *date = [NSDate dateWithTimeIntervalSince1970:timeInterval];

    _CFNetworkResetHSTSHostsSinceDate(session.platformSession(), (__bridge CFDateRef)date);
}

void NetworkProcess::clearDiskCache(WallTime modifiedSince, Function<void ()>&& completionHandler)
{
    if (!m_clearCacheDispatchGroup)
        m_clearCacheDispatchGroup = dispatch_group_create();

    if (auto* cache = NetworkProcess::singleton().cache()) {
        auto group = m_clearCacheDispatchGroup;
        dispatch_group_async(group, dispatch_get_main_queue(), BlockPtr<void()>::fromCallable([cache, modifiedSince, completionHandler = WTFMove(completionHandler)] () mutable {
            cache->clear(modifiedSince, [completionHandler = WTFMove(completionHandler)] () mutable {
            });
        }).get());
    }
}

#if PLATFORM(COCOA)
void NetworkProcess::setSharedHTTPCookieStorage(const Vector<uint8_t>& identifier)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    [NSHTTPCookieStorage _setSharedHTTPCookieStorage:adoptNS([[NSHTTPCookieStorage alloc] _initWithCFHTTPCookieStorage:cookieStorageFromIdentifyingData(identifier).get()]).get()];
}
#endif

void NetworkProcess::setCookieStoragePartitioningEnabled(bool enabled)
{
    WebCore::NetworkStorageSession::setCookieStoragePartitioningEnabled(enabled);
}

void NetworkProcess::setStorageAccessAPIEnabled(bool enabled)
{
    WebCore::NetworkStorageSession::setStorageAccessAPIEnabled(enabled);
}

void NetworkProcess::syncAllCookies()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000)
    RefPtr<CallbackAggregator> callbackAggregator = CallbackAggregator::create([this] {
        didSyncAllCookies();
    });
    WebCore::NetworkStorageSession::forEach([&] (auto& networkStorageSession) {
        [networkStorageSession.nsCookieStorage() _saveCookies:[callbackAggregator] { }];
    });
#else
    _CFHTTPCookieStorageFlushCookieStores();
    didSyncAllCookies();
#endif

#pragma clang diagnostic pop
}

void NetworkProcess::platformPrepareToSuspend()
{
#if ENABLE(WIFI_ASSERTIONS)
    suspendWiFiAssertions();
#endif
}

void NetworkProcess::platformProcessDidResume()
{
#if ENABLE(WIFI_ASSERTIONS)
    resumeWiFiAssertions();
#endif
}

}
