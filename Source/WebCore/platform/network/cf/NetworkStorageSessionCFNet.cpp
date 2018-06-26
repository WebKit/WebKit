/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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
#include <wtf/ProcessPrivilege.h>

#if PLATFORM(COCOA)
#include "PublicSuffix.h"
#include "ResourceRequest.h"
#else
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#endif

namespace WebCore {

static bool cookieStoragePartitioningEnabled;
static bool storageAccessAPIEnabled;

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

    if (!NetworkStorageSession::processMayUseCookieAPI())
        return storageSession;

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    auto cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, storageSession.get()));
    if (!cookieStorage)
        return nullptr;

    auto sharedCookieStorage = _CFHTTPCookieStorageGetDefault(kCFAllocatorDefault);
    auto sharedPolicy = CFHTTPCookieStorageGetCookieAcceptPolicy(sharedCookieStorage);
    CFHTTPCookieStorageSetCookieAcceptPolicy(cookieStorage.get(), sharedPolicy);

    return storageSession;
}

NetworkStorageSession::NetworkStorageSession(PAL::SessionID sessionID, RetainPtr<CFURLStorageSessionRef>&& platformSession, RetainPtr<CFHTTPCookieStorageRef>&& platformCookieStorage)
    : m_sessionID(sessionID)
    , m_platformSession(WTFMove(platformSession))
{
    ASSERT(processMayUseCookieAPI() || !platformCookieStorage);
    m_platformCookieStorage = platformCookieStorage ? WTFMove(platformCookieStorage) : cookieStorage();
}

NetworkStorageSession::NetworkStorageSession(PAL::SessionID sessionID)
    : m_sessionID(sessionID)
{
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
    session = adoptCF(createPrivateStorageSession(sessionName.createCFString().get()));
#else
    session = adoptCF(wkCreatePrivateStorageSession(sessionName.createCFString().get(), defaultStorageSession().platformSession()));
#endif

    RetainPtr<CFHTTPCookieStorageRef> cookieStorage;
    if (NetworkStorageSession::processMayUseCookieAPI()) {
        ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
        if (session)
            cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, session.get()));
    }

    defaultNetworkStorageSession() = std::make_unique<NetworkStorageSession>(PAL::SessionID::defaultSessionID(), WTFMove(session), WTFMove(cookieStorage));
}

NetworkStorageSession& NetworkStorageSession::defaultStorageSession()
{
    if (!defaultNetworkStorageSession())
        defaultNetworkStorageSession() = std::make_unique<NetworkStorageSession>(PAL::SessionID::defaultSessionID());
    return *defaultNetworkStorageSession();
}

void NetworkStorageSession::ensureSession(PAL::SessionID sessionID, const String& identifierBase, RetainPtr<CFHTTPCookieStorageRef>&& cookieStorage)
{
    auto addResult = globalSessionMap().add(sessionID, nullptr);
    if (!addResult.isNewEntry)
        return;

    RetainPtr<CFStringRef> cfIdentifier = String(identifierBase + ".PrivateBrowsing").createCFString();

    RetainPtr<CFURLStorageSessionRef> storageSession;
    if (sessionID.isEphemeral()) {
#if PLATFORM(COCOA)
        storageSession = adoptCF(createPrivateStorageSession(cfIdentifier.get()));
#else
        storageSession = adoptCF(wkCreatePrivateStorageSession(cfIdentifier.get(), defaultNetworkStorageSession()->platformSession()));
#endif
    } else
        storageSession = createCFStorageSessionForIdentifier(cfIdentifier.get());

    if (NetworkStorageSession::processMayUseCookieAPI()) {
        ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
        if (!cookieStorage && storageSession)
            cookieStorage = adoptCF(_CFURLStorageSessionCopyCookieStorage(kCFAllocatorDefault, storageSession.get()));
    }

    addResult.iterator->value = std::make_unique<NetworkStorageSession>(sessionID, WTFMove(storageSession), WTFMove(cookieStorage));
}

void NetworkStorageSession::ensureSession(PAL::SessionID sessionID, const String& identifierBase)
{
    ensureSession(sessionID, identifierBase, nullptr);
}

