/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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

#include "CredentialStorage.h"
#include "FrameIdentifier.h"
#include "PageIdentifier.h"
#include "RegistrableDomain.h"
#include "ShouldRelaxThirdPartyCookieBlocking.h"
#include <pal/SessionID.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
#include <wtf/RetainPtr.h>
#endif

#if PLATFORM(COCOA)
#include <pal/spi/cf/CFNetworkSPI.h>
#elif USE(CFURLCONNECTION)
#include <pal/spi/win/CFNetworkSPIWin.h>
#endif

#if USE(SOUP)
#include <wtf/Function.h>
#include <wtf/glib/GRefPtr.h>
typedef struct _SoupCookieJar SoupCookieJar;
#endif

#if USE(CURL)
#include "CookieJarDB.h"
#include <wtf/UniqueRef.h>
#endif

#ifdef __OBJC__
#include <objc/objc.h>
#endif

#if PLATFORM(COCOA)
#include "CookieStorageObserver.h"
OBJC_CLASS NSArray;
OBJC_CLASS NSHTTPCookie;
OBJC_CLASS NSMutableSet;
#endif

namespace WebCore {

class CurlProxySettings;
class NetworkingContext;
class ResourceRequest;

struct Cookie;
struct CookieRequestHeaderFieldProxy;
struct SameSiteInfo;

enum class HTTPCookieAcceptPolicy : uint8_t;
enum class IncludeSecureCookies : bool;
enum class IncludeHttpOnlyCookies : bool;
enum class ThirdPartyCookieBlockingMode : uint8_t { All, AllExceptBetweenAppBoundDomains, AllOnSitesWithoutUserInteraction, OnlyAccordingToPerDomainPolicy };
enum class SameSiteStrictEnforcementEnabled : bool { Yes, No };
enum class FirstPartyWebsiteDataRemovalMode : uint8_t { AllButCookies, None, AllButCookiesLiveOnTestingTimeout, AllButCookiesReproTestingTimeout };
enum class ShouldAskITP : bool { No, Yes };

#if HAVE(COOKIE_CHANGE_LISTENER_API)
class CookieChangeObserver {
public:
    virtual ~CookieChangeObserver() { }
    virtual void cookiesAdded(const String& host, const Vector<WebCore::Cookie>&) = 0;
    virtual void cookiesDeleted(const String& host, const Vector<WebCore::Cookie>&) = 0;
    virtual void allCookiesDeleted() = 0;
};
#endif

class NetworkStorageSession {
    WTF_MAKE_NONCOPYABLE(NetworkStorageSession); WTF_MAKE_FAST_ALLOCATED;
public:
    using TopFrameDomain = WebCore::RegistrableDomain;
    using SubResourceDomain = WebCore::RegistrableDomain;

    WEBCORE_EXPORT static void permitProcessToUseCookieAPI(bool);
    WEBCORE_EXPORT static bool processMayUseCookieAPI();

    PAL::SessionID sessionID() const { return m_sessionID; }
    CredentialStorage& credentialStorage() { return m_credentialStorage; }

#ifdef __OBJC__
    WEBCORE_EXPORT NSHTTPCookieStorage *nsCookieStorage() const;
#endif

#if PLATFORM(COCOA)
    WEBCORE_EXPORT ~NetworkStorageSession();
#endif

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
    WEBCORE_EXPORT static RetainPtr<CFURLStorageSessionRef> createCFStorageSessionForIdentifier(CFStringRef identifier);
    enum class IsInMemoryCookieStore : bool { No, Yes };
    WEBCORE_EXPORT NetworkStorageSession(PAL::SessionID, RetainPtr<CFURLStorageSessionRef>&&, RetainPtr<CFHTTPCookieStorageRef>&&, IsInMemoryCookieStore = IsInMemoryCookieStore::No);
    WEBCORE_EXPORT explicit NetworkStorageSession(PAL::SessionID);

    // May be null, in which case a Foundation default should be used.
    CFURLStorageSessionRef platformSession() { return m_platformSession.get(); }
    WEBCORE_EXPORT RetainPtr<CFHTTPCookieStorageRef> cookieStorage() const;
    WEBCORE_EXPORT static void setStorageAccessAPIEnabled(bool);
#elif USE(SOUP)
    WEBCORE_EXPORT explicit NetworkStorageSession(PAL::SessionID);
    ~NetworkStorageSession();

