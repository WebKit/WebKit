/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "NetworkStorageSession.h"

#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ProcessID.h>

#if PLATFORM(COCOA)
#include "PublicSuffix.h"
#include "ResourceRequest.h"
#include "WebCoreSystemInterface.h"
#else
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#endif

namespace WebCore {

static bool cookieStoragePartitioningEnabled;

static RetainPtr<CFURLStorageSessionRef> createCFStorageSessionForIdentifier(CFStringRef identifier)
{
    auto storageSession = adoptCF(_CFURLStorageSessionCreate(kCFAllocatorDefault, identifier, nullptr));

    if (!storageSession)
        return nullptr;

    auto cache = adoptCF(_CFURLStorageSessionCopyCache(kCFAllocatorDefault, storageSession.get()));
    if (!cache)
        return nullptr;

    CFURLCacheSetDiskCapacity(cache.get(), 0);

    auto sharedCache = adoptCF(CFURLCacheCopySharedURLCache());
    CFURLCacheSetMemoryCapacity(cache.get(), CFURLCacheMemoryCapacity(sharedCache.get()));

    auto cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, storageSession.get()));
    if (!cookieStorage)
        return nullptr;

    auto sharedCookieStorage = _CFHTTPCookieStorageGetDefault(kCFAllocatorDefault);
    auto sharedPolicy = CFHTTPCookieStorageGetCookieAcceptPolicy(sharedCookieStorage);
    CFHTTPCookieStorageSetCookieAcceptPolicy(cookieStorage.get(), sharedPolicy);

    return storageSession;
}

NetworkStorageSession::NetworkStorageSession(SessionID sessionID, RetainPtr<CFURLStorageSessionRef>&& platformSession, RetainPtr<CFHTTPCookieStorageRef>&& platformCookieStorage)
    : m_sessionID(sessionID)
    , m_platformSession(WTFMove(platformSession))
{
    m_platformCookieStorage = platformCookieStorage ? WTFMove(platformCookieStorage) : cookieStorage();
}

static std::unique_ptr<NetworkStorageSession>& defaultNetworkStorageSession()
{
    ASSERT(isMainThread());
    static NeverDestroyed<std::unique_ptr<NetworkStorageSession>> session;
    return session;
}

void NetworkStorageSession::switchToNewTestingSession()
{
    // Session name should be short enough for shared memory region name to be under the limit, otehrwise sandbox rules won't work (see <rdar://problem/13642852>).
    String sessionName = String::format("WebKit Test-%u", static_cast<uint32_t>(getCurrentProcessID()));

    RetainPtr<CFURLStorageSessionRef> session;
#if PLATFORM(COCOA)
    session = adoptCF(wkCreatePrivateStorageSession(sessionName.createCFString().get()));
#else
    session = adoptCF(wkCreatePrivateStorageSession(sessionName.createCFString().get(), defaultStorageSession().platformSession()));
#endif

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage;
    if (session)
        cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, session.get()));

    defaultNetworkStorageSession() = std::make_unique<NetworkStorageSession>(SessionID::defaultSessionID(), WTFMove(session), WTFMove(cookieStorage));
}

NetworkStorageSession& NetworkStorageSession::defaultStorageSession()
{
    if (!defaultNetworkStorageSession())
        defaultNetworkStorageSession() = std::make_unique<NetworkStorageSession>(SessionID::defaultSessionID(), nullptr, nullptr);
    return *defaultNetworkStorageSession();
}

void NetworkStorageSession::ensurePrivateBrowsingSession(SessionID sessionID, const String& identifierBase)
{
    ASSERT(sessionID.isEphemeral());
    ensureSession(sessionID, identifierBase);
}

