/*
    Copyright (C) 2010 ProFUSION embedded systems
    Copyright (C) 2010 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "ewk_cookies.h"

#include "ResourceHandle.h"
#include <Eina.h>
#include <eina_safety_checks.h>
#include <wtf/text/CString.h>

#if USE(SOUP)
#include "CookieJarSoup.h"
#include <glib.h>
#include <libsoup/soup.h>
#endif

Eina_Bool ewk_cookies_file_set(const char* filename)
{
#if USE(SOUP)
    SoupCookieJar* cookieJar = 0;
    if (filename)
        cookieJar = soup_cookie_jar_text_new(filename, FALSE);
    else
        cookieJar = soup_cookie_jar_new();

    if (!cookieJar)
        return false;

    soup_cookie_jar_set_accept_policy(cookieJar, SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY);

    SoupSession* session = WebCore::ResourceHandle::defaultSession();
    SoupSessionFeature* oldjar = soup_session_get_feature(session, SOUP_TYPE_COOKIE_JAR);
    if (oldjar)
        soup_session_remove_feature(session, oldjar);

    WebCore::setDefaultCookieJar(cookieJar);
    soup_session_add_feature(session, SOUP_SESSION_FEATURE(cookieJar));

    return true;
#else
    return false;
#endif
}

void ewk_cookies_clear(void)
{
#if USE(SOUP)
    GSList* list;
    GSList* p;
    SoupCookieJar* cookieJar = WebCore::defaultCookieJar();

    list = soup_cookie_jar_all_cookies(cookieJar);
    for (p = list; p; p = p->next)
        soup_cookie_jar_delete_cookie(cookieJar, (SoupCookie*)p->data);

    soup_cookies_free(list);
#endif
}

Eina_List* ewk_cookies_get_all(void)
{
    Eina_List* result = 0;
#if USE(SOUP)
    GSList* list;
    GSList* p;
    SoupCookieJar* cookieJar = WebCore::defaultCookieJar();

    list = soup_cookie_jar_all_cookies(cookieJar);
    for (p = list; p; p = p->next) {
        SoupCookie* cookie = static_cast<SoupCookie*>(p->data);
        Ewk_Cookie* ewkCookie = static_cast<Ewk_Cookie*>(malloc(sizeof(*ewkCookie)));
        ewkCookie->name = strdup(cookie->name);
        ewkCookie->value = strdup(cookie->value);
        ewkCookie->domain = strdup(cookie->domain);
        ewkCookie->path = strdup(cookie->path);
        ewkCookie->expires = soup_date_to_time_t(cookie->expires);
        ewkCookie->secure = static_cast<Eina_Bool>(cookie->secure);
        ewkCookie->http_only = static_cast<Eina_Bool>(cookie->http_only);
        result = eina_list_append(result, ewkCookie);
    }

    soup_cookies_free(list);
#endif
    return result;
}

void ewk_cookies_cookie_del(Ewk_Cookie* cookie)
{
#if USE(SOUP)
    EINA_SAFETY_ON_NULL_RETURN(cookie);
    GSList* list;
    GSList* p;
    SoupCookieJar* cookieJar = WebCore::defaultCookieJar();
    SoupCookie* cookie1 = soup_cookie_new(
        cookie->name, cookie->value, cookie->domain, cookie->path, -1);

    list = soup_cookie_jar_all_cookies(cookieJar);
    for (p = list; p; p = p->next) {
        SoupCookie* cookie2 = static_cast<SoupCookie*>(p->data);
        if (soup_cookie_equal(cookie1, cookie2)) {
            soup_cookie_jar_delete_cookie(cookieJar, cookie2);
            break;
        }
    }

    soup_cookie_free(cookie1);
    soup_cookies_free(list);
#endif
}

void ewk_cookies_cookie_free(Ewk_Cookie* cookie)
{
#if USE(SOUP)
    EINA_SAFETY_ON_NULL_RETURN(cookie);
    free(cookie->name);
    free(cookie->value);
    free(cookie->domain);
    free(cookie->path);
    free(cookie);
#endif
}

void ewk_cookies_policy_set(Ewk_Cookie_Policy cookiePolicy)
{
#if USE(SOUP)
    SoupCookieJar* cookieJar = WebCore::defaultCookieJar();
    SoupCookieJarAcceptPolicy policy;

    policy = SOUP_COOKIE_JAR_ACCEPT_ALWAYS;
    switch (cookiePolicy) {
    case EWK_COOKIE_JAR_ACCEPT_NEVER:
        policy = SOUP_COOKIE_JAR_ACCEPT_NEVER;
        break;
    case EWK_COOKIE_JAR_ACCEPT_ALWAYS:
        policy = SOUP_COOKIE_JAR_ACCEPT_ALWAYS;
        break;
    case EWK_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY:
        policy = SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY;
        break;
    }

    soup_cookie_jar_set_accept_policy(cookieJar, policy);
#endif
}

Ewk_Cookie_Policy ewk_cookies_policy_get(void)
{
    Ewk_Cookie_Policy ewkPolicy = EWK_COOKIE_JAR_ACCEPT_ALWAYS;
#if USE(SOUP)
    SoupCookieJar* cookieJar = WebCore::defaultCookieJar();
    SoupCookieJarAcceptPolicy policy;

    policy = soup_cookie_jar_get_accept_policy(cookieJar);
    switch (policy) {
    case SOUP_COOKIE_JAR_ACCEPT_NEVER:
        ewkPolicy = EWK_COOKIE_JAR_ACCEPT_NEVER;
        break;
    case SOUP_COOKIE_JAR_ACCEPT_ALWAYS:
        ewkPolicy = EWK_COOKIE_JAR_ACCEPT_ALWAYS;
        break;
    case SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY:
        ewkPolicy = EWK_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY;
        break;
    }
#endif

    return ewkPolicy;
}