    SoupCookieJar* cookieStorage() const { return m_cookieStorage.get(); }
    void setCookieStorage(GRefPtr<SoupCookieJar>&&);
    void setCookieAcceptPolicy(HTTPCookieAcceptPolicy);
    void setCookieObserverHandler(Function<void ()>&&);
    void getCredentialFromPersistentStorage(const ProtectionSpace&, GCancellable*, Function<void (Credential&&)>&& completionHandler);
    void saveCredentialToPersistentStorage(const ProtectionSpace&, const Credential&);
#elif USE(CURL)
    WEBCORE_EXPORT NetworkStorageSession(PAL::SessionID);
    ~NetworkStorageSession();

    CookieJarDB& cookieDatabase() const;
    WEBCORE_EXPORT void setCookieDatabase(UniqueRef<CookieJarDB>&&);
    WEBCORE_EXPORT void setCookiesFromHTTPResponse(const URL& firstParty, const URL&, const String&) const;
    WEBCORE_EXPORT void setCookieAcceptPolicy(CookieAcceptPolicy) const;
    WEBCORE_EXPORT void setProxySettings(CurlProxySettings&&);
#else
    WEBCORE_EXPORT NetworkStorageSession(PAL::SessionID, NetworkingContext*);
    ~NetworkStorageSession();

    NetworkingContext* context() const;
#endif

    WEBCORE_EXPORT HTTPCookieAcceptPolicy cookieAcceptPolicy() const;
    WEBCORE_EXPORT void setCookie(const Cookie&);
    WEBCORE_EXPORT void setCookies(const Vector<Cookie>&, const URL&, const URL& mainDocumentURL);
    WEBCORE_EXPORT void setCookiesFromDOM(const URL& firstParty, const SameSiteInfo&, const URL&, Optional<FrameIdentifier>, Optional<PageIdentifier>, ShouldAskITP, const String&, ShouldRelaxThirdPartyCookieBlocking) const;
    WEBCORE_EXPORT void deleteCookie(const Cookie&);
    WEBCORE_EXPORT void deleteCookie(const URL&, const String&) const;
    WEBCORE_EXPORT void deleteAllCookies();
    WEBCORE_EXPORT void deleteAllCookiesModifiedSince(WallTime);
    WEBCORE_EXPORT void deleteCookiesForHostnames(const Vector<String>& cookieHostNames);
    WEBCORE_EXPORT void deleteCookiesForHostnames(const Vector<String>& cookieHostNames, IncludeHttpOnlyCookies);
    WEBCORE_EXPORT Vector<Cookie> getAllCookies();
    WEBCORE_EXPORT Vector<Cookie> getCookies(const URL&);
    WEBCORE_EXPORT void hasCookies(const RegistrableDomain&, CompletionHandler<void(bool)>&&) const;
    WEBCORE_EXPORT bool getRawCookies(const URL& firstParty, const SameSiteInfo&, const URL&, Optional<FrameIdentifier>, Optional<PageIdentifier>, ShouldAskITP, ShouldRelaxThirdPartyCookieBlocking, Vector<Cookie>&) const;
    WEBCORE_EXPORT void flushCookieStore();
    WEBCORE_EXPORT void getHostnamesWithCookies(HashSet<String>& hostnames);
    WEBCORE_EXPORT std::pair<String, bool> cookiesForDOM(const URL& firstParty, const SameSiteInfo&, const URL&, Optional<FrameIdentifier>, Optional<PageIdentifier>, IncludeSecureCookies, ShouldAskITP, ShouldRelaxThirdPartyCookieBlocking) const;
    WEBCORE_EXPORT std::pair<String, bool> cookieRequestHeaderFieldValue(const URL& firstParty, const SameSiteInfo&, const URL&, Optional<FrameIdentifier>, Optional<PageIdentifier>, IncludeSecureCookies, ShouldAskITP, ShouldRelaxThirdPartyCookieBlocking) const;
    WEBCORE_EXPORT std::pair<String, bool> cookieRequestHeaderFieldValue(const CookieRequestHeaderFieldProxy&) const;

