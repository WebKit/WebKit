/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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

#import "ArgumentCodersCocoa.h"
#import "CodeSigning.h"
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
#import <WebCore/PublicSuffixStore.h>
#import <WebCore/ResourceRequestCFNet.h>
#import <WebCore/SecurityOrigin.h>
#import <WebCore/SecurityOriginData.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/FileSystem.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/RetainPtr.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/cocoa/AuditToken.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/spi/darwin/SandboxSPI.h>

#if ENABLE(CONTENT_FILTERING)
#import <pal/spi/cocoa/NEFilterSourceSPI.h>
#endif

#if USE(EXTENSIONKIT)
#import "ExtensionKitSPI.h"
#import "WKProcessExtension.h"
#endif

#import <pal/spi/cocoa/NetworkSPI.h>

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
    m_isParentProcessFullWebBrowserOrRunningTest = parameters.isParentProcessFullWebBrowserOrRunningTest;

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

#if ENABLE(CONTENT_FILTERING)
    auto auditToken = protect(parentProcessConnection())->getAuditToken();
    ASSERT(auditToken);
    if (auditToken && [NEFilterSource respondsToSelector:@selector(setDelegation:)])
        [NEFilterSource setDelegation:&auditToken.value()];
#endif
    m_enableModernDownloadProgress = parameters.enableModernDownloadProgress;

#if ENABLE(DNS_SERVER_FOR_TESTING_IN_NETWORKING_PROCESS)
    // See TestController::cocoaPlatformInitialize for supporting a local DNS resolver when !ENABLE(TEST_DNS_SERVER_IN_NETWORKING_PROCESS).
    auto webPlatformTestDomain = "web-platform.test"_s;
    if (parameters.localhostAliasesForTesting.contains(webPlatformTestDomain)) {
        m_resolverConfig = adoptOSObject(nw_resolver_config_create());
        if (auto resolverConfig = m_resolverConfig) {
            nw_resolver_config_set_protocol(resolverConfig.get(), nw_resolver_protocol_dns53);
            nw_resolver_config_set_class(resolverConfig.get(), nw_resolver_class_designated_direct);
            nw_resolver_config_add_name_server(resolverConfig.get(), "127.0.0.1:8053");
            nw_resolver_config_add_match_domain(resolverConfig.get(), webPlatformTestDomain.characters());
            nw_privacy_context_require_encrypted_name_resolution(NW_DEFAULT_PRIVACY_CONTEXT, true, m_resolverConfig.get());
        }
    }
#endif // ENABLE(DNS_SERVER_FOR_TESTING_IN_NETWORKING_PROCESS)

#if ENABLE(INHERITANCE_OF_NETWORK_ACCESS_FROM_UI_PROCESS)
    if (auto auditToken = protect(parentProcessConnection())->getAuditToken()) {
        bool isNetworkAccessBlockedInUIProcess = (1 == sandbox_check_by_audit_token(*auditToken, "network-outbound", SANDBOX_FILTER_PATH, "/private/var/run/mDNSResponder"));

        OSObjectPtr xpcConnection = protect(parentProcessConnection())->xpcConnection();
        auto [signingIdentifier, isPlatformBinary] = codeSigningIdentifierAndPlatformBinaryStatus(xpcConnection.get());
        if (!isPlatformBinary && isNetworkAccessBlockedInUIProcess) {
            RELEASE_LOG(Process, "Setting sandbox state flag to block network access");
            if (auto auditTokenForSelf = WTF::auditTokenForSelf()) {
                if (!sandbox_enable_state_flag("BlockNetworkAccess", *auditTokenForSelf))
                    RELEASE_LOG_ERROR(Process, "Unable to set sandbox state flag to block network access");
            } else
                RELEASE_LOG_FAULT(Process, "Unable to get audit token to block network access");
        }
    } else
        RELEASE_LOG_FAULT(Process, "Unable to get audit token for UI process to block network access");
#endif

    increaseFileDescriptorLimit();
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
    return protect(parentProcessConnection())->getAuditToken();
#else
    return { };
#endif
}

