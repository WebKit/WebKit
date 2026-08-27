/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/CookieChangeObserver.h>
#include <WebCore/CookieStorageSession.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ShouldRelaxThirdPartyCookieBlocking.h>
#include <WebCore/ThirdPartyCookieBlockingMode.h>
#include <WebCore/TrackingPreventionTypes.h>
#include <optional>
#include <pal/SessionID.h>
#include <span>
#include <wtf/CheckedPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/Platform.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <pal/spi/cf/CFNetworkSPI.h>
#include <wtf/RetainPtr.h>
#endif

#if USE(SOUP)
#include <wtf/glib/GRefPtr.h>
typedef struct _SoupCookieJar SoupCookieJar;
typedef struct _SoupCookie SoupCookie;
#endif

#if USE(CURL)
#include <WebCore/CookieJarDB.h>
#include <wtf/UniqueRef.h>
#endif

#ifdef __OBJC__
#include <objc/objc.h>
#endif

#if PLATFORM(COCOA)
#include "CookieStorageObserver.h"
OBJC_CLASS NSArray;
OBJC_CLASS NSHTTPCookie;
OBJC_CLASS NSHTTPCookieStorage;
OBJC_CLASS NSMutableSet;
#endif

namespace WebCore {

class CurlProxySettings;
class ResourceRequest;

struct ClientOrigin;
struct Cookie;
struct CookieRequestHeaderFieldProxy;
struct CookieStoreGetOptions;
struct SameSiteInfo;

enum class HTTPCookieAcceptPolicy : uint8_t;
enum class IncludeSecureCookies : bool;
enum class IncludeHttpOnlyCookies : bool;
enum class ShouldPartitionCookie : bool;

}

