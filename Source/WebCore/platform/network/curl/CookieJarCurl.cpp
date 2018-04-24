/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
#include "CookieJarCurl.h"

#if USE(CURL)
#include "Cookie.h"
#include "CookieRequestHeaderFieldProxy.h"
#include "NetworkStorageSession.h"
#include "URL.h"

#include <wtf/text/WTFString.h>

namespace WebCore {

std::pair<String, bool> cookiesForDOM(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies)
{
    return session.cookieStorage().cookiesForDOM(session, firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies);
}

void setCookiesFromDOM(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, const String& value)
{
    session.cookieStorage().setCookiesFromDOM(session, firstParty, sameSiteInfo, url, frameID, pageID, value);
}

std::pair<String, bool> cookieRequestHeaderFieldValue(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies)
{
    return session.cookieStorage().cookieRequestHeaderFieldValue(session, firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies);
}

std::pair<String, bool> cookieRequestHeaderFieldValue(const NetworkStorageSession& session, const CookieRequestHeaderFieldProxy& headerFieldProxy)
{
    return session.cookieStorage().cookieRequestHeaderFieldValue(session, headerFieldProxy.firstParty, headerFieldProxy.sameSiteInfo, headerFieldProxy.url, headerFieldProxy.frameID, headerFieldProxy.pageID, headerFieldProxy.includeSecureCookies);
}

bool cookiesEnabled(const NetworkStorageSession& session)
{
    return session.cookieStorage().cookiesEnabled(session);
}

bool getRawCookies(const NetworkStorageSession& session, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, Vector<Cookie>& rawCookies)
{
    return session.cookieStorage().getRawCookies(session, firstParty, sameSiteInfo, url, frameID, pageID, rawCookies);
}

void deleteCookie(const NetworkStorageSession& session, const URL& url, const String& cookie)
{
    session.cookieStorage().deleteCookie(session, url, cookie);
}

void getHostnamesWithCookies(const NetworkStorageSession& session, HashSet<String>& hostnames)
{
    session.cookieStorage().getHostnamesWithCookies(session, hostnames);
}

void deleteCookiesForHostnames(const NetworkStorageSession& session, const Vector<String>& cookieHostNames)
{
    session.cookieStorage().deleteCookiesForHostnames(session, cookieHostNames);
}

void deleteAllCookies(const NetworkStorageSession& session)
{
    session.cookieStorage().deleteAllCookies(session);
}

void deleteAllCookiesModifiedSince(const NetworkStorageSession& session, WallTime since)
{
    session.cookieStorage().deleteAllCookiesModifiedSince(session, since);
}

}

#endif