RetainPtr<CFHTTPCookieStorageRef> NetworkStorageSession::cookieStorage() const
{
    if (!processMayUseCookieAPI())
        return nullptr;

    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

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

void NetworkStorageSession::setStorageAccessAPIEnabled(bool enabled)
{
    storageAccessAPIEnabled = enabled;
}

#if HAVE(CFNETWORK_STORAGE_PARTITIONING)

String NetworkStorageSession::cookieStoragePartition(const ResourceRequest& request, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID) const
{
    return cookieStoragePartition(request.firstPartyForCookies(), request.url(), frameID, pageID);
}

static inline String getPartitioningDomain(const URL& url) 
{
#if ENABLE(PUBLIC_SUFFIX_LIST)
    auto domain = topPrivatelyControlledDomain(url.host().toString());
    if (domain.isEmpty())
        domain = url.host().toString();
#else
    auto domain = url.host().toString();
#endif
    return domain;
}

String NetworkStorageSession::cookieStoragePartition(const URL& firstPartyForCookies, const URL& resource, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID) const
{
    if (!cookieStoragePartitioningEnabled)
        return emptyString();
    
    auto resourceDomain = getPartitioningDomain(resource);
    if (!shouldPartitionCookies(resourceDomain))
        return emptyString();

    auto firstPartyDomain = getPartitioningDomain(firstPartyForCookies);
    if (firstPartyDomain == resourceDomain)
        return emptyString();

    if (pageID && hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID.value()))
        return emptyString();

    return firstPartyDomain;
}

bool NetworkStorageSession::shouldPartitionCookies(const String& topPrivatelyControlledDomain) const
{
    if (topPrivatelyControlledDomain.isEmpty())
        return false;

    return m_topPrivatelyControlledDomainsToPartition.contains(topPrivatelyControlledDomain);
}

bool NetworkStorageSession::shouldBlockThirdPartyCookies(const String& topPrivatelyControlledDomain) const
{
    if (topPrivatelyControlledDomain.isEmpty())
        return false;

    return m_topPrivatelyControlledDomainsToBlock.contains(topPrivatelyControlledDomain);
}

bool NetworkStorageSession::shouldBlockCookies(const ResourceRequest& request) const
{
    if (!cookieStoragePartitioningEnabled)
        return false;

    return shouldBlockCookies(request.firstPartyForCookies(), request.url());
}
    
bool NetworkStorageSession::shouldBlockCookies(const URL& firstPartyForCookies, const URL& resource) const
{
    if (!cookieStoragePartitioningEnabled)
        return false;
    
    auto firstPartyDomain = getPartitioningDomain(firstPartyForCookies);
    if (firstPartyDomain.isEmpty())
        return false;

    auto resourceDomain = getPartitioningDomain(resource);
    if (resourceDomain.isEmpty())
        return false;

    if (firstPartyDomain == resourceDomain)
        return false;

    return shouldBlockThirdPartyCookies(resourceDomain);
}

void NetworkStorageSession::setPrevalentDomainsToPartitionOrBlockCookies(const Vector<String>& domainsToPartition, const Vector<String>& domainsToBlock, const Vector<String>& domainsToNeitherPartitionNorBlock, bool clearFirst)
{
    if (clearFirst) {
        m_topPrivatelyControlledDomainsToPartition.clear();
        m_topPrivatelyControlledDomainsToBlock.clear();
        m_framesGrantedStorageAccess.clear();
    }

    for (auto& domain : domainsToPartition) {
        m_topPrivatelyControlledDomainsToPartition.add(domain);
        if (!clearFirst)
            m_topPrivatelyControlledDomainsToBlock.remove(domain);
    }

    for (auto& domain : domainsToBlock) {
        m_topPrivatelyControlledDomainsToBlock.add(domain);
        if (!clearFirst)
            m_topPrivatelyControlledDomainsToPartition.remove(domain);
    }
    
    if (!clearFirst) {
        for (auto& domain : domainsToNeitherPartitionNorBlock) {
            m_topPrivatelyControlledDomainsToPartition.remove(domain);
            m_topPrivatelyControlledDomainsToBlock.remove(domain);
        }
    }
}