namespace WebKit {

class NetworkStorageSession
    : public WebCore::CookieStorageSession
    , public CanMakeWeakPtr<NetworkStorageSession> {
    WTF_MAKE_TZONE_ALLOCATED(NetworkStorageSession);
    WTF_MAKE_NONCOPYABLE(NetworkStorageSession);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(NetworkStorageSession);
public:
    using TopFrameDomain = WebCore::RegistrableDomain;
    using SubResourceDomain = WebCore::RegistrableDomain;

    // Re-export the base's cookiePartitionIdentifier(const URL&), which the ResourceRequest
    // overload below would otherwise hide. The base's policy-explicit cookie accessors are
    // deliberately *not* re-exported: the wide overloads below hide them by name, so cookie access
    // through a NetworkStorageSession always goes through tracking prevention.
    using WebCore::CookieStorageSession::cookiePartitionIdentifier;

#if PLATFORM(COCOA)
    NetworkStorageSession(PAL::SessionID sessionID, RetainPtr<CFURLStorageSessionRef>&& platformSession, RetainPtr<CFHTTPCookieStorageRef>&& platformCookieStorage, IsInMemoryCookieStore isInMemoryCookieStore = IsInMemoryCookieStore::No)
        : WebCore::CookieStorageSession(sessionID, WTF::move(platformSession), WTF::move(platformCookieStorage), isInMemoryCookieStore) { }
    explicit NetworkStorageSession(PAL::SessionID sessionID)
        : WebCore::CookieStorageSession(sessionID) { }
    ~NetworkStorageSession();

    CookieStorageObserver& cookieStorageObserver() const;
#elif USE(SOUP)
    explicit NetworkStorageSession(PAL::SessionID, IsInMemoryCookieStore = IsInMemoryCookieStore::No);
    ~NetworkStorageSession();

    SoupCookieJar* cookieStorage() const { return m_cookieStorage.get(); }
    void setCookieStorage(GRefPtr<SoupCookieJar>&&);
    void setCookieAcceptPolicy(WebCore::HTTPCookieAcceptPolicy);
    void setCookieObserverHandler(Function<void ()>&&);
    void getCredentialFromPersistentStorage(const WebCore::ProtectionSpace&, GCancellable*, Function<void (WebCore::Credential&&)>&& completionHandler);
    void saveCredentialToPersistentStorage(const WebCore::ProtectionSpace&, const WebCore::Credential&);
    void replaceCookies(const Vector<WebCore::Cookie>&);
#elif USE(CURL)
    NetworkStorageSession(PAL::SessionID, const String& alternativeServicesDirectory = nullString());
    ~NetworkStorageSession();

    WebCore::CookieJarDB& cookieDatabase() const;
    void setCookieDatabase(UniqueRef<WebCore::CookieJarDB>&&);
    void setCookiesFromHTTPResponse(const URL& firstParty, const URL&, const String&) const;
    void setCookieAcceptPolicy(WebCore::CookieAcceptPolicy) const;
    void setProxySettings(const WebCore::CurlProxySettings&);

    void clearAlternativeServices();
#endif

    WebCore::HTTPCookieAcceptPolicy cookieAcceptPolicy() const;
#if PLATFORM(COCOA)
    using WebCore::CookieStorageSession::setCookie;
    using WebCore::CookieStorageSession::deleteAllCookies;
#else
    void setCookie(const WebCore::Cookie&);
    void deleteAllCookies(CompletionHandler<void()>&&);
#endif
#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
    void setCookie(const URL& firstParty, const WebCore::Cookie&, WebCore::ShouldPartitionCookie);
#endif
    void setCookie(const WebCore::Cookie&, const URL&, const URL& mainDocumentURL);
    void setCookies(const Vector<WebCore::Cookie>&, const URL&, const URL& mainDocumentURL);
    void setCookiesFromDOM(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::ApplyTrackingPrevention, WebCore::RequiresScriptTrackingPrivacy, const String& cookieString, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::IsKnownCrossSiteTracker) const;
    bool setCookieFromDOM(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::ApplyTrackingPrevention, WebCore::RequiresScriptTrackingPrivacy, const WebCore::Cookie&, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::IsKnownCrossSiteTracker) const;
    void deleteCookie(const WebCore::Cookie&, CompletionHandler<void()>&&);
    void deleteCookie(const URL& firstParty, const URL&, const String&, CompletionHandler<void()>&&) const;
    void deleteAllCookiesModifiedSince(WallTime, CompletionHandler<void()>&&);
    void deleteCookies(const WebCore::ClientOrigin&, CompletionHandler<void()>&&);
    void deleteCookiesForHostnames(std::span<const String> cookieHostNames, CompletionHandler<void()>&&);
    void deleteCookiesForHostnames(std::span<const String> cookieHostNames, WebCore::IncludeHttpOnlyCookies, WebCore::ScriptWrittenCookiesOnly, CompletionHandler<void()>&&);
    Vector<WebCore::Cookie> getAllCookies();
    Vector<WebCore::Cookie> getCookies(const URL&);
    void hasCookies(const WebCore::RegistrableDomain&, CompletionHandler<void(bool)>&&) const;
    bool getRawCookies(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::ApplyTrackingPrevention, WebCore::ShouldRelaxThirdPartyCookieBlocking, Vector<WebCore::Cookie>&) const;
    void getHostnamesWithCookies(HashSet<String>& hostnames);
    std::pair<String, bool> cookiesForDOM(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::IncludeSecureCookies, WebCore::ApplyTrackingPrevention, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::IsKnownCrossSiteTracker) const;
    std::optional<Vector<WebCore::Cookie>> cookiesForDOMAsVector(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::IncludeSecureCookies, WebCore::ApplyTrackingPrevention, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::IsKnownCrossSiteTracker, WebCore::CookieStoreGetOptions&&) const;
    std::pair<String, bool> cookieRequestHeaderFieldValue(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::IncludeSecureCookies, WebCore::ApplyTrackingPrevention, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::IsKnownCrossSiteTracker) const;
    std::pair<String, bool> cookieRequestHeaderFieldValue(const WebCore::CookieRequestHeaderFieldProxy&) const;
    bool cookiesEnabled(const URL& firstParty, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::IsKnownCrossSiteTracker) const;

    Vector<WebCore::Cookie> domCookiesForHost(const URL&);

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    bool startListeningForCookieChangeNotifications(WebCore::CookieChangeObserver&, const URL&, const URL& firstParty, WebCore::FrameIdentifier, WebCore::PageIdentifier, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::IsKnownCrossSiteTracker);
    void stopListeningForCookieChangeNotifications(WebCore::CookieChangeObserver&, const HashSet<String>& hosts);
#endif
    void addCookiesEnabledStateObserver(WebCore::CookiesEnabledStateObserver&);
    void removeCookiesEnabledStateObserver(WebCore::CookiesEnabledStateObserver&);
    void cookieEnabledStateMayHaveChanged();

    void NODELETE setTrackingPreventionEnabled(bool);
    bool NODELETE trackingPreventionEnabled() const;
    void NODELETE setTrackingPreventionDebugLoggingEnabled(bool);
    bool trackingPreventionDebugLoggingEnabled() const { return m_isTrackingPreventionDebugLoggingEnabled; }
    WebCore::ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecisionForRequest(const WebCore::ResourceRequest&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::IsKnownCrossSiteTracker, bool isInitiatedByDedicatedWorker = false, bool navigationLosesFrameSpecificStorageAccess = false) const;
    WebCore::ThirdPartyCookieBlockingDecision thirdPartyCookieBlockingDecisionForRequest(const URL& firstPartyForCookies, const URL& resource, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::IsKnownCrossSiteTracker, bool isInitiatedByDedicatedWorker = false, bool navigationLosesFrameSpecificStorageAccess = false) const;
    bool shouldBlockCookies(const WebCore::ResourceRequest&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::IsKnownCrossSiteTracker) const;
    bool shouldBlockCookies(const URL& firstPartyForCookies, const URL& resource, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::IsKnownCrossSiteTracker) const;
    bool shouldBlockThirdPartyCookies(const WebCore::RegistrableDomain&) const;
    bool shouldBlockThirdPartyCookiesButKeepFirstPartyCookiesFor(const WebCore::RegistrableDomain&) const;
    static bool NODELETE shouldBlockCookies(WebCore::ThirdPartyCookieBlockingDecision);
    void setAllCookiesToSameSiteStrict(const WebCore::RegistrableDomain&, CompletionHandler<void()>&&);
    static String cookiePartitionIdentifier(const WebCore::ResourceRequest&);
    bool hasHadUserInteractionAsFirstParty(const WebCore::RegistrableDomain&) const;
    void setPrevalentDomainsToBlockAndDeleteCookiesFor(const Vector<WebCore::RegistrableDomain>&);
    void setPrevalentDomainsToBlockButKeepCookiesFor(const Vector<WebCore::RegistrableDomain>&);
    void setDomainsWithUserInteractionAsFirstParty(const Vector<WebCore::RegistrableDomain>&);
    void setDomainsWithCrossPageStorageAccess(const HashMap<TopFrameDomain, Vector<SubResourceDomain>>&);
    void grantCrossPageStorageAccess(const TopFrameDomain&, const SubResourceDomain&);
    void NODELETE setAgeCapForClientSideCookies(std::optional<Seconds>);
    bool hasStorageAccess(const WebCore::RegistrableDomain& resourceDomain, const WebCore::RegistrableDomain& firstPartyDomain, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>) const;
    Vector<String> getAllStorageAccessEntries() const;
    void grantStorageAccess(const WebCore::RegistrableDomain& resourceDomain, const WebCore::RegistrableDomain& firstPartyDomain, std::optional<WebCore::FrameIdentifier>, WebCore::PageIdentifier);
    void removeStorageAccessForFrame(WebCore::FrameIdentifier, WebCore::PageIdentifier);
    void clearPageSpecificDataForResourceLoadStatistics(WebCore::PageIdentifier);
    void removeAllStorageAccess();
    void NODELETE setCacheMaxAgeCapForPrevalentResources(Seconds);
    void NODELETE resetCacheMaxAgeCapForPrevalentResources();
    std::optional<Seconds> maxAgeCacheCap(const WebCore::ResourceRequest&, WebCore::IsKnownCrossSiteTracker);
    void didCommitCrossSiteLoadWithDataTransferFromPrevalentResource(const WebCore::RegistrableDomain& toDomain, WebCore::PageIdentifier);
    void resetCrossSiteLoadsWithLinkDecorationForTesting();
    void NODELETE setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode);
    void setOptInCookiePartitioningEnabled(bool);

#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
    bool isOptInCookiePartitioningEnabled() const { return m_isOptInCookiePartitioningEnabled; }
#endif

#if ENABLE(APP_BOUND_DOMAINS)
    void setAppBoundDomains(HashSet<WebCore::RegistrableDomain>&&);
    void resetAppBoundDomains();
#endif

#if ENABLE(MANAGED_DOMAINS)
    void setManagedDomains(HashSet<WebCore::RegistrableDomain>&&);
    void resetManagedDomains();
#endif

