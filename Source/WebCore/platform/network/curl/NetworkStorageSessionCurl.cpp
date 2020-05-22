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

#include "CookieJarDB.h"
#include "CookieRequestHeaderFieldProxy.h"
#include "CurlContext.h"
#include "HTTPCookieAcceptPolicy.h"
#include <wtf/FileSystem.h>
#include <wtf/URL.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static String defaultCookieJarPath()
{
    static const char* defaultFileName = "cookie.jar.db";
    char* cookieJarPath = getenv("CURL_COOKIE_JAR_PATH");
    if (cookieJarPath)
        return cookieJarPath;

#if PLATFORM(WIN)
    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), defaultFileName);
#else
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=192417
    return defaultFileName;
#endif
}

static String cookiesForSession(const NetworkStorageSession& session, const URL& firstParty, const URL& url, bool forHTTPHeader)
{
    StringBuilder cookies;

    auto searchHTTPOnly = forHTTPHeader ? WTF::nullopt : Optional<bool> { false };
    auto secure = url.protocolIs("https") ? WTF::nullopt : Optional<bool> { false };

    if (auto result = session.cookieDatabase().searchCookies(firstParty, url, searchHTTPOnly, secure, WTF::nullopt)) {
        for (const auto& cookie : *result) {
            if (!cookies.isEmpty())
                cookies.append("; ");
            cookies.append(cookie.name);
            cookies.append("=");
            cookies.append(cookie.value);
        }
    }

    return cookies.toString();
}

NetworkStorageSession::NetworkStorageSession(PAL::SessionID sessionID)
    : m_sessionID(sessionID)
    // :memory: creates in-memory database, see https://www.sqlite.org/inmemorydb.html
    , m_cookieDatabase(makeUniqueRef<CookieJarDB>(sessionID.isEphemeral() ? ":memory:"_s : defaultCookieJarPath()))
{
}

NetworkStorageSession::~NetworkStorageSession()
{
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

void NetworkStorageSession::setCookiesFromDOM(const URL& firstParty, const SameSiteInfo&, const URL& url, Optional<FrameIdentifier>, Optional<PageIdentifier>, ShouldAskITP, const String& value, ShouldRelaxThirdPartyCookieBlocking) const
{
    cookieDatabase().setCookie(firstParty, url, value, CookieJarDB::Source::Script);
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

std::pair<String, bool> NetworkStorageSession::cookiesForDOM(const URL& firstParty, const SameSiteInfo&, const URL& url, Optional<FrameIdentifier>, Optional<PageIdentifier>, IncludeSecureCookies, ShouldAskITP, ShouldRelaxThirdPartyCookieBlocking) const
{
    // FIXME: This should filter secure cookies out if the caller requests it.
    return { cookiesForSession(*this, firstParty, url, false), false };
}

void NetworkStorageSession::setCookies(const Vector<Cookie>&, const URL&, const URL& /* mainDocumentURL */)
{
    // FIXME: Implement for WebKit to use.
}

void NetworkStorageSession::setCookie(const Cookie& cookie)
{
    cookieDatabase().setCookie(cookie);
}

void NetworkStorageSession::deleteCookie(const Cookie& cookie)
{
    String url = makeString(cookie.secure ? "https"_s : "http"_s, "://"_s, cookie.domain, cookie.path);
    cookieDatabase().deleteCookie(url, cookie.name);
}

void NetworkStorageSession::deleteCookie(const URL& url, const String& name) const
{
    cookieDatabase().deleteCookie(url.string(), name);
}

void NetworkStorageSession::deleteAllCookies()
{
    cookieDatabase().deleteAllCookies();
}

void NetworkStorageSession::deleteAllCookiesModifiedSince(WallTime)
{
    // FIXME: Not yet implemented
}

void NetworkStorageSession::deleteCookiesForHostnames(const Vector<String>& /* cookieHostNames */, IncludeHttpOnlyCookies)
{
    // FIXME: Not yet implemented.
}

void NetworkStorageSession::deleteCookiesForHostnames(const Vector<String>& cookieHostNames)
{
    deleteCookiesForHostnames(cookieHostNames, IncludeHttpOnlyCookies::Yes);
}

Vector<Cookie> NetworkStorageSession::getAllCookies()
{
    return cookieDatabase().getAllCookies();
}

void NetworkStorageSession::getHostnamesWithCookies(HashSet<String>& /* hostnames */)
{
    // FIXME: Not yet implemented
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

bool NetworkStorageSession::getRawCookies(const URL& firstParty, const SameSiteInfo&, const URL& url, Optional<FrameIdentifier>, Optional<PageIdentifier>, ShouldAskITP, ShouldRelaxThirdPartyCookieBlocking, Vector<Cookie>& rawCookies) const
{
    auto cookies = cookieDatabase().searchCookies(firstParty, url, WTF::nullopt, WTF::nullopt, WTF::nullopt);
    if (!cookies)
        return false;

    rawCookies = WTFMove(*cookies);
    return true;
}

void NetworkStorageSession::flushCookieStore()
{
    // FIXME: Implement for WebKit to use.
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const URL& firstParty, const SameSiteInfo&, const URL& url, Optional<FrameIdentifier>, Optional<PageIdentifier>, IncludeSecureCookies, ShouldAskITP, ShouldRelaxThirdPartyCookieBlocking) const
{
    // FIXME: This should filter secure cookies out if the caller requests it.
    return { cookiesForSession(*this, firstParty, url, true), false };
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const CookieRequestHeaderFieldProxy& headerFieldProxy) const
{
    return cookieRequestHeaderFieldValue(headerFieldProxy.firstParty, headerFieldProxy.sameSiteInfo, headerFieldProxy.url, headerFieldProxy.frameID, headerFieldProxy.pageID, headerFieldProxy.includeSecureCookies, ShouldAskITP::Yes, ShouldRelaxThirdPartyCookieBlocking::No);
}

void NetworkStorageSession::setProxySettings(CurlProxySettings&& proxySettings)
{
    CurlContext::singleton().setProxySettings(WTFMove(proxySettings));
}

} // namespace WebCore

#endif // USE(CURL)
