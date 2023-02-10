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
#import "NetworkStorageManager.h"
#import "SandboxExtension.h"
#import "WebCookieManager.h"
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
#import <wtf/FileSystem.h>
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
    _CFNetworkSetATSContext(parameters.networkATSContext.get());

    m_uiProcessBundleIdentifier = parameters.uiProcessBundleIdentifier;
    
    initializeNetworkSettings();

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    setSharedHTTPCookieStorage(parameters.uiProcessCookieStorageIdentifier);
#endif

    // Allow the network process to materialize files stored in the cloud so that loading/reading such files actually succeeds.
    FileSystem::setAllowsMaterializingDatalessFiles(true, FileSystem::PolicyScope::Process);

    // FIXME: Most of what this function does for cache size gets immediately overridden by setCacheModel().
    // - memory cache size passed from UI process is always ignored;
    // - disk cache size passed from UI process is effectively a minimum size.
    // One non-obvious constraint is that we need to use -setSharedURLCache: even in testing mode, to prevent creating a default one on disk later, when some other code touches the cache.

    m_cacheOptions = { NetworkCache::CacheOption::RegisterNotify };

    // Disable NSURLCache.
    auto urlCache(adoptNS([[NSURLCache alloc] initWithMemoryCapacity:0 diskCapacity:0 diskPath:nil]));
    [NSURLCache setSharedURLCache:urlCache.get()];
}

RetainPtr<CFDataRef> NetworkProcess::sourceApplicationAuditData() const
{
#if USE(SOURCE_APPLICATION_AUDIT_DATA)
    if (auto auditToken = sourceApplicationAuditToken())
        return adoptCF(CFDataCreate(nullptr, (const UInt8*)&*auditToken, sizeof(*auditToken)));
#endif

    return nullptr;
}

std::optional<audit_token_t> NetworkProcess::sourceApplicationAuditToken() const
{
#if USE(SOURCE_APPLICATION_AUDIT_DATA)
    ASSERT(parentProcessConnection());
    if (!parentProcessConnection())
        return { };
    return parentProcessConnection()->getAuditToken();
#else
    return { };
#endif
}

HashSet<String> NetworkProcess::hostNamesWithHSTSCache(PAL::SessionID sessionID) const
{
    HashSet<String> hostNames;
    if (auto* networkSession = static_cast<NetworkSessionCocoa*>(this->networkSession(sessionID))) {
        for (NSString *host in networkSession->hstsStorage().nonPreloadedHosts)
            hostNames.add(host);
    }
    return hostNames;
}

void NetworkProcess::deleteHSTSCacheForHostNames(PAL::SessionID sessionID, const Vector<String>& hostNames)
{
    if (auto* networkSession = static_cast<NetworkSessionCocoa*>(this->networkSession(sessionID))) {
        for (auto& hostName : hostNames)
            [networkSession->hstsStorage() resetHSTSForHost:hostName];
    }
}

void NetworkProcess::allowSpecificHTTPSCertificateForHost(PAL::SessionID, const WebCore::CertificateInfo& certificateInfo, const String& host)
{
    // FIXME: Remove this once rdar://30655740 is fixed.
    [NSURLRequest setAllowsSpecificHTTPSCertificate:(NSArray *)WebCore::CertificateInfo::certificateChainFromSecTrust(certificateInfo.trust().get()).get() forHost:host];
}

void NetworkProcess::clearHSTSCache(PAL::SessionID sessionID, WallTime modifiedSince)
{
    NSTimeInterval timeInterval = modifiedSince.secondsSinceEpoch().seconds();
    NSDate *date = [NSDate dateWithTimeIntervalSince1970:timeInterval];
    if (auto* networkSession = static_cast<NetworkSessionCocoa*>(this->networkSession(sessionID)))
        [networkSession->hstsStorage() resetHSTSHostsSinceDate:date];
}

void NetworkProcess::clearDiskCache(WallTime modifiedSince, CompletionHandler<void()>&& completionHandler)
{
    if (!m_clearCacheDispatchGroup)
        m_clearCacheDispatchGroup = adoptOSObject(dispatch_group_create());

    auto group = m_clearCacheDispatchGroup.get();
    dispatch_group_async(group, dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }, modifiedSince, completionHandler = WTFMove(completionHandler)] () mutable {
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

void NetworkProcess::flushCookies(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    platformFlushCookies(sessionID, WTFMove(completionHandler));
}

void saveCookies(NSHTTPCookieStorage *cookieStorage, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(cookieStorage);
    [cookieStorage _saveCookies:makeBlockPtr([completionHandler = WTFMove(completionHandler)]() mutable {
        // CFNetwork may call the completion block on a background queue, so we need to redispatch to the main thread.
        RunLoop::main().dispatch(WTFMove(completionHandler));
    }).get()];
}

void NetworkProcess::platformFlushCookies(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    if (auto* networkStorageSession = storageSession(sessionID))
        saveCookies(networkStorageSession->nsCookieStorage(), WTFMove(completionHandler));
    else
        completionHandler();
}

#if ENABLE(CFPREFS_DIRECT_MODE)
void NetworkProcess::notifyPreferencesChanged(const String& domain, const String& key, const std::optional<String>& encodedValue)
{
    preferenceDidUpdate(domain, key, encodedValue);
}
#endif

const String& NetworkProcess::uiProcessBundleIdentifier() const
{
    if (m_uiProcessBundleIdentifier.isNull())
        m_uiProcessBundleIdentifier = [[NSBundle mainBundle] bundleIdentifier];

    return m_uiProcessBundleIdentifier;
}

#if PLATFORM(IOS_FAMILY)

void NetworkProcess::setBackupExclusionPeriodForTesting(PAL::SessionID sessionID, Seconds period, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    if (auto* session = networkSession(sessionID))
        session->storageManager().setBackupExclusionPeriodForTesting(period, [callbackAggregator] { });
}

#endif

} // namespace WebKit