    uint64_t cookiesVersion() const { return m_cookiesVersion; }
    void setCookiesVersion(uint64_t);
    struct CookieVersionChangeCallback {
        enum class Reason : uint8_t { VersionChange, SessionClose };
        uint64_t version;
        CompletionHandler<void(Reason)> callback;
    };
    void addCookiesVersionChangeCallback(CookieVersionChangeCallback&&);

private:
#if PLATFORM(COCOA)
    std::pair<String, bool> cookiesForSession(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, CookiesFor, WebCore::IncludeSecureCookies, WebCore::ApplyTrackingPrevention, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::IsKnownCrossSiteTracker) const;
    std::optional<Vector<WebCore::Cookie>> cookiesForSessionAsVector(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, CookiesFor, WebCore::IncludeSecureCookies, WebCore::ApplyTrackingPrevention, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::IsKnownCrossSiteTracker, WebCore::CookieStoreGetOptions&&) const;
    RetainPtr<NSArray> cookiesForURL(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::ApplyTrackingPrevention, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::IsKnownCrossSiteTracker) const;
    void deleteCookiesMatching(NOESCAPE const Function<bool(NSHTTPCookie *)>& matches, CompletionHandler<void()>&&);

    // The opt-in cookie partition for this session, or a null string when partitioning is off.
    // The WebCore base takes the partition as an explicit argument rather than deriving it.
    String cookiePartitionIdentifierIfEnabled(const URL& firstParty) const;
#endif

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    void registerCookieChangeListenersIfNecessary();
    void unregisterCookieChangeListenersIfNecessary();
#endif
    void clearCookiesVersionChangeCallbacks();

#if USE(SOUP)
    static void cookiesDidChange(NetworkStorageSession*, SoupCookie* oldCookie, SoupCookie* newCookie, SoupCookieJar*);

