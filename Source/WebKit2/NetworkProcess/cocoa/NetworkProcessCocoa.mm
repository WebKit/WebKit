/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#import "NetworkCache.h"
#import "NetworkProcessCreationParameters.h"
#import "NetworkResourceLoader.h"
#import "NetworkSessionCocoa.h"
#import "SandboxExtension.h"
#import "SessionTracker.h"
#import <WebCore/CFNetworkSPI.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/PublicSuffix.h>
#import <WebCore/ResourceRequestCFNet.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/SecurityOrigin.h>
#import <WebCore/SecurityOriginData.h>
#import <WebKitSystemInterface.h>

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

#if PLATFORM(IOS) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
    _CFNetworkSetATSContext(parameters.networkATSContext.get());
#endif

    SessionTracker::setIdentifierBase(parameters.uiProcessBundleIdentifier);

#if USE(NETWORK_SESSION)
    NetworkSessionCocoa::setSourceApplicationAuditTokenData(sourceApplicationAuditData());
    NetworkSessionCocoa::setSourceApplicationBundleIdentifier(parameters.sourceApplicationBundleIdentifier);
    NetworkSessionCocoa::setSourceApplicationSecondaryIdentifier(parameters.sourceApplicationSecondaryIdentifier);
#if PLATFORM(IOS)
    NetworkSessionCocoa::setCTDataConnectionServiceType(parameters.ctDataConnectionServiceType);
#endif
#endif

    initializeNetworkSettings();

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    setSharedHTTPCookieStorage(parameters.uiProcessCookieStorageIdentifier);
#endif

    WebCore::NetworkStorageSession::setCookieStoragePartitioningEnabled(parameters.cookieStoragePartitioningEnabled);

    // FIXME: Most of what this function does for cache size gets immediately overridden by setCacheModel().
    // - memory cache size passed from UI process is always ignored;
    // - disk cache size passed from UI process is effectively a minimum size.
    // One non-obvious constraint is that we need to use -setSharedURLCache: even in testing mode, to prevent creating a default one on disk later, when some other code touches the cache.

    ASSERT(!m_diskCacheIsDisabledForTesting || !parameters.nsURLCacheDiskCapacity);

    if (!m_diskCacheDirectory.isNull()) {
        SandboxExtension::consumePermanently(parameters.diskCacheDirectoryExtensionHandle);
#if ENABLE(NETWORK_CACHE)
        if (parameters.shouldEnableNetworkCache) {
            NetworkCache::Cache::Parameters cacheParameters = {
                parameters.shouldEnableNetworkCacheEfficacyLogging
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
                , parameters.shouldEnableNetworkCacheSpeculativeRevalidation
#endif
            };
            if (NetworkCache::singleton().initialize(m_diskCacheDirectory, cacheParameters)) {
                auto urlCache(adoptNS([[NSURLCache alloc] initWithMemoryCapacity:0 diskCapacity:0 diskPath:nil]));
                [NSURLCache setSharedURLCache:urlCache.get()];
                return;
            }
        }
#endif
        String nsURLCacheDirectory = m_diskCacheDirectory;
#if PLATFORM(IOS)
        // NSURLCache path is relative to network process cache directory.
        // This puts cache files under <container>/Library/Caches/com.apple.WebKit.Networking/
        nsURLCacheDirectory = ".";
#endif
        [NSURLCache setSharedURLCache:adoptNS([[NSURLCache alloc]
            initWithMemoryCapacity:parameters.nsURLCacheMemoryCapacity
            diskCapacity:parameters.nsURLCacheDiskCapacity
            diskPath:nsURLCacheDirectory]).get()];
    }
}

void NetworkProcess::platformSetURLCacheSize(unsigned urlCacheMemoryCapacity, uint64_t urlCacheDiskCapacity)
{
    NSURLCache *nsurlCache = [NSURLCache sharedURLCache];
    [nsurlCache setMemoryCapacity:urlCacheMemoryCapacity];
    if (!m_diskCacheIsDisabledForTesting)
        [nsurlCache setDiskCapacity:std::max<uint64_t>(urlCacheDiskCapacity, [nsurlCache diskCapacity])]; // Don't shrink a big disk cache, since that would cause churn.
}

RetainPtr<CFDataRef> NetworkProcess::sourceApplicationAuditData() const
{
#if PLATFORM(IOS)
    audit_token_t auditToken;
    ASSERT(parentProcessConnection());
    if (!parentProcessConnection() || !parentProcessConnection()->getAuditToken(auditToken))
        return nullptr;
    return adoptCF(CFDataCreate(nullptr, (const UInt8*)&auditToken, sizeof(auditToken)));
#else
    return nullptr;
#endif
}

void NetworkProcess::clearHSTSCache(WebCore::NetworkStorageSession& session, std::chrono::system_clock::time_point modifiedSince)
{
    NSTimeInterval timeInterval = std::chrono::duration_cast<std::chrono::duration<double>>(modifiedSince.time_since_epoch()).count();
    NSDate *date = [NSDate dateWithTimeIntervalSince1970:timeInterval];

    _CFNetworkResetHSTSHostsSinceDate(session.platformSession(), (__bridge CFDateRef)date);
}

static void clearNSURLCache(dispatch_group_t group, std::chrono::system_clock::time_point modifiedSince, const std::function<void ()>& completionHandler)
{
    dispatch_group_async(group, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), [modifiedSince, completionHandler] {
        NSURLCache *cache = [NSURLCache sharedURLCache];

        NSTimeInterval timeInterval = std::chrono::duration_cast<std::chrono::duration<double>>(modifiedSince.time_since_epoch()).count();
        NSDate *date = [NSDate dateWithTimeIntervalSince1970:timeInterval];
        [cache removeCachedResponsesSinceDate:date];

        dispatch_async(dispatch_get_main_queue(), [completionHandler] {
            completionHandler();
        });
    });
}

void NetworkProcess::clearDiskCache(std::chrono::system_clock::time_point modifiedSince, std::function<void ()> completionHandler)
{
    if (!m_clearCacheDispatchGroup)
        m_clearCacheDispatchGroup = dispatch_group_create();

#if ENABLE(NETWORK_CACHE)
    auto group = m_clearCacheDispatchGroup;
    dispatch_group_async(group, dispatch_get_main_queue(), [group, modifiedSince, completionHandler] {
        NetworkCache::singleton().clear(modifiedSince, [group, modifiedSince, completionHandler] {
            // FIXME: Probably not necessary.
            clearNSURLCache(group, modifiedSince, completionHandler);
        });
    });
#else
    clearNSURLCache(m_clearCacheDispatchGroup, modifiedSince, completionHandler);
#endif
}

void NetworkProcess::setCookieStoragePartitioningEnabled(bool enabled)
{
    WebCore::NetworkStorageSession::setCookieStoragePartitioningEnabled(enabled);
}

}
