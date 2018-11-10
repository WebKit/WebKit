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
#if USE(CFURLCONNECTION)
#include "Cookie.h"
#include "CookieRequestHeaderFieldProxy.h"
#include "CookiesStrategy.h"
#include "NotImplemented.h"
#include "URL.h"
#include <CFNetwork/CFHTTPCookiesPriv.h>
#include <CoreFoundation/CoreFoundation.h>
#include <pal/spi/cf/CFNetworkSPI.h>
#include <windows.h>
#include <wtf/SoftLinking.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/text/WTFString.h>

enum {
    CFHTTPCookieStorageAcceptPolicyExclusivelyFromMainDocumentDomain = 3
};

namespace WTF {

#define DECLARE_CF_TYPE_TRAIT(ClassName) \
template <> \
struct CFTypeTrait<ClassName##Ref> { \
static inline CFTypeID typeID() { return ClassName##GetTypeID(); } \
};

#if COMPILER(CLANG)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
#endif
DECLARE_CF_TYPE_TRAIT(CFHTTPCookie);
#if COMPILER(CLANG)
ALLOW_DEPRECATED_DECLARATIONS_END
#endif

#undef DECLARE_CF_TYPE_TRAIT
} // namespace WTF

#endif

namespace WebCore {

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

void NetworkStorageSession::setStorageAccessAPIEnabled(bool enabled)
{
    storageAccessAPIEnabled = enabled;
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)

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

bool NetworkStorageSession::shouldBlockThirdPartyCookies(const String& topPrivatelyControlledDomain) const
{
    if (topPrivatelyControlledDomain.isEmpty())
        return false;

    return m_topPrivatelyControlledDomainsToBlock.contains(topPrivatelyControlledDomain);
}

bool NetworkStorageSession::shouldBlockCookies(const ResourceRequest& request, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID) const
{
    return shouldBlockCookies(request.firstPartyForCookies(), request.url(), frameID, pageID);
}
    
bool NetworkStorageSession::shouldBlockCookies(const URL& firstPartyForCookies, const URL& resource, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID) const
{
    auto firstPartyDomain = getPartitioningDomain(firstPartyForCookies);
    if (firstPartyDomain.isEmpty())
        return false;

    auto resourceDomain = getPartitioningDomain(resource);
    if (resourceDomain.isEmpty())
        return false;

    if (firstPartyDomain == resourceDomain)
        return false;

    if (pageID && hasStorageAccess(resourceDomain, firstPartyDomain, frameID, pageID.value()))
        return false;

    return shouldBlockThirdPartyCookies(resourceDomain);
}

std::optional<Seconds> NetworkStorageSession::maxAgeCacheCap(const ResourceRequest& request)
{
    if (m_cacheMaxAgeCapForPrevalentResources && shouldBlockCookies(request, std::nullopt, std::nullopt))
        return m_cacheMaxAgeCapForPrevalentResources;
    return std::nullopt;
}

void NetworkStorageSession::setAgeCapForClientSideCookies(std::optional<Seconds> seconds)
{
    m_ageCapForClientSideCookies = seconds;
}

void NetworkStorageSession::setPrevalentDomainsToBlockCookiesFor(const Vector<String>& domains)
{
    m_topPrivatelyControlledDomainsToBlock.clear();
    m_topPrivatelyControlledDomainsToBlock.add(domains.begin(), domains.end());
}

void NetworkStorageSession::removePrevalentDomains(const Vector<String>& domains)
{
    for (auto& domain : domains)
        m_topPrivatelyControlledDomainsToBlock.remove(domain);
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

void NetworkStorageSession::setCacheMaxAgeCapForPrevalentResources(Seconds seconds)
{
    m_cacheMaxAgeCapForPrevalentResources = seconds;
}
    
void NetworkStorageSession::resetCacheMaxAgeCapForPrevalentResources()
{
    m_cacheMaxAgeCapForPrevalentResources = std::nullopt;
}
#endif //  ENABLE(RESOURCE_LOAD_STATISTICS)

#if !PLATFORM(COCOA)
void NetworkStorageSession::setCookies(const Vector<Cookie>&, const URL&, const URL&)
{
    // FIXME: Implement this. <https://webkit.org/b/156298>
}
#endif

} // namespace WebCore

#if USE(CFURLCONNECTION)

namespace WebCore {

static const CFStringRef s_setCookieKeyCF = CFSTR("Set-Cookie");
static const CFStringRef s_cookieCF = CFSTR("Cookie");
static const CFStringRef s_createdCF = CFSTR("Created");

static inline RetainPtr<CFStringRef> cookieDomain(CFHTTPCookieRef cookie)
{
    return adoptCF(CFHTTPCookieCopyDomain(cookie));
}

static double canonicalCookieTime(double time)
{
    if (!time)
        return time;

    return (time + kCFAbsoluteTimeIntervalSince1970) * 1000;
}

static double cookieCreatedTime(CFHTTPCookieRef cookie)
{
    RetainPtr<CFDictionaryRef> props = adoptCF(CFHTTPCookieCopyProperties(cookie));
    auto value = CFDictionaryGetValue(props.get(), s_createdCF);

    auto asNumber = dynamic_cf_cast<CFNumberRef>(value);
    if (asNumber) {
        double asDouble;
        if (CFNumberGetValue(asNumber, kCFNumberFloat64Type, &asDouble))
            return canonicalCookieTime(asDouble);
        return 0.0;
    }

    auto asString = dynamic_cf_cast<CFStringRef>(value);
    if (asString)
        return canonicalCookieTime(CFStringGetDoubleValue(asString));

    return 0.0;
}

static inline CFAbsoluteTime cookieExpirationTime(CFHTTPCookieRef cookie)
{
    return canonicalCookieTime(CFHTTPCookieGetExpirationTime(cookie));
}

static inline RetainPtr<CFStringRef> cookieName(CFHTTPCookieRef cookie)
{
    return adoptCF(CFHTTPCookieCopyName(cookie));
}

static inline RetainPtr<CFStringRef> cookiePath(CFHTTPCookieRef cookie)
{
    return adoptCF(CFHTTPCookieCopyPath(cookie));
}

static inline RetainPtr<CFStringRef> cookieValue(CFHTTPCookieRef cookie)
{
    return adoptCF(CFHTTPCookieCopyValue(cookie));
}

static RetainPtr<CFArrayRef> filterCookies(CFArrayRef unfilteredCookies)
{
    ASSERT(unfilteredCookies);
    CFIndex count = CFArrayGetCount(unfilteredCookies);
    RetainPtr<CFMutableArrayRef> filteredCookies = adoptCF(CFArrayCreateMutable(0, count, &kCFTypeArrayCallBacks));
    for (CFIndex i = 0; i < count; ++i) {
        CFHTTPCookieRef cookie = (CFHTTPCookieRef)CFArrayGetValueAtIndex(unfilteredCookies, i);

        // <rdar://problem/5632883> CFHTTPCookieStorage would store an empty cookie,
        // which would be sent as "Cookie: =". We have a workaround in setCookies() to prevent
        // that, but we also need to avoid sending cookies that were previously stored, and
        // there's no harm to doing this check because such a cookie is never valid.
        if (!CFStringGetLength(cookieName(cookie).get()))
            continue;

        if (CFHTTPCookieIsHTTPOnly(cookie))
            continue;

        CFArrayAppendValue(filteredCookies.get(), cookie);
    }
    return filteredCookies;
}

static RetainPtr<CFArrayRef> copyCookiesForURLWithFirstPartyURL(const NetworkStorageSession& session, const URL& firstParty, const URL& url, IncludeSecureCookies includeSecureCookies)
{
    bool secure = includeSecureCookies == IncludeSecureCookies::Yes;

    ASSERT(!secure || (secure && url.protocolIs("https")));

    UNUSED_PARAM(firstParty);
    return adoptCF(CFHTTPCookieStorageCopyCookiesForURL(session.cookieStorage().get(), url.createCFURL().get(), secure));
}

static CFArrayRef createCookies(CFDictionaryRef headerFields, CFURLRef url)
{
    CFArrayRef parsedCookies = CFHTTPCookieCreateWithResponseHeaderFields(kCFAllocatorDefault, headerFields, url);
    if (!parsedCookies)
        parsedCookies = CFArrayCreate(kCFAllocatorDefault, 0, 0, &kCFTypeArrayCallBacks);

    return parsedCookies;
}

void NetworkStorageSession::setCookiesFromDOM(const URL& firstParty, const SameSiteInfo&, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, const String& value) const
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    // <rdar://problem/5632883> CFHTTPCookieStorage stores an empty cookie, which would be sent as "Cookie: =".
    if (value.isEmpty())
        return;

