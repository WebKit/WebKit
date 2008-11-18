/*
 *  Copyright (C) 2008 Xan Lopez <xan@gnome.org>
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

#include "CString.h"
#include "KURL.h"

namespace WebCore {

SoupCookieJar* getCookieJar()
{
    static SoupCookieJar* jar = soup_cookie_jar_new();
    return jar;
}

void setCookies(Document* /*document*/, const KURL& url, const KURL& /*policyURL*/, const String& value)
{
    SoupCookieJar* jar = getCookieJar();
    if (!jar)
        return;

    SoupURI* origin = soup_uri_new(url.string().utf8().data());

    soup_cookie_jar_set_cookie(jar, origin, value.utf8().data());
    soup_uri_free(origin);
}

String cookies(const Document* /*document*/, const KURL& url)
{
    SoupCookieJar* jar = getCookieJar();
    if (!jar)
        return String();

    SoupURI* uri = soup_uri_new(url.string().utf8().data());
    char* cookies = soup_cookie_jar_get_cookies(jar, uri, FALSE);
    soup_uri_free(uri);

    String result(String::fromUTF8(cookies));
    g_free(cookies);

    return result;
}

bool cookiesEnabled(const Document* /*document*/)
{
    return getCookieJar();
}

}
