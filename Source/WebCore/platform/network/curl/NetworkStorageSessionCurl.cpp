/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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

#if USE(CURL)

#include "Cookie.h"
#include "CookieJarDB.h"
#include "CookieRequestHeaderFieldProxy.h"
#include "CookieStoreGetOptions.h"
#include "CurlContext.h"
#include "HTTPCookieAcceptPolicy.h"
#include <optional>
#include <wtf/FileSystem.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static String defaultCookieJarPath()
{
    static constexpr auto defaultFileName = "cookie.jar.db"_s;
    char* cookieJarPath = getenv("CURL_COOKIE_JAR_PATH");
    if (cookieJarPath)
        return String::fromUTF8(cookieJarPath);

#if PLATFORM(WIN)
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), defaultFileName);
#else
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=192417
    return defaultFileName;
#endif
}

static String alternativeServicesStorageFile(const String& alternativeServicesDirectory)
{
    static constexpr auto defaultFileName = "altsvc-cache.txt"_s;

    return FileSystem::pathByAppendingComponent(alternativeServicesDirectory, defaultFileName);
}

static std::pair<String, bool> cookiesForSession(const NetworkStorageSession& session, const URL& firstParty, const URL& url, bool forHTTPHeader, IncludeSecureCookies includeSecureCookies)
{
    StringBuilder cookies;
    bool didAccessSecureCookies = false;
    auto searchHTTPOnly = forHTTPHeader ? std::nullopt : std::optional<bool> { false };
    auto secure = includeSecureCookies == IncludeSecureCookies::Yes ? std::nullopt : std::optional<bool> { false };

    if (auto result = session.cookieDatabase().searchCookies(firstParty, url, searchHTTPOnly, secure, std::nullopt)) {
        for (const auto& cookie : *result) {
            if (!cookies.isEmpty())
                cookies.append("; "_s);
            if (!cookie.name.isEmpty())
                cookies.append(cookie.name, '=');
            if (cookie.secure)
                didAccessSecureCookies = true;
            cookies.append(cookie.value);
        }
    }

    return { cookies.toString(), didAccessSecureCookies };
}

NetworkStorageSession::NetworkStorageSession(PAL::SessionID sessionID, const String& alternativeServicesDirectory)
    : m_sessionID(sessionID)
    // :memory: creates in-memory database, see https://www.sqlite.org/inmemorydb.html
    , m_cookieDatabase(makeUniqueRef<CookieJarDB>(sessionID.isEphemeral() ? ":memory:"_s : defaultCookieJarPath()))
{
    if (!alternativeServicesDirectory.isEmpty()) {
        FileSystem::makeAllDirectories(alternativeServicesDirectory);
        CurlContext::singleton().setAlternativeServicesStorageFile(alternativeServicesStorageFile(alternativeServicesDirectory));
    }
}

NetworkStorageSession::~NetworkStorageSession()
{
    clearCookiesVersionChangeCallbacks();
}

void NetworkStorageSession::setCookieDatabase(UniqueRef<CookieJarDB>&& cookieDatabase)
{
    m_cookieDatabase = WTFMove(cookieDatabase);
}

CookieJarDB& NetworkStorageSession::cookieDatabase() const
{
    m_cookieDatabase->open();
    return m_cookieDatabase;
}

void NetworkStorageSession::setCookiesFromDOM(const URL& firstParty, const SameSiteInfo&, const URL& url, std::optional<FrameIdentifier>, std::optional<PageIdentifier> pageID, ApplyTrackingPrevention, RequiresScriptTrackingPrivacy requiresScriptTrackingPrivacy, const String& value, ShouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker) const
{
    auto cappedLifetime = clientSideCookieCap(RegistrableDomain { firstParty }, requiresScriptTrackingPrivacy, pageID);
    cookieDatabase().setCookie(firstParty, url, value, CookieJarDB::Source::Script, cappedLifetime);
}

bool NetworkStorageSession::setCookieFromDOM(const URL&, const SameSiteInfo&, const URL&, std::optional<FrameIdentifier>, std::optional<PageIdentifier>, ApplyTrackingPrevention, RequiresScriptTrackingPrivacy, const Cookie&, ShouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker) const
{
    // FIXME: Implement for the Cookie Store API.
    return false;
}

void NetworkStorageSession::setCookiesFromHTTPResponse(const URL& firstParty, const URL& url, const String& value) const
{
    cookieDatabase().setCookie(firstParty, url, value, CookieJarDB::Source::Network);
}

void NetworkStorageSession::setCookieAcceptPolicy(CookieAcceptPolicy policy) const
{
    cookieDatabase().setAcceptPolicy(policy);
}

HTTPCookieAcceptPolicy NetworkStorageSession::cookieAcceptPolicy() const
{
    switch (cookieDatabase().acceptPolicy()) {
    case CookieAcceptPolicy::Always:
        return HTTPCookieAcceptPolicy::AlwaysAccept;
    case CookieAcceptPolicy::Never:
        return HTTPCookieAcceptPolicy::Never;
    case CookieAcceptPolicy::OnlyFromMainDocumentDomain:
        return HTTPCookieAcceptPolicy::OnlyFromMainDocumentDomain;
    case CookieAcceptPolicy::ExclusivelyFromMainDocumentDomain:
        return HTTPCookieAcceptPolicy::ExclusivelyFromMainDocumentDomain;
    }

    ASSERT_NOT_REACHED();
    return HTTPCookieAcceptPolicy::OnlyFromMainDocumentDomain;
}