    RetainPtr<CFURLRef> urlCF = url.createCFURL();
    RetainPtr<CFURLRef> firstPartyForCookiesCF = firstParty.createCFURL();

    // <http://bugs.webkit.org/show_bug.cgi?id=6531>, <rdar://4409034>
    // cookiesWithResponseHeaderFields doesn't parse cookies without a value
    String cookieString = value.contains('=') ? value : value + "=";

    RetainPtr<CFStringRef> cookieStringCF = cookieString.createCFString();
    auto cookieStringCFPtr = cookieStringCF.get();
    RetainPtr<CFDictionaryRef> headerFieldsCF = adoptCF(CFDictionaryCreate(kCFAllocatorDefault,
        (const void**)&s_setCookieKeyCF, (const void**)&cookieStringCFPtr, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    RetainPtr<CFArrayRef> unfilteredCookies = adoptCF(createCookies(headerFieldsCF.get(), urlCF.get()));
    CFHTTPCookieStorageSetCookies(cookieStorage().get(), filterCookies(unfilteredCookies.get()).get(), urlCF.get(), firstPartyForCookiesCF.get());
}

static bool containsSecureCookies(CFArrayRef cookies)
{
    CFIndex cookieCount = CFArrayGetCount(cookies);
    while (cookieCount--) {
        if (CFHTTPCookieIsSecure(checked_cf_cast<CFHTTPCookieRef>(CFArrayGetValueAtIndex(cookies, cookieCount))))
            return true;
    }

    return false;
}

std::pair<String, bool> NetworkStorageSession::cookiesForDOM(const URL& firstParty, const SameSiteInfo&, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies) const
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    RetainPtr<CFArrayRef> cookiesCF = copyCookiesForURLWithFirstPartyURL(*this, firstParty, url, includeSecureCookies);

    auto filteredCookies = filterCookies(cookiesCF.get());

    bool didAccessSecureCookies = containsSecureCookies(filteredCookies.get());

    RetainPtr<CFDictionaryRef> headerCF = adoptCF(CFHTTPCookieCopyRequestHeaderFields(kCFAllocatorDefault, filteredCookies.get()));
    String cookieString = checked_cf_cast<CFStringRef>(CFDictionaryGetValue(headerCF.get(), s_cookieCF));
    return { cookieString, didAccessSecureCookies };
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const URL& firstParty, const SameSiteInfo&, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies) const
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    RetainPtr<CFArrayRef> cookiesCF = copyCookiesForURLWithFirstPartyURL(*this, firstParty, url, includeSecureCookies);

