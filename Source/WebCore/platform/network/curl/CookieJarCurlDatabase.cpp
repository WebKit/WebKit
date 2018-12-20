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
#include "CookieJarCurlDatabase.h"

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
    auto searchHTTPOnly = (forHTTPHeader ? WTF::nullopt : Optional<bool> {false});
    auto secure = url.protocolIs("https") ? WTF::nullopt : Optional<bool> {false};

    Vector<Cookie> results;
    if (cookieJarDB.searchCookies(url.string(), searchHTTPOnly, secure, WTF::nullopt, results)) {
        for (auto result : results) {
            if (!cookies.isEmpty())
                cookies.append("; ");
            cookies.append(result.name);
            cookies.append("=");
            cookies.append(result.value);
        }
    }
    return cookies.toString();
}

void CookieJarCurlDatabase::setCookiesFromDOM(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo&, const URL& url, Optional<uint64_t> frameID, Optional<uint64_t> pageID, const String& value) const
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    UNUSED_PARAM(firstParty);

    CookieJarDB& cookieJarDB = session.cookieDatabase();
    cookieJarDB.setCookie(url.string(), value, true);
}

void CookieJarCurlDatabase::setCookiesFromHTTPResponse(const NetworkStorageSession& session, const URL& url, const String& value) const
{
    CookieJarDB& cookieJarDB = session.cookieDatabase();
    cookieJarDB.setCookie(url.string(), value, false);
}

std::pair<String, bool> CookieJarCurlDatabase::cookiesForDOM(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo&, const URL& url, Optional<uint64_t> frameID, Optional<uint64_t> pageID, IncludeSecureCookies) const
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);

    // FIXME: This should filter secure cookies out if the caller requests it.
    return { cookiesForSession(session, firstParty, url, false), false };
}

std::pair<String, bool> CookieJarCurlDatabase::cookieRequestHeaderFieldValue(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo&, const URL& url, Optional<uint64_t> frameID, Optional<uint64_t> pageID, IncludeSecureCookies) const
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);

    // FIXME: This should filter secure cookies out if the caller requests it.
    return { cookiesForSession(session, firstParty, url, true), false };
}

std::pair<String, bool> CookieJarCurlDatabase::cookieRequestHeaderFieldValue(const NetworkStorageSession& session, const CookieRequestHeaderFieldProxy& headerFieldProxy) const
{
    return cookieRequestHeaderFieldValue(session, headerFieldProxy.firstParty, headerFieldProxy.sameSiteInfo, headerFieldProxy.url, headerFieldProxy.frameID, headerFieldProxy.pageID, headerFieldProxy.includeSecureCookies);
}

bool CookieJarCurlDatabase::cookiesEnabled(const NetworkStorageSession& session) const
{
    return session.cookieDatabase().isEnabled();
}

bool CookieJarCurlDatabase::getRawCookies(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo&, const URL&, Optional<uint64_t> frameID, Optional<uint64_t> pageID, Vector<Cookie>& rawCookies) const
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);

    CookieJarDB& cookieJarDB = session.cookieDatabase();
    return cookieJarDB.searchCookies(firstParty.string(), WTF::nullopt, WTF::nullopt, WTF::nullopt, rawCookies);
}

void CookieJarCurlDatabase::deleteCookie(const NetworkStorageSession& session, const URL& url, const String& name) const
{
    CookieJarDB& cookieJarDB = session.cookieDatabase();
    cookieJarDB.deleteCookie(url, name);
}

void CookieJarCurlDatabase::getHostnamesWithCookies(const NetworkStorageSession&, HashSet<String>&) const
{
    // FIXME: Not yet implemented
}

void CookieJarCurlDatabase::deleteCookiesForHostnames(const NetworkStorageSession&, const Vector<String>&) const
{
    // FIXME: Not yet implemented
}

void CookieJarCurlDatabase::deleteAllCookies(const NetworkStorageSession& session) const
{
    CookieJarDB& cookieJarDB = session.cookieDatabase();
    cookieJarDB.deleteAllCookies();
}

void CookieJarCurlDatabase::deleteAllCookiesModifiedSince(const NetworkStorageSession&, WallTime) const
{
    // FIXME: Not yet implemented
}

} // namespace WebCore

#endif // USE(CURL)