    WebCore::HTTPCookieAcceptPolicy m_cookieAcceptPolicy;
    GRefPtr<SoupCookieJar> m_cookieStorage;
    Function<void ()> m_cookieObserverHandler;
#elif USE(CURL)
    mutable UniqueRef<WebCore::CookieJarDB> m_cookieDatabase;
#endif

#if HAVE(COOKIE_CHANGE_LISTENER_API)
#if PLATFORM(COCOA)
    RetainPtr<NSMutableSet> m_subscribedDomainsForCookieChanges;
    bool m_didRegisterCookieListeners { false };
#elif USE(SOUP)
    void notifyCookie(SoupCookie*, bool added);
    void notifyCookieAdded(SoupCookie*);
    void notifyCookieDeleted(SoupCookie*);
#endif
    MemoryCompactRobinHoodHashMap<String, WeakHashSet<WebCore::CookieChangeObserver>> m_cookieChangeObservers;
#endif // HAVE(COOKIE_CHANGE_LISTENER_API)
    WeakHashSet<WebCore::CookiesEnabledStateObserver> m_cookiesEnabledStateObservers;
#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
    bool m_isOptInCookiePartitioningEnabled { false };
#endif

    bool m_isTrackingPreventionEnabled = false;
    bool m_isTrackingPreventionDebugLoggingEnabled = false;
    std::optional<Seconds> NODELETE clientSideCookieCap(const TopFrameDomain&, WebCore::RequiresScriptTrackingPrivacy, std::optional<WebCore::PageIdentifier>) const;
    bool shouldExemptDomainPairFromThirdPartyCookieBlocking(const TopFrameDomain&, const SubResourceDomain&) const;
    HashSet<WebCore::RegistrableDomain> m_registrableDomainsToBlockAndDeleteCookiesFor;
    HashSet<WebCore::RegistrableDomain> m_registrableDomainsToBlockButKeepCookiesFor;
    HashSet<WebCore::RegistrableDomain> m_registrableDomainsWithUserInteractionAsFirstParty;
    HashMap<WebCore::PageIdentifier, HashMap<WebCore::FrameIdentifier, WebCore::RegistrableDomain>> m_framesGrantedStorageAccess;
    HashMap<WebCore::PageIdentifier, HashMap<WebCore::RegistrableDomain, WebCore::RegistrableDomain>> m_pagesGrantedStorageAccess;
    HashMap<TopFrameDomain, HashSet<SubResourceDomain>> m_pairsGrantedCrossPageStorageAccess;
    std::optional<Seconds> m_cacheMaxAgeCapForPrevalentResources;
    std::optional<Seconds> m_ageCapForClientSideCookies;
    std::optional<Seconds> m_ageCapForClientSideCookiesShort;
    std::optional<Seconds> m_ageCapForClientSideCookiesForScriptTrackingPrivacy;
#if ENABLE(JS_COOKIE_CHECKING)
    std::optional<Seconds> m_ageCapForClientSideCookiesForLinkDecorationTargetPage;
#endif
    HashMap<WebCore::PageIdentifier, WebCore::RegistrableDomain> m_navigatedToWithLinkDecorationByPrevalentResource;
    bool m_navigationWithLinkDecorationTestMode = false;
    WebCore::ThirdPartyCookieBlockingMode m_thirdPartyCookieBlockingMode { WebCore::ThirdPartyCookieBlockingMode::All };
    HashSet<WebCore::RegistrableDomain> m_appBoundDomains;
    HashSet<WebCore::RegistrableDomain> m_managedDomains;

#if PLATFORM(COCOA)
    mutable std::unique_ptr<CookieStorageObserver> m_cookieStorageObserver;
#endif
    uint64_t m_cookiesVersion { 0 };
    Deque<CookieVersionChangeCallback> m_cookiesVersionChangeCallbacks;
};

} // namespace WebKit