void NetworkStorageSession::ensureSession(SessionID sessionID, const String& identifierBase, RetainPtr<CFHTTPCookieStorageRef>&& cookieStorage)
{
    auto addResult = globalSessionMap().add(sessionID, nullptr);
    if (!addResult.isNewEntry)
        return;

    RetainPtr<CFStringRef> cfIdentifier = String(identifierBase + ".PrivateBrowsing").createCFString();

    RetainPtr<CFURLStorageSessionRef> storageSession;
    if (sessionID.isEphemeral()) {
#if PLATFORM(COCOA)
        storageSession = adoptCF(wkCreatePrivateStorageSession(cfIdentifier.get()));
#else
        storageSession = adoptCF(wkCreatePrivateStorageSession(cfIdentifier.get(), defaultNetworkStorageSession()->platformSession()));
#endif
    } else
        storageSession = createCFStorageSessionForIdentifier(cfIdentifier.get());

    if (!cookieStorage && storageSession)
        cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, storageSession.get()));

    addResult.iterator->value = std::make_unique<NetworkStorageSession>(sessionID, WTFMove(storageSession), WTFMove(cookieStorage));
}

void NetworkStorageSession::ensureSession(SessionID sessionID, const String& identifierBase)
{
    ensureSession(sessionID, identifierBase, nullptr);
}

RetainPtr<CFHTTPCookieStorageRef> NetworkStorageSession::cookieStorage() const
{
    if (m_platformCookieStorage)
        return m_platformCookieStorage;

    if (m_platformSession)
        return adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, m_platformSession.get()));

#if USE(CFURLCONNECTION)
    return _CFHTTPCookieStorageGetDefault(kCFAllocatorDefault);
#else
    // When using NSURLConnection, we also use its shared cookie storage.
    return nullptr;
#endif
}

void NetworkStorageSession::setCookieStoragePartitioningEnabled(bool enabled)
{
    cookieStoragePartitioningEnabled = enabled;
}

#if HAVE(CFNETWORK_STORAGE_PARTITIONING)

String NetworkStorageSession::cookieStoragePartition(const ResourceRequest& request) const
{
    return cookieStoragePartition(request.firstPartyForCookies(), request.url());
}

static inline String getPartitioningDomain(const URL& url) 
{
#if ENABLE(PUBLIC_SUFFIX_LIST)
    auto domain = topPrivatelyControlledDomain(url.host());
    if (domain.isEmpty())
        domain = url.host();
#else
    auto domain = url.host();
#endif
    return domain;
}

String NetworkStorageSession::cookieStoragePartition(const URL& firstPartyForCookies, const URL& resource) const
{
    if (!cookieStoragePartitioningEnabled)
        return emptyString();
    
    auto resourceDomain = getPartitioningDomain(resource);
    if (!shouldPartitionCookies(resourceDomain))
        return emptyString();

    auto firstPartyDomain = getPartitioningDomain(firstPartyForCookies);
    if (firstPartyDomain == resourceDomain)
        return emptyString();

    return firstPartyDomain;
}

bool NetworkStorageSession::shouldPartitionCookies(const String& topPrivatelyControlledDomain) const
{
    if (topPrivatelyControlledDomain.isEmpty())
        return false;

    return m_topPrivatelyControlledDomainsForCookiePartitioning.contains(topPrivatelyControlledDomain);
}

void NetworkStorageSession::setShouldPartitionCookiesForHosts(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst)
{
    if (clearFirst)
        m_topPrivatelyControlledDomainsForCookiePartitioning.clear();

    if (!domainsToRemove.isEmpty()) {
        for (auto& domain : domainsToRemove)
            m_topPrivatelyControlledDomainsForCookiePartitioning.remove(domain);
    }
        
    if (!domainsToAdd.isEmpty())
        m_topPrivatelyControlledDomainsForCookiePartitioning.add(domainsToAdd.begin(), domainsToAdd.end());
}

#endif // HAVE(CFNETWORK_STORAGE_PARTITIONING)

#if !PLATFORM(COCOA)
void NetworkStorageSession::setCookies(const Vector<Cookie>&, const URL&, const URL&)
{
    // FIXME: Implement this. <https://webkit.org/b/156298>
}
#endif

}
