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

#if USE(SOUP)
#include "CookieJarSoup.h"
#endif
#include "EWebKit.h"
#include "ResourceHandle.h"

#include <Eina.h>
#include <eina_safety_checks.h>
#if USE(SOUP)
#include <glib.h>
#include <libsoup/soup.h>
#endif
#include <wtf/text/CString.h>


/**
 * Sets the path where the cookies are going to be stored. Use @c NULL for keep
 * them just in memory.
 *
 * @param filename path to the cookies.txt file.
 *
 * @return @c EINA_FALSE if it wasn't possible to create the cookie jar,
 *          @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_cookies_file_set(const char *filename)
{
#if USE(SOUP)
    SoupCookieJar* cookieJar = 0;
    if (filename)
        cookieJar = soup_cookie_jar_text_new(filename, FALSE);
    else
        cookieJar = soup_cookie_jar_new();

    if (!cookieJar)
        return EINA_FALSE;

    soup_cookie_jar_set_accept_policy(cookieJar, SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY);

    SoupSession* session = WebCore::ResourceHandle::defaultSession();
    SoupSessionFeature* oldjar = soup_session_get_feature(session, SOUP_TYPE_COOKIE_JAR);
    if (oldjar)
        soup_session_remove_feature(session, oldjar);

    WebCore::setDefaultCookieJar(cookieJar);
    soup_session_add_feature(session, SOUP_SESSION_FEATURE(cookieJar));

    return EINA_TRUE;
#else
    return EINA_FALSE;
#endif
}

/**
 * Clears all the cookies from the cookie jar.
 */
void ewk_cookies_clear(void)
{
#if USE(SOUP)
    GSList* l;
    GSList* p;
    SoupCookieJar* cookieJar = WebCore::defaultCookieJar();

    l = soup_cookie_jar_all_cookies(cookieJar);
    for (p = l; p; p = p->next)
        soup_cookie_jar_delete_cookie(cookieJar, (SoupCookie*)p->data);

    soup_cookies_free(l);
#endif
}

/**
 * Returns a list of cookies in the cookie jar.
 *
 * @return an @c Eina_List with all the cookies in the cookie jar.
 */
Eina_List* ewk_cookies_get_all(void)
{
    Eina_List* el = 0;
#if USE(SOUP)
    GSList* l;
    GSList* p;
    SoupCookieJar* cookieJar = WebCore::defaultCookieJar();

    l = soup_cookie_jar_all_cookies(cookieJar);
    for (p = l; p; p = p->next) {
        SoupCookie* cookie = static_cast<SoupCookie*>(p->data);
        Ewk_Cookie* c = static_cast<Ewk_Cookie*>(malloc(sizeof(*c)));
        c->name = strdup(cookie->name);
        c->value = strdup(cookie->value);
        c->domain = strdup(cookie->domain);
        c->path = strdup(cookie->path);
        c->expires = soup_date_to_time_t(cookie->expires);
        c->secure = static_cast<Eina_Bool>(cookie->secure);
        c->http_only = static_cast<Eina_Bool>(cookie->http_only);
        el = eina_list_append(el, c);
    }

    soup_cookies_free(l);
#endif
    return el;
}

/**
 * Deletes a cookie from the cookie jar.
 *
 * Note that the fields name, value, domain and path are used to match this
 * cookie in the cookie jar.
 *
 * @param cookie an @c Ewk_Cookie that has the info relative to that cookie.
 */
void ewk_cookies_cookie_del(Ewk_Cookie *cookie)
{
#if USE(SOUP)
    EINA_SAFETY_ON_NULL_RETURN(cookie);
    GSList* l;
    GSList* p;
    SoupCookieJar* cookieJar = WebCore::defaultCookieJar();
    SoupCookie* c1 = soup_cookie_new(
        cookie->name, cookie->value, cookie->domain, cookie->path, -1);

    l = soup_cookie_jar_all_cookies(cookieJar);
    for (p = l; p; p = p->next) {
        SoupCookie* c2 = static_cast<SoupCookie*>(p->data);
        if (soup_cookie_equal(c1, c2)) {
            soup_cookie_jar_delete_cookie(cookieJar, c2);
            break;
        }
    }

    soup_cookie_free(c1);
    soup_cookies_free(l);
#endif
}

/**
 * Frees the memory used by a cookie.
 *
 * @param cookie the Ewk_Cookie struct that will be freed.
 */
void ewk_cookies_cookie_free(Ewk_Cookie *cookie)
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

/**
 * Sets the cookies accept policy.
 *
 * @param p the acceptance policy
 * @see Ewk_Cookie_Policy
 */
void ewk_cookies_policy_set(Ewk_Cookie_Policy p)
{
#if USE(SOUP)
    SoupCookieJar* cookieJar = WebCore::defaultCookieJar();
    SoupCookieJarAcceptPolicy policy;

    policy = SOUP_COOKIE_JAR_ACCEPT_ALWAYS;
    switch (p) {
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

/**
 * Gets the acceptance policy used in the current cookie jar.
 *
 * @return the current acceptance policy
 * @see Ewk_Cookie_Policy
 */
Ewk_Cookie_Policy ewk_cookies_policy_get(void)
{
    Ewk_Cookie_Policy ewk_policy = EWK_COOKIE_JAR_ACCEPT_ALWAYS;
#if USE(SOUP)
    SoupCookieJar* cookieJar = WebCore::defaultCookieJar();
    SoupCookieJarAcceptPolicy policy;

    policy = soup_cookie_jar_get_accept_policy(cookieJar);
    switch (policy) {
    case SOUP_COOKIE_JAR_ACCEPT_NEVER:
        ewk_policy = EWK_COOKIE_JAR_ACCEPT_NEVER;
        break;
    case SOUP_COOKIE_JAR_ACCEPT_ALWAYS:
        ewk_policy = EWK_COOKIE_JAR_ACCEPT_ALWAYS;
        break;
    case SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY:
        ewk_policy = EWK_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY;
        break;
    }
#endif

    return ewk_policy;
}
