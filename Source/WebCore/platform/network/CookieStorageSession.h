/*
 * Copyright (C) 2012-2026 Apple Inc. All rights reserved.
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

#include <WebCore/CredentialStorage.h>
#include <WebCore/ThirdPartyCookieBlockingMode.h>
#include <optional>
#include <pal/SessionID.h>
#include <wtf/CheckedPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/Noncopyable.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <pal/spi/cf/CFNetworkSPI.h>
#include <wtf/RetainPtr.h>
OBJC_CLASS NSArray;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSHTTPCookie;
OBJC_CLASS NSHTTPCookieStorage;
OBJC_CLASS NSString;
OBJC_CLASS NSURL;
#endif

namespace WebCore {

struct Cookie;
struct CookieRequestHeaderFieldProxy;
struct SameSiteInfo;

enum class HTTPCookieAcceptPolicy : uint8_t;
enum class IncludeSecureCookies : bool;

class CookieStorageSession : public CanMakeCheckedPtr<CookieStorageSession> {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(CookieStorageSession, WEBCORE_EXPORT);
    WTF_MAKE_NONCOPYABLE(CookieStorageSession);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(CookieStorageSession);
public:
    enum class IsInMemoryCookieStore : bool { No, Yes };

    WEBCORE_EXPORT explicit CookieStorageSession(PAL::SessionID, IsInMemoryCookieStore = IsInMemoryCookieStore::No);
#if PLATFORM(COCOA)
    WEBCORE_EXPORT CookieStorageSession(PAL::SessionID, RetainPtr<CFURLStorageSessionRef>&&, RetainPtr<CFHTTPCookieStorageRef>&&, IsInMemoryCookieStore = IsInMemoryCookieStore::No);
#endif
    WEBCORE_EXPORT virtual ~CookieStorageSession();

    PAL::SessionID sessionID() const { return m_sessionID; }
    CredentialStorage& credentialStorage() LIFETIME_BOUND { return m_credentialStorage; }

    WEBCORE_EXPORT static void NODELETE permitProcessToUseCookieAPI(bool);
    WEBCORE_EXPORT static bool NODELETE processMayUseCookieAPI();

    WEBCORE_EXPORT static String cookiePartitionIdentifier(const URL& firstPartyForCookies);

#if PLATFORM(COCOA)
    enum class ShouldDisableCFURLCache : bool { No, Yes };
    WEBCORE_EXPORT static RetainPtr<CFURLStorageSessionRef> createCFStorageSessionForIdentifier(CFStringRef identifier, ShouldDisableCFURLCache = ShouldDisableCFURLCache::No);

    // May be null, in which case a Foundation default should be used.
    CFURLStorageSessionRef platformSession() const { return m_platformSession.get(); }
    WEBCORE_EXPORT RetainPtr<CFHTTPCookieStorageRef> cookieStorage() const;
    WEBCORE_EXPORT RetainPtr<NSHTTPCookieStorage> nsCookieStorage() const;

    WEBCORE_EXPORT static RetainPtr<NSHTTPCookie> capExpiryOfPersistentCookie(NSHTTPCookie *, Seconds cap);
#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
    WEBCORE_EXPORT static NSHTTPCookie *setCookiePartition(NSHTTPCookie *, NSString *partition);
#endif

    WEBCORE_EXPORT std::pair<String, bool> cookiesForDOM(const URL& firstParty, const SameSiteInfo&, const URL&, IncludeSecureCookies, ThirdPartyCookieBlockingDecision, const String& partition) const;
    WEBCORE_EXPORT std::pair<String, bool> cookieRequestHeaderFieldValue(const URL& firstParty, const SameSiteInfo&, const URL&, IncludeSecureCookies, ThirdPartyCookieBlockingDecision, const String& partition) const;
    WEBCORE_EXPORT std::pair<String, bool> cookieRequestHeaderFieldValue(const CookieRequestHeaderFieldProxy&, ThirdPartyCookieBlockingDecision, const String& partition) const;
    WEBCORE_EXPORT void setCookiesFromDOM(const URL& firstParty, const SameSiteInfo&, const URL&, const String& cookieString, ThirdPartyCookieBlockingDecision, std::optional<Seconds> cappedLifetime, const String& partition) const;
    WEBCORE_EXPORT bool setCookieFromDOM(const URL& firstParty, const SameSiteInfo&, const URL&, const Cookie&, ThirdPartyCookieBlockingDecision, std::optional<Seconds> cappedLifetime, const String& partition) const;
    WEBCORE_EXPORT bool getRawCookies(const URL& firstParty, const SameSiteInfo&, const URL&, ThirdPartyCookieBlockingDecision, const String& partition, Vector<Cookie>&) const;
    WEBCORE_EXPORT void setCookie(const Cookie&);
    WEBCORE_EXPORT void deleteCookie(const URL& firstParty, const URL&, const String& cookieName, const String& partition, CompletionHandler<void()>&&) const;
    WEBCORE_EXPORT void deleteAllCookies(CompletionHandler<void()>&&);
#endif // PLATFORM(COCOA)

protected:
    bool isInMemoryCookieStore() const { return m_isInMemoryCookieStore; }

#if PLATFORM(COCOA)
    enum class CookiesFor : bool { Dom, Http };

    WEBCORE_EXPORT std::pair<String, bool> cookiesForSession(const URL& firstParty, const SameSiteInfo&, const URL&, CookiesFor, IncludeSecureCookies, ThirdPartyCookieBlockingDecision, const String& partition) const;
    WEBCORE_EXPORT std::optional<Vector<Cookie>> cookiesForSessionAsVector(const URL& firstParty, const SameSiteInfo&, const URL&, CookiesFor, IncludeSecureCookies, ThirdPartyCookieBlockingDecision, const String& partition, const String& cookieName) const;
    WEBCORE_EXPORT RetainPtr<NSArray> cookiesForURL(const URL& firstParty, const SameSiteInfo&, const URL&, ThirdPartyCookieBlockingDecision, const String& partition) const;
    WEBCORE_EXPORT void deleteHTTPCookie(CFHTTPCookieStorageRef, NSHTTPCookie *, CompletionHandler<void()>&&) const;
    WEBCORE_EXPORT static Vector<Cookie> nsCookiesToCookieVector(NSArray *, NOESCAPE const Function<bool(NSHTTPCookie *)>& filter = { });
#endif

private:
#if PLATFORM(COCOA)
    static RetainPtr<NSDictionary> policyProperties(const SameSiteInfo&, NSURL *, NSString *partition, ThirdPartyCookieBlockingDecision);
    static RetainPtr<NSArray> cookiesForURLFromStorage(NSHTTPCookieStorage *, NSURL *, NSURL *mainDocumentURL, const std::optional<SameSiteInfo>&, ThirdPartyCookieBlockingDecision, NSString *partition);
    static RetainPtr<NSHTTPCookie> parseDOMCookie(const String&, NSURL *cookieURL, std::optional<Seconds> cappedLifetime, const String& partition);
    static RetainPtr<NSHTTPCookie> adjustScriptWrittenCookie(NSHTTPCookie *, std::optional<Seconds> cappedLifetime);
    static bool shouldIncludeCookie(NSHTTPCookie *, CookiesFor, IncludeSecureCookies, bool& didAccessSecureCookies);
    RetainPtr<NSArray> httpCookiesForURL(CFHTTPCookieStorageRef, NSURL *firstParty, const std::optional<SameSiteInfo>&, NSURL *, ThirdPartyCookieBlockingDecision, NSString *partition) const;
    void setHTTPCookiesForURL(CFHTTPCookieStorageRef, NSArray *cookies, NSURL *, NSURL *mainDocumentURL, NSString *partition, const SameSiteInfo&, ThirdPartyCookieBlockingDecision) const;
#endif

    PAL::SessionID m_sessionID;
    const bool m_isInMemoryCookieStore { false };
    CredentialStorage m_credentialStorage;
#if PLATFORM(COCOA)
    RetainPtr<CFURLStorageSessionRef> m_platformSession;
    RetainPtr<CFHTTPCookieStorageRef> m_platformCookieStorage;
#endif

    static bool m_processMayUseCookieAPI;
};

#if PLATFORM(COCOA)
WEBCORE_EXPORT RetainPtr<CFURLStorageSessionRef> createPrivateStorageSession(CFStringRef identifier, std::optional<HTTPCookieAcceptPolicy> = std::nullopt, CookieStorageSession::ShouldDisableCFURLCache = CookieStorageSession::ShouldDisableCFURLCache::No);
#endif

} // namespace WebCore
