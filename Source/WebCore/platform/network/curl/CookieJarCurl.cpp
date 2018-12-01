/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CookieJarCurl.h"

#if USE(CURL)
#include "Cookie.h"
#include "CookieJarDB.h"
#include "CookieRequestHeaderFieldProxy.h"
#include "NetworkStorageSession.h"
#include "NotImplemented.h"

#include <wtf/Optional.h>
#include <wtf/URL.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static String cookiesForSession(const NetworkStorageSession& session, const URL&, const URL& url, bool forHTTPHeader)
{
    StringBuilder cookies;

    CookieJarDB& cookieJarDB = session.cookieDatabase();
    auto searchHTTPOnly = (forHTTPHeader ? std::nullopt : std::optional<bool> {false});
    auto secure = url.protocolIs("https") ? std::nullopt : std::optional<bool> {false};

    if (auto result = cookieJarDB.searchCookies(url.string(), searchHTTPOnly, secure, std::nullopt)) {
        for (auto& cookie : *result) {
            if (!cookies.isEmpty())
                cookies.append("; ");
            cookies.append(cookie.name);
            cookies.append("=");
            cookies.append(cookie.value);
        }
    }
    return cookies.toString();
}

void CookieJarCurl::setCookiesFromDOM(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo&, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, const String& value) const
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    UNUSED_PARAM(firstParty);

    CookieJarDB& cookieJarDB = session.cookieDatabase();
    cookieJarDB.setCookie(url.string(), value, CookieJarDB::Source::Script);
}

void CookieJarCurl::setCookiesFromHTTPResponse(const NetworkStorageSession& session, const URL& url, const String& value) const
{
    CookieJarDB& cookieJarDB = session.cookieDatabase();
    cookieJarDB.setCookie(url.string(), value, CookieJarDB::Source::Network);
}

std::pair<String, bool> CookieJarCurl::cookiesForDOM(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo&, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies) const
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);

    // FIXME: This should filter secure cookies out if the caller requests it.
    return { cookiesForSession(session, firstParty, url, false), false };
}

std::pair<String, bool> CookieJarCurl::cookieRequestHeaderFieldValue(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo&, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies) const
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);

    // FIXME: This should filter secure cookies out if the caller requests it.
    return { cookiesForSession(session, firstParty, url, true), false };
}

std::pair<String, bool> CookieJarCurl::cookieRequestHeaderFieldValue(const NetworkStorageSession& session, const CookieRequestHeaderFieldProxy& headerFieldProxy) const
{
    return cookieRequestHeaderFieldValue(session, headerFieldProxy.firstParty, headerFieldProxy.sameSiteInfo, headerFieldProxy.url, headerFieldProxy.frameID, headerFieldProxy.pageID, headerFieldProxy.includeSecureCookies);
}

bool CookieJarCurl::cookiesEnabled(const NetworkStorageSession& session) const
{
    return session.cookieDatabase().isEnabled();
}

bool CookieJarCurl::getRawCookies(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo&, const URL&, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, Vector<Cookie>& rawCookies) const
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);

    CookieJarDB& cookieJarDB = session.cookieDatabase();
    if (auto cookies = cookieJarDB.searchCookies(firstParty.string(), std::nullopt, std::nullopt, std::nullopt)) {
        rawCookies = WTFMove(*cookies);
        return true;
    }
    return false;
}

void CookieJarCurl::deleteCookie(const NetworkStorageSession& session, const URL& url, const String& name) const
{
    CookieJarDB& cookieJarDB = session.cookieDatabase();
    cookieJarDB.deleteCookie(url, name);
}

void CookieJarCurl::getHostnamesWithCookies(const NetworkStorageSession&, HashSet<String>&) const
{
    // FIXME: Not yet implemented
}

void CookieJarCurl::deleteCookiesForHostnames(const NetworkStorageSession&, const Vector<String>&) const
{
    // FIXME: Not yet implemented
}

void CookieJarCurl::deleteAllCookies(const NetworkStorageSession& session) const
{
    CookieJarDB& cookieJarDB = session.cookieDatabase();
    cookieJarDB.deleteAllCookies();
}

void CookieJarCurl::deleteAllCookiesModifiedSince(const NetworkStorageSession&, WallTime) const
{
    // FIXME: Not yet implemented
}

} // namespace WebCore

#endif // USE(CURL)
