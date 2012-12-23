/*
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
#include "PlatformCookieJar.h"

#include "Cookie.h"
#include "KURL.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static HashMap<String, String> cookieJar;

void setCookiesFromDOM(const NetworkStorageSession&, const KURL&, const KURL& url, const String& value)
{
    cookieJar.set(url.string(), value);
}

String cookiesForDOM(const NetworkStorageSession&, const KURL&, const KURL& url)
{
    return cookieJar.get(url.string());
}

String cookieRequestHeaderFieldValue(const NetworkStorageSession&, const KURL& /*firstParty*/, const KURL& url)
{
    // FIXME: include HttpOnly cookie.
    return cookieJar.get(url.string());
}

bool cookiesEnabled(const NetworkStorageSession&, const KURL& /*firstParty*/, const KURL& /*url*/)
{
    return true;
}

bool getRawCookies(const NetworkStorageSession&, const KURL& /*firstParty*/, const KURL& /*url*/, Vector<Cookie>& rawCookies)
{
    // FIXME: Not yet implemented
    rawCookies.clear();
    return false; // return true when implemented
}

void deleteCookie(const NetworkStorageSession&, const KURL&, const String&)
{
    // FIXME: Not yet implemented
}

void getHostnamesWithCookies(const NetworkStorageSession&, HashSet<String>& hostnames)
{
    // FIXME: Not yet implemented
}

void deleteCookiesForHostname(const NetworkStorageSession&, const String& hostname)
{
    // FIXME: Not yet implemented
}

void deleteAllCookies(const NetworkStorageSession&)
{
    // FIXME: Not yet implemented
}

}