    WEBCORE_EXPORT Vector<Cookie> domCookiesForHost(const String& host);

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    WEBCORE_EXPORT void startListeningForCookieChangeNotifications(CookieChangeObserver&, const String& host);
    WEBCORE_EXPORT void stopListeningForCookieChangeNotifications(CookieChangeObserver&, const HashSet<String>& hosts);
    WEBCORE_EXPORT bool supportsCookieChangeListenerAPI() const;
#endif

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    WEBCORE_EXPORT void setResourceLoadStatisticsEnabled(bool);
    WEBCORE_EXPORT bool resourceLoadStatisticsEnabled() const;
    WEBCORE_EXPORT void setResourceLoadStatisticsDebugLoggingEnabled(bool);
    WEBCORE_EXPORT bool resourceLoadStatisticsDebugLoggingEnabled() const;
    WEBCORE_EXPORT bool shouldBlockCookies(const ResourceRequest&, Optional<FrameIdentifier>, Optional<PageIdentifier>, ShouldRelaxThirdPartyCookieBlocking) const;
    WEBCORE_EXPORT bool shouldBlockCookies(const URL& firstPartyForCookies, const URL& resource, Optional<FrameIdentifier>, Optional<PageIdentifier>, ShouldRelaxThirdPartyCookieBlocking) const;
    WEBCORE_EXPORT bool shouldBlockThirdPartyCookies(const RegistrableDomain&) const;
    WEBCORE_EXPORT bool shouldBlockThirdPartyCookiesButKeepFirstPartyCookiesFor(const RegistrableDomain&) const;
    WEBCORE_EXPORT void setAllCookiesToSameSiteStrict(const RegistrableDomain&, CompletionHandler<void()>&&);
#if PLATFORM(COCOA)
    WEBCORE_EXPORT static NSHTTPCookie *capExpiryOfPersistentCookie(NSHTTPCookie *, Seconds cap);
#endif
    WEBCORE_EXPORT bool hasHadUserInteractionAsFirstParty(const RegistrableDomain&) const;
    WEBCORE_EXPORT void setPrevalentDomainsToBlockAndDeleteCookiesFor(const Vector<RegistrableDomain>&);
    WEBCORE_EXPORT void setPrevalentDomainsToBlockButKeepCookiesFor(const Vector<RegistrableDomain>&);
    WEBCORE_EXPORT void setDomainsWithUserInteractionAsFirstParty(const Vector<RegistrableDomain>&);
    WEBCORE_EXPORT void setAgeCapForClientSideCookies(Optional<Seconds>);
    WEBCORE_EXPORT bool hasStorageAccess(const RegistrableDomain& resourceDomain, const RegistrableDomain& firstPartyDomain, Optional<FrameIdentifier>, PageIdentifier) const;
    WEBCORE_EXPORT Vector<String> getAllStorageAccessEntries() const;
    WEBCORE_EXPORT void grantStorageAccess(const RegistrableDomain& resourceDomain, const RegistrableDomain& firstPartyDomain, Optional<FrameIdentifier>, PageIdentifier);
    WEBCORE_EXPORT void removeStorageAccessForFrame(FrameIdentifier, PageIdentifier);
    WEBCORE_EXPORT void clearPageSpecificDataForResourceLoadStatistics(PageIdentifier);
    WEBCORE_EXPORT void removeAllStorageAccess();
    WEBCORE_EXPORT void setCacheMaxAgeCapForPrevalentResources(Seconds);
    WEBCORE_EXPORT void resetCacheMaxAgeCapForPrevalentResources();
    WEBCORE_EXPORT Optional<Seconds> maxAgeCacheCap(const ResourceRequest&);
    WEBCORE_EXPORT void didCommitCrossSiteLoadWithDataTransferFromPrevalentResource(const RegistrableDomain& toDomain, PageIdentifier);
    WEBCORE_EXPORT void resetCrossSiteLoadsWithLinkDecorationForTesting();
    WEBCORE_EXPORT void setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode);
    WEBCORE_EXPORT void setAppBoundDomains(HashSet<RegistrableDomain>&&);
    WEBCORE_EXPORT void resetAppBoundDomains();
#endif

private:
#if PLATFORM(COCOA)
    enum IncludeHTTPOnlyOrNot { DoNotIncludeHTTPOnly, IncludeHTTPOnly };
    std::pair<String, bool> cookiesForSession(const URL& firstParty, const SameSiteInfo&, const URL&, Optional<FrameIdentifier>, Optional<PageIdentifier>, IncludeHTTPOnlyOrNot, IncludeSecureCookies, ShouldAskITP, ShouldRelaxThirdPartyCookieBlocking) const;
    NSArray *httpCookies(CFHTTPCookieStorageRef) const;
    NSArray *httpCookiesForURL(CFHTTPCookieStorageRef, NSURL *firstParty, const Optional<SameSiteInfo>&, NSURL *) const;
    NSArray *cookiesForURL(const URL& firstParty, const SameSiteInfo&, const URL&, Optional<FrameIdentifier>, Optional<PageIdentifier>, ShouldAskITP, ShouldRelaxThirdPartyCookieBlocking) const;
    void setHTTPCookiesForURL(CFHTTPCookieStorageRef, NSArray *cookies, NSURL *, NSURL *mainDocumentURL, const SameSiteInfo&) const;
    void deleteHTTPCookie(CFHTTPCookieStorageRef, NSHTTPCookie *) const;
#endif

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    void registerCookieChangeListenersIfNecessary();
    void unregisterCookieChangeListenersIfNecessary();
#endif