    bool didAccessSecureCookies = containsSecureCookies(cookiesCF.get());

    RetainPtr<CFDictionaryRef> headerCF = adoptCF(CFHTTPCookieCopyRequestHeaderFields(kCFAllocatorDefault, cookiesCF.get()));
    String cookieString = checked_cf_cast<CFStringRef>(CFDictionaryGetValue(headerCF.get(), s_cookieCF));
    return { cookieString, didAccessSecureCookies };
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const CookieRequestHeaderFieldProxy& headerFieldProxy) const
{
    return cookieRequestHeaderFieldValue(headerFieldProxy.firstParty, headerFieldProxy.sameSiteInfo, headerFieldProxy.url, headerFieldProxy.frameID, headerFieldProxy.pageID, headerFieldProxy.includeSecureCookies);
}

bool NetworkStorageSession::cookiesEnabled() const
{
    CFHTTPCookieStorageAcceptPolicy policy = CFHTTPCookieStorageGetCookieAcceptPolicy(cookieStorage().get());
    return policy == CFHTTPCookieStorageAcceptPolicyOnlyFromMainDocumentDomain || policy == CFHTTPCookieStorageAcceptPolicyExclusivelyFromMainDocumentDomain || policy == CFHTTPCookieStorageAcceptPolicyAlways;
}

bool NetworkStorageSession::getRawCookies(const URL& firstParty, const SameSiteInfo&, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, Vector<Cookie>& rawCookies) const
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    rawCookies.clear();