std::pair<String, bool> NetworkStorageSession::cookiesForDOM(const URL& firstParty, const SameSiteInfo&, const URL& url, std::optional<FrameIdentifier>, std::optional<PageIdentifier>, IncludeSecureCookies includeSecureCookies, ApplyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker) const
{
    return cookiesForSession(*this, firstParty, url, false, includeSecureCookies);
}

std::optional<Vector<Cookie>> NetworkStorageSession::cookiesForDOMAsVector(const URL&, const SameSiteInfo&, const URL&, std::optional<FrameIdentifier>, std::optional<PageIdentifier>, IncludeSecureCookies, ApplyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker, CookieStoreGetOptions&&) const
{
    // FIXME: Implement for the Cookie Store API.
    return std::nullopt;
}

void NetworkStorageSession::setCookies(const Vector<Cookie>& cookies, const URL&, const URL& /* mainDocumentURL */)
{
    for (const auto& cookie : cookies)
        cookieDatabase().setCookie(cookie);
}

void NetworkStorageSession::setCookie(const Cookie& cookie)
{
    cookieDatabase().setCookie(cookie);
}

void NetworkStorageSession::setCookie(const Cookie& cookie, const URL&, const URL&)
{
    setCookie(cookie);
}

void NetworkStorageSession::deleteCookie(const Cookie& cookie, CompletionHandler<void()>&& completionHandler)
{
    String url = makeString(cookie.secure ? "https"_s : "http"_s, "://"_s, cookie.domain, cookie.path);
    cookieDatabase().deleteCookie(url, cookie.name);
    completionHandler();
}

void NetworkStorageSession::deleteCookie(const URL&, const URL& url, const String& name, CompletionHandler<void()>&& completionHandler) const
{
    cookieDatabase().deleteCookie(url.string(), name);
    completionHandler();
}

void NetworkStorageSession::deleteAllCookies(CompletionHandler<void()>&& completionHandler)
{
    cookieDatabase().deleteAllCookies();
    completionHandler();
}

void NetworkStorageSession::deleteAllCookiesModifiedSince(WallTime, CompletionHandler<void()>&& completionHandler)
{
    // FIXME: Not yet implemented
    completionHandler();
}

void NetworkStorageSession::deleteCookiesForHostnames(const Vector<String>& cookieHostNames, IncludeHttpOnlyCookies includeHttpOnlyCookies, ScriptWrittenCookiesOnly, CompletionHandler<void()>&& completionHandler)
{
    for (auto hostname : cookieHostNames)
        cookieDatabase().deleteCookiesForHostname(hostname, includeHttpOnlyCookies);
    completionHandler();
}

Vector<Cookie> NetworkStorageSession::getAllCookies()
{
    return cookieDatabase().getAllCookies();
}

void NetworkStorageSession::getHostnamesWithCookies(HashSet<String>& hostnames)
{
    hostnames = cookieDatabase().allDomains();
}

Vector<Cookie> NetworkStorageSession::getCookies(const URL&)
{
    // FIXME: Implement for WebKit to use.
    return { };
}

void NetworkStorageSession::hasCookies(const RegistrableDomain&, CompletionHandler<void(bool)>&& completionHandler) const
{
    // FIXME: Implement.
    completionHandler(false);
}

bool NetworkStorageSession::getRawCookies(const URL& firstParty, const SameSiteInfo&, const URL& url, std::optional<FrameIdentifier>, std::optional<PageIdentifier>, ApplyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking, Vector<Cookie>& rawCookies) const
{
    auto cookies = cookieDatabase().searchCookies(firstParty, url, std::nullopt, std::nullopt, std::nullopt);
    if (!cookies)
        return false;

    rawCookies = WTFMove(*cookies);
    return true;
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const URL& firstParty, const SameSiteInfo&, const URL& url, std::optional<FrameIdentifier>, std::optional<PageIdentifier>, IncludeSecureCookies includeSecureCookies, ApplyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking, IsKnownCrossSiteTracker) const
{
    return cookiesForSession(*this, firstParty, url, true, includeSecureCookies);
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const CookieRequestHeaderFieldProxy& headerFieldProxy) const
{
    return cookieRequestHeaderFieldValue(headerFieldProxy.firstParty, headerFieldProxy.sameSiteInfo, headerFieldProxy.url, headerFieldProxy.frameID, headerFieldProxy.pageID, headerFieldProxy.includeSecureCookies, ApplyTrackingPrevention::Yes, ShouldRelaxThirdPartyCookieBlocking::No, IsKnownCrossSiteTracker::No);
}

void NetworkStorageSession::setProxySettings(const CurlProxySettings& proxySettings)
{
    CurlContext::singleton().setProxySettings(proxySettings);
}

void NetworkStorageSession::clearAlternativeServices()
{
    CurlContext::singleton().clearAlternativeServicesStorageFile();
}

} // namespace WebCore

#endif // USE(CURL)