void NetworkStorageSession::removePrevalentDomains(const Vector<String>& domains)
{
    for (auto& domain : domains) {
        m_topPrivatelyControlledDomainsToPartition.remove(domain);
        m_topPrivatelyControlledDomainsToBlock.remove(domain);
    }
}

bool NetworkStorageSession::hasStorageAccess(const String& resourceDomain, const String& firstPartyDomain, std::optional<uint64_t> frameID, uint64_t pageID) const
{
    if (frameID) {
        auto framesGrantedIterator = m_framesGrantedStorageAccess.find(pageID);
        if (framesGrantedIterator != m_framesGrantedStorageAccess.end()) {
            auto it = framesGrantedIterator->value.find(frameID.value());
            if (it != framesGrantedIterator->value.end() && it->value == resourceDomain)
                return true;
        }
    }

    if (!firstPartyDomain.isEmpty()) {
        auto pagesGrantedIterator = m_pagesGrantedStorageAccess.find(pageID);
        if (pagesGrantedIterator != m_pagesGrantedStorageAccess.end()) {
            auto it = pagesGrantedIterator->value.find(firstPartyDomain);
            if (it != pagesGrantedIterator->value.end() && it->value == resourceDomain)
                return true;
        }
    }

    return false;
}

Vector<String> NetworkStorageSession::getAllStorageAccessEntries() const
{
    Vector<String> entries;
    for (auto& innerMap : m_framesGrantedStorageAccess.values()) {
        for (auto& value : innerMap.values())
            entries.append(value);
    }
    return entries;
}
    
void NetworkStorageSession::grantStorageAccess(const String& resourceDomain, const String& firstPartyDomain, std::optional<uint64_t> frameID, uint64_t pageID)
{
    if (!frameID) {
        if (firstPartyDomain.isEmpty())
            return;
        auto pagesGrantedIterator = m_pagesGrantedStorageAccess.find(pageID);
        if (pagesGrantedIterator == m_pagesGrantedStorageAccess.end()) {
            HashMap<String, String> entry;
            entry.add(firstPartyDomain, resourceDomain);
            m_pagesGrantedStorageAccess.add(pageID, entry);
        } else {
            auto firstPartyDomainIterator = pagesGrantedIterator->value.find(firstPartyDomain);
            if (firstPartyDomainIterator == pagesGrantedIterator->value.end())
                pagesGrantedIterator->value.add(firstPartyDomain, resourceDomain);
            else
                firstPartyDomainIterator->value = resourceDomain;
        }
        return;
    }

    auto pagesGrantedIterator = m_framesGrantedStorageAccess.find(pageID);
    if (pagesGrantedIterator == m_framesGrantedStorageAccess.end()) {
        HashMap<uint64_t, String, DefaultHash<uint64_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> entry;
        entry.add(frameID.value(), resourceDomain);
        m_framesGrantedStorageAccess.add(pageID, entry);
    } else {
        auto framesGrantedIterator = pagesGrantedIterator->value.find(frameID.value());
        if (framesGrantedIterator == pagesGrantedIterator->value.end())
            pagesGrantedIterator->value.add(frameID.value(), resourceDomain);
        else
            framesGrantedIterator->value = resourceDomain;
    }
}

void NetworkStorageSession::removeStorageAccessForFrame(uint64_t frameID, uint64_t pageID)
{
    auto iteration = m_framesGrantedStorageAccess.find(pageID);
    if (iteration == m_framesGrantedStorageAccess.end())
        return;

    iteration->value.remove(frameID);
}

void NetworkStorageSession::removeStorageAccessForAllFramesOnPage(uint64_t pageID)
{
    m_pagesGrantedStorageAccess.remove(pageID);
    m_framesGrantedStorageAccess.remove(pageID);
}

void NetworkStorageSession::removeAllStorageAccess()
{
    m_pagesGrantedStorageAccess.clear();
    m_framesGrantedStorageAccess.clear();
}

#endif // HAVE(CFNETWORK_STORAGE_PARTITIONING)

#if !PLATFORM(COCOA)
void NetworkStorageSession::setCookies(const Vector<Cookie>&, const URL&, const URL&)
{
    // FIXME: Implement this. <https://webkit.org/b/156298>
}
#endif

}