    auto includeSecureCookies = url.protocolIs("https") ? IncludeSecureCookies::Yes : IncludeSecureCookies::No;

    RetainPtr<CFArrayRef> cookiesCF = copyCookiesForURLWithFirstPartyURL(*this, firstParty, url, includeSecureCookies);

    CFIndex count = CFArrayGetCount(cookiesCF.get());
    rawCookies.reserveCapacity(count);

    for (CFIndex i = 0; i < count; i++) {
        CFHTTPCookieRef cfCookie = checked_cf_cast<CFHTTPCookieRef>(CFArrayGetValueAtIndex(cookiesCF.get(), i));
        Cookie cookie;
        cookie.name = cookieName(cfCookie).get();
        cookie.value = cookieValue(cfCookie).get();
        cookie.domain = cookieDomain(cfCookie).get();
        cookie.path = cookiePath(cfCookie).get();
        cookie.created = cookieCreatedTime(cfCookie);
        cookie.expires = cookieExpirationTime(cfCookie);
        cookie.httpOnly = CFHTTPCookieIsHTTPOnly(cfCookie);
        cookie.secure = CFHTTPCookieIsSecure(cfCookie);
        cookie.session = false; // FIXME: Need API for if a cookie is a session cookie.
        rawCookies.uncheckedAppend(WTFMove(cookie));
    }

    return true;
}

void NetworkStorageSession::deleteCookie(const URL& url, const String& name) const
{
    RetainPtr<CFHTTPCookieStorageRef> cookieStorage = this->cookieStorage();

    RetainPtr<CFURLRef> urlCF = url.createCFURL();

    bool sendSecureCookies = url.protocolIs("https");
    RetainPtr<CFArrayRef> cookiesCF = adoptCF(CFHTTPCookieStorageCopyCookiesForURL(cookieStorage.get(), urlCF.get(), sendSecureCookies));

    CFIndex count = CFArrayGetCount(cookiesCF.get());
    for (CFIndex i = 0; i < count; i++) {
        CFHTTPCookieRef cookie = checked_cf_cast<CFHTTPCookieRef>(CFArrayGetValueAtIndex(cookiesCF.get(), i));
        if (String(cookieName(cookie).get()) == name) {
            CFHTTPCookieStorageDeleteCookie(cookieStorage.get(), cookie);
            break;
        }
    }
}

void NetworkStorageSession::getHostnamesWithCookies(HashSet<String>& hostnames)
{
    RetainPtr<CFArrayRef> cookiesCF = adoptCF(CFHTTPCookieStorageCopyCookies(cookieStorage().get()));
    if (!cookiesCF)
        return;

    CFIndex count = CFArrayGetCount(cookiesCF.get());
    for (CFIndex i = 0; i < count; ++i) {
        CFHTTPCookieRef cookie = checked_cf_cast<CFHTTPCookieRef>(CFArrayGetValueAtIndex(cookiesCF.get(), i));
        RetainPtr<CFStringRef> domain = cookieDomain(cookie);
        hostnames.add(domain.get());
    }
}

void NetworkStorageSession::deleteAllCookies()
{
    CFHTTPCookieStorageDeleteAllCookies(cookieStorage().get());
}

void NetworkStorageSession::deleteCookiesForHostnames(const Vector<String>& hostnames)
{
}

void NetworkStorageSession::deleteAllCookiesModifiedSince(WallTime)
{
}

} // namespace WebCore

#endif // USE(CFURLCONNECTION)