    PAL::SessionID m_sessionID;

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
    RetainPtr<CFURLStorageSessionRef> m_platformSession;
    RetainPtr<CFHTTPCookieStorageRef> m_platformCookieStorage;
    bool m_isInMemoryCookieStore { false };
#elif USE(SOUP)
    static void cookiesDidChange(NetworkStorageSession*);

    HTTPCookieAcceptPolicy m_cookieAcceptPolicy;
    GRefPtr<SoupCookieJar> m_cookieStorage;
    Function<void ()> m_cookieObserverHandler;
#elif USE(CURL)
    mutable UniqueRef<CookieJarDB> m_cookieDatabase;
#else
    RefPtr<NetworkingContext> m_context;
#endif

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    bool m_didRegisterCookieListeners { false };
    RetainPtr<NSMutableSet> m_subscribedDomainsForCookieChanges;
    HashMap<String, HashSet<CookieChangeObserver*>> m_cookieChangeObservers;
#endif

    CredentialStorage m_credentialStorage;

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    bool m_isResourceLoadStatisticsEnabled = false;
    bool m_isResourceLoadStatisticsDebugLoggingEnabled = false;
    Optional<Seconds> clientSideCookieCap(const TopFrameDomain&, Optional<PageIdentifier>) const;
    bool shouldExemptDomainPairFromThirdPartyCookieBlocking(const TopFrameDomain&, const SubResourceDomain&) const;
    HashSet<RegistrableDomain> m_registrableDomainsToBlockAndDeleteCookiesFor;
    HashSet<RegistrableDomain> m_registrableDomainsToBlockButKeepCookiesFor;
    HashSet<RegistrableDomain> m_registrableDomainsWithUserInteractionAsFirstParty;
    HashMap<PageIdentifier, HashMap<FrameIdentifier, RegistrableDomain>> m_framesGrantedStorageAccess;
    HashMap<PageIdentifier, HashMap<RegistrableDomain, RegistrableDomain>> m_pagesGrantedStorageAccess;
    Optional<Seconds> m_cacheMaxAgeCapForPrevalentResources { };
    Optional<Seconds> m_ageCapForClientSideCookies { };
    Optional<Seconds> m_ageCapForClientSideCookiesShort { };
    HashMap<WebCore::PageIdentifier, RegistrableDomain> m_navigatedToWithLinkDecorationByPrevalentResource;
    bool m_navigationWithLinkDecorationTestMode = false;
    ThirdPartyCookieBlockingMode m_thirdPartyCookieBlockingMode { ThirdPartyCookieBlockingMode::All };
    HashSet<RegistrableDomain> m_appBoundDomains;
#endif

#if PLATFORM(COCOA)
public:
    CookieStorageObserver& cookieStorageObserver() const;

private:
    mutable std::unique_ptr<CookieStorageObserver> m_cookieStorageObserver;
#endif
    static bool m_processMayUseCookieAPI;
};

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
WEBCORE_EXPORT CFURLStorageSessionRef createPrivateStorageSession(CFStringRef identifier, Optional<HTTPCookieAcceptPolicy> = WTF::nullopt);
#endif

}

namespace WTF {

template<> struct EnumTraits<WebCore::ThirdPartyCookieBlockingMode> {
    using values = EnumValues<
        WebCore::ThirdPartyCookieBlockingMode,
        WebCore::ThirdPartyCookieBlockingMode::All,
        WebCore::ThirdPartyCookieBlockingMode::AllExceptBetweenAppBoundDomains,
        WebCore::ThirdPartyCookieBlockingMode::AllOnSitesWithoutUserInteraction,
        WebCore::ThirdPartyCookieBlockingMode::OnlyAccordingToPerDomainPolicy
    >;
};

template<> struct EnumTraits<WebCore::FirstPartyWebsiteDataRemovalMode> {
    using values = EnumValues<
        WebCore::FirstPartyWebsiteDataRemovalMode,
        WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies,
        WebCore::FirstPartyWebsiteDataRemovalMode::None,
        WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookiesLiveOnTestingTimeout,
        WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookiesReproTestingTimeout
    >;
};

}