HashSet<String> NetworkProcess::hostNamesWithHSTSCache(PAL::SessionID sessionID) const
{
    HashSet<String> hostNames;
    if (CheckedPtr networkSession = downcast<NetworkSessionCocoa>(this->networkSession(sessionID))) {
        for (NSString *host in protect(networkSession->hstsStorage()).get().nonPreloadedHosts)
            hostNames.add(host);
    }
    return hostNames;
}

void NetworkProcess::deleteHSTSCacheForHostNames(PAL::SessionID sessionID, const Vector<String>& hostNames)
{
    if (CheckedPtr networkSession = downcast<NetworkSessionCocoa>(this->networkSession(sessionID))) {
        for (auto& hostName : hostNames)
            [protect(networkSession->hstsStorage()).get() resetHSTSForHost:hostName.createNSString().get()];
    }
}

void NetworkProcess::clearHSTSCache(PAL::SessionID sessionID, WallTime modifiedSince)
{
    NSTimeInterval timeInterval = modifiedSince.secondsSinceEpoch().seconds();
    RetainPtr date = [NSDate dateWithTimeIntervalSince1970:timeInterval];
    if (CheckedPtr networkSession = downcast<NetworkSessionCocoa>(this->networkSession(sessionID)))
        [protect(networkSession->hstsStorage()).get() resetHSTSHostsSinceDate:date.get()];
}

void NetworkProcess::clearDiskCache(WallTime modifiedSince, CompletionHandler<void()>&& completionHandler)
{
    if (!m_clearCacheDispatchGroup)
        m_clearCacheDispatchGroup = adoptOSObject(dispatch_group_create());

    RetainPtr group = m_clearCacheDispatchGroup.get();
    dispatch_group_async(group.get(), mainDispatchQueueSingleton(), makeBlockPtr([this, protectedThis = Ref { *this }, modifiedSince, completionHandler = WTF::move(completionHandler)] () mutable {
        auto aggregator = CallbackAggregator::create(WTF::move(completionHandler));
        forEachNetworkSession([modifiedSince, &aggregator](NetworkSession& session) {
            if (RefPtr cache = session.cache())
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
    platformFlushCookies(sessionID, WTF::move(completionHandler));
}

void saveCookies(NSHTTPCookieStorage *cookieStorage, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(cookieStorage);
    [cookieStorage _saveCookies:makeBlockPtr([completionHandler = WTF::move(completionHandler)]() mutable {
        // CFNetwork may call the completion block on a background queue, so we need to redispatch to the main thread.
        RunLoop::mainSingleton().dispatch(WTF::move(completionHandler));
    }).get()];
}

void NetworkProcess::platformFlushCookies(PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    CheckedPtr networkStorageSession = storageSession(sessionID);
    if (!networkStorageSession)
        return completionHandler();

    RetainPtr cookieStorage = networkStorageSession->nsCookieStorage();
    saveCookies(cookieStorage.get(), WTF::move(completionHandler));
}

const String& NetworkProcess::uiProcessBundleIdentifier() const
{
    if (m_uiProcessBundleIdentifier.isNull())
        m_uiProcessBundleIdentifier = [[NSBundle mainBundle] bundleIdentifier];

    return m_uiProcessBundleIdentifier;
}

#if PLATFORM(IOS_FAMILY)
void NetworkProcess::setBackupExclusionPeriodForTesting(PAL::SessionID sessionID, Seconds period, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTF::move(completionHandler));
    if (CheckedPtr session = networkSession(sessionID))
        session->storageManager().setBackupExclusionPeriodForTesting(period, [callbackAggregator] { });
}
#endif // PLATFORM(IOS_FAMILY)

#if HAVE(NW_PROXY_CONFIG)
void NetworkProcess::clearProxyConfigData(PAL::SessionID sessionID)
{
    CheckedPtr session = networkSession(sessionID);
    if (!session)
        return;

    session->clearProxyConfigData();
}

void NetworkProcess::setProxyConfigData(PAL::SessionID sessionID, Vector<std::pair<Vector<uint8_t>, std::optional<WTF::UUID>>>&& proxyConfigurations)
{
    CheckedPtr session = networkSession(sessionID);
    if (!session)
        return;

    session->setProxyConfigData(WTF::move(proxyConfigurations));
}
#endif // HAVE(NW_PROXY_CONFIG)

} // namespace WebKit
