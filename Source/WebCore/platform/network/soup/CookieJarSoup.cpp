/*
 *  Copyright (C) 2008 Xan Lopez <xan@gnome.org>
 *  Copyright (C) 2009 Igalia S.L.
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if USE(SOUP)

#include "Cookie.h"
#include "CookieRequestHeaderFieldProxy.h"
#include "CookiesStrategy.h"
#include "GUniquePtrSoup.h"
#include "NetworkStorageSession.h"
#include "NetworkingContext.h"
#include "PlatformCookieJar.h"
#include "SoupNetworkSession.h"
#include "URL.h"
#include <wtf/DateMath.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

namespace WebCore {

static inline bool httpOnlyCookieExists(const GSList* cookies, const gchar* name, const gchar* path)
{
    for (const GSList* iter = cookies; iter; iter = g_slist_next(iter)) {
        SoupCookie* cookie = static_cast<SoupCookie*>(iter->data);
        if (!strcmp(soup_cookie_get_name(cookie), name)
            && !g_strcmp0(soup_cookie_get_path(cookie), path)) {
            if (soup_cookie_get_http_only(cookie))
                return true;
            break;
        }
    }
    return false;
}

void setCookiesFromDOM(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo&, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, const String& value)
{
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    GUniquePtr<SoupURI> origin = url.createSoupURI();
    if (!origin)
        return;

    GUniquePtr<SoupURI> firstPartyURI = firstParty.createSoupURI();
    if (!firstPartyURI)
        return;

    // Get existing cookies for this origin.
    SoupCookieJar* jar = session.cookieStorage();
    GSList* existingCookies = soup_cookie_jar_get_cookie_list(jar, origin.get(), TRUE);

    Vector<String> cookies;
    value.split('\n', cookies);
    const size_t cookiesCount = cookies.size();
    for (size_t i = 0; i < cookiesCount; ++i) {
        GUniquePtr<SoupCookie> cookie(soup_cookie_parse(cookies[i].utf8().data(), origin.get()));
        if (!cookie)
            continue;

        // Make sure the cookie is not httpOnly since such cookies should not be set from JavaScript.
        if (soup_cookie_get_http_only(cookie.get()))
            continue;

        // Make sure we do not overwrite httpOnly cookies from JavaScript.
        if (httpOnlyCookieExists(existingCookies, soup_cookie_get_name(cookie.get()), soup_cookie_get_path(cookie.get())))
            continue;

        soup_cookie_jar_add_cookie_with_first_party(jar, firstPartyURI.get(), cookie.release());
    }

    soup_cookies_free(existingCookies);
}

static std::pair<String, bool> cookiesForSession(const NetworkStorageSession& session, const URL& url, bool forHTTPHeader, IncludeSecureCookies includeSecureCookies)
{
    GUniquePtr<SoupURI> uri = url.createSoupURI();
    if (!uri)
        return { { }, false };

    GSList* cookies = soup_cookie_jar_get_cookie_list(session.cookieStorage(), uri.get(), forHTTPHeader);
    bool didAccessSecureCookies = false;

    // libsoup should omit secure cookies itself if the protocol is not https.
    if (url.protocolIs("https")) {
        GSList* item = cookies;
        while (item) {
            auto cookie = static_cast<SoupCookie*>(item->data);
            if (soup_cookie_get_secure(cookie)) {
                didAccessSecureCookies = true;
                if (includeSecureCookies == IncludeSecureCookies::No) {
                    GSList* next = item->next;
                    soup_cookie_free(static_cast<SoupCookie*>(item->data));
                    cookies = g_slist_remove_link(cookies, item);
                    item = next;
                    continue;
                }
            }
            item = item->next;
        }
    }

    if (!cookies)
        return { { }, false };

    GUniquePtr<char> cookieHeader(soup_cookies_to_cookie_header(cookies));
    soup_cookies_free(cookies);

    return { String::fromUTF8(cookieHeader.get()), didAccessSecureCookies };
}

std::pair<String, bool> cookiesForDOM(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo&, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies)
{
    UNUSED_PARAM(firstParty);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    return cookiesForSession(session, url, false, includeSecureCookies);
}

std::pair<String, bool> cookieRequestHeaderFieldValue(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo&, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies)
{
    UNUSED_PARAM(firstParty);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    // Secure cookies will still only be included if url's protocol is https.
    return cookiesForSession(session, url, true, includeSecureCookies);
}

std::pair<String, bool> cookieRequestHeaderFieldValue(const NetworkStorageSession& session, const CookieRequestHeaderFieldProxy& headerFieldProxy)
{
    return cookieRequestHeaderFieldValue(session, headerFieldProxy.firstParty, headerFieldProxy.sameSiteInfo, headerFieldProxy.url, headerFieldProxy.frameID, headerFieldProxy.pageID, headerFieldProxy.includeSecureCookies);
}

bool cookiesEnabled(const NetworkStorageSession& session)
{
    auto policy = soup_cookie_jar_get_accept_policy(session.cookieStorage());
    return policy == SOUP_COOKIE_JAR_ACCEPT_ALWAYS || policy == SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY;
}

bool getRawCookies(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo&, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, Vector<Cookie>& rawCookies)
{
    UNUSED_PARAM(firstParty);
    UNUSED_PARAM(frameID);
    UNUSED_PARAM(pageID);
    rawCookies.clear();
    GUniquePtr<SoupURI> uri = url.createSoupURI();
    if (!uri)
        return false;

    GUniquePtr<GSList> cookies(soup_cookie_jar_get_cookie_list(session.cookieStorage(), uri.get(), TRUE));
    if (!cookies)
        return false;

    for (GSList* iter = cookies.get(); iter; iter = g_slist_next(iter)) {
        SoupCookie* soupCookie = static_cast<SoupCookie*>(iter->data);
        Cookie cookie;
        cookie.name = String::fromUTF8(soupCookie->name);
        cookie.value = String::fromUTF8(soupCookie->value);
        cookie.domain = String::fromUTF8(soupCookie->domain);
        cookie.path = String::fromUTF8(soupCookie->path);
        cookie.created = 0;
        cookie.expires = soupCookie->expires ? static_cast<double>(soup_date_to_time_t(soupCookie->expires)) * 1000 : 0;
        cookie.httpOnly = soupCookie->http_only;
        cookie.secure = soupCookie->secure;
        cookie.session = !soupCookie->expires;
        rawCookies.append(WTFMove(cookie));
        soup_cookie_free(soupCookie);
    }

    return true;
}

void deleteCookie(const NetworkStorageSession& session, const URL& url, const String& name)
{
    GUniquePtr<SoupURI> uri = url.createSoupURI();
    if (!uri)
        return;

    SoupCookieJar* jar = session.cookieStorage();
    GUniquePtr<GSList> cookies(soup_cookie_jar_get_cookie_list(jar, uri.get(), TRUE));
    if (!cookies)
        return;

    CString cookieName = name.utf8();
    bool wasDeleted = false;
    for (GSList* iter = cookies.get(); iter; iter = g_slist_next(iter)) {
        SoupCookie* cookie = static_cast<SoupCookie*>(iter->data);
        if (!wasDeleted && cookieName == cookie->name) {
            soup_cookie_jar_delete_cookie(jar, cookie);
            wasDeleted = true;
        }
        soup_cookie_free(cookie);
    }
}

void getHostnamesWithCookies(const NetworkStorageSession& session, HashSet<String>& hostnames)
{
    GUniquePtr<GSList> cookies(soup_cookie_jar_all_cookies(session.cookieStorage()));
    for (GSList* item = cookies.get(); item; item = g_slist_next(item)) {
        SoupCookie* cookie = static_cast<SoupCookie*>(item->data);
        if (cookie->domain)
            hostnames.add(String::fromUTF8(cookie->domain));
        soup_cookie_free(cookie);
    }
}

void deleteCookiesForHostnames(const NetworkStorageSession& session, const Vector<String>& hostnames)
{
    SoupCookieJar* cookieJar = session.cookieStorage();

    for (const auto& hostname : hostnames) {
        CString hostNameString = hostname.utf8();

        GUniquePtr<GSList> cookies(soup_cookie_jar_all_cookies(cookieJar));
        for (GSList* item = cookies.get(); item; item = g_slist_next(item)) {
            SoupCookie* cookie = static_cast<SoupCookie*>(item->data);
            if (soup_cookie_domain_matches(cookie, hostNameString.data()))
                soup_cookie_jar_delete_cookie(cookieJar, cookie);
            soup_cookie_free(cookie);
        }
    }
}

void deleteAllCookies(const NetworkStorageSession& session)
{
    SoupCookieJar* cookieJar = session.cookieStorage();
    GUniquePtr<GSList> cookies(soup_cookie_jar_all_cookies(cookieJar));
    for (GSList* item = cookies.get(); item; item = g_slist_next(item)) {
        SoupCookie* cookie = static_cast<SoupCookie*>(item->data);
        soup_cookie_jar_delete_cookie(cookieJar, cookie);
        soup_cookie_free(cookie);
    }
}

void deleteAllCookiesModifiedSince(const NetworkStorageSession& session, WallTime timestamp)
{
    // FIXME: Add support for deleting cookies modified since the given timestamp. It should probably be added to libsoup.
    if (timestamp == WallTime::fromRawSeconds(0))
        deleteAllCookies(session);
    else
        g_warning("Deleting cookies modified since a given time span is not supported yet");
}

}

#endif
