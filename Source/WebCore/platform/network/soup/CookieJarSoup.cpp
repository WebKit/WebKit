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
#include "CookieJarSoup.h"

#include "Cookie.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "GOwnPtrSoup.h"
#include "KURL.h"
#include "NetworkingContext.h"
#include "ResourceHandle.h"
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

namespace WebCore {

static SoupCookieJar* cookieJarForDocument(const Document* document)
{
    if (!document)
        return 0;
    const Frame* frame = document->frame();
    if (!frame)
        return 0;
    const FrameLoader* loader = frame->loader();
    if (!loader)
        return 0;
    const NetworkingContext* context = loader->networkingContext();
    if (!context)
        return 0;
    return SOUP_COOKIE_JAR(soup_session_get_feature(context->soupSession(), SOUP_TYPE_COOKIE_JAR));
}

static GRefPtr<SoupCookieJar>& defaultCookieJar()
{
    DEFINE_STATIC_LOCAL(GRefPtr<SoupCookieJar>, cookieJar, ());
    return cookieJar;
}

SoupCookieJar* soupCookieJar()
{
    if (GRefPtr<SoupCookieJar>& jar = defaultCookieJar())
        return jar.get();

    SoupCookieJar* jar = soup_cookie_jar_new();
    soup_cookie_jar_set_accept_policy(jar, SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY);
    setSoupCookieJar(jar);
    return jar;
}

void setSoupCookieJar(SoupCookieJar* jar)
{
    defaultCookieJar() = jar;
}

void setCookies(Document* document, const KURL& url, const String& value)
{
    SoupCookieJar* jar = cookieJarForDocument(document);
    if (!jar)
        return;

    GOwnPtr<SoupURI> origin(soup_uri_new(url.string().utf8().data()));
    GOwnPtr<SoupURI> firstParty(soup_uri_new(document->firstPartyForCookies().string().utf8().data()));
    soup_cookie_jar_set_cookie_with_first_party(jar, origin.get(), firstParty.get(), value.utf8().data());
}

static String cookiesForDocument(const Document* document, const KURL& url, bool forHTTPHeader)
{
    SoupCookieJar* jar = cookieJarForDocument(document);
    if (!jar)
        return String();

    GOwnPtr<SoupURI> uri(soup_uri_new(url.string().utf8().data()));
    GOwnPtr<char> cookies(soup_cookie_jar_get_cookies(jar, uri.get(), forHTTPHeader));
    return String::fromUTF8(cookies.get());
}

String cookies(const Document* document, const KURL& url)
{
    return cookiesForDocument(document, url, false);
}

String cookieRequestHeaderFieldValue(const Document* document, const KURL& url)
{
    return cookiesForDocument(document, url, true);
}

bool cookiesEnabled(const Document* document)
{
    return !!cookieJarForDocument(document);
}

bool getRawCookies(const Document* document, const KURL& url, Vector<Cookie>& rawCookies)
{
    rawCookies.clear();
    SoupCookieJar* jar = cookieJarForDocument(document);
    if (!jar)
        return false;

    GOwnPtr<GSList> cookies(soup_cookie_jar_all_cookies(jar));
    if (!cookies)
        return false;

    GOwnPtr<SoupURI> uri(soup_uri_new(url.string().utf8().data()));
    for (GSList* iter = cookies.get(); iter; iter = g_slist_next(iter)) {
        GOwnPtr<SoupCookie> cookie(static_cast<SoupCookie*>(iter->data));
        if (!soup_cookie_applies_to_uri(cookie.get(), uri.get()))
            continue;
        // FIXME: we are currently passing false always for session because there's no API to know
        // whether SoupCookieJar is persistent or not. We could probably add soup_cookie_jar_is_persistent().
        rawCookies.append(Cookie(String::fromUTF8(cookie->name), String::fromUTF8(cookie->value), String::fromUTF8(cookie->domain),
                                 String::fromUTF8(cookie->path), static_cast<double>(soup_date_to_time_t(cookie->expires)) * 1000,
                                 cookie->http_only, cookie->secure, false));
    }

    return true;
}

void deleteCookie(const Document* document, const KURL& url, const String& name)
{
    SoupCookieJar* jar = cookieJarForDocument(document);
    if (!jar)
        return;

    GOwnPtr<GSList> cookies(soup_cookie_jar_all_cookies(jar));
    if (!cookies)
        return;

    CString cookieName = name.utf8();
    GOwnPtr<SoupURI> uri(soup_uri_new(url.string().utf8().data()));
    for (GSList* iter = cookies.get(); iter; iter = g_slist_next(iter)) {
        GOwnPtr<SoupCookie> cookie(static_cast<SoupCookie*>(iter->data));
        if (!soup_cookie_applies_to_uri(cookie.get(), uri.get()))
            continue;
        if (cookieName == cookie->name)
            soup_cookie_jar_delete_cookie(jar, cookie.get());
    }
}

void getHostnamesWithCookies(HashSet<String>& hostnames)
{
    SoupCookieJar* cookieJar = soupCookieJar();
    GOwnPtr<GSList> cookies(soup_cookie_jar_all_cookies(cookieJar));
    for (GSList* item = cookies.get(); item; item = g_slist_next(item)) {
        GOwnPtr<SoupCookie> cookie(static_cast<SoupCookie*>(item->data));
        if (!cookie->domain)
            continue;
        hostnames.add(String::fromUTF8(cookie->domain));
    }
}

void deleteCookiesForHostname(const String& hostname)
{
    CString hostNameString = hostname.utf8();
    SoupCookieJar* cookieJar = soupCookieJar();
    GOwnPtr<GSList> cookies(soup_cookie_jar_all_cookies(cookieJar));
    for (GSList* item = cookies.get(); item; item = g_slist_next(item)) {
        SoupCookie* cookie = static_cast<SoupCookie*>(item->data);
        if (soup_cookie_domain_matches(cookie, hostNameString.data()))
            soup_cookie_jar_delete_cookie(cookieJar, cookie);
        soup_cookie_free(cookie);
    }
}

void deleteAllCookies()
{
    SoupCookieJar* cookieJar = soupCookieJar();
    GOwnPtr<GSList> cookies(soup_cookie_jar_all_cookies(cookieJar));
    for (GSList* item = cookies.get(); item; item = g_slist_next(item)) {
        SoupCookie* cookie = static_cast<SoupCookie*>(item->data);
        soup_cookie_jar_delete_cookie(cookieJar, cookie);
        soup_cookie_free(cookie);
    }
}

}
