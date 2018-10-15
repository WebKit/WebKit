/*
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#pragma once

#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class URL;
class NetworkStorageSession;

struct Cookie;
struct CookieRequestHeaderFieldProxy;
struct SameSiteInfo;

enum class IncludeSecureCookies : bool;

class CookieJarCurl {
public:
    virtual std::pair<String, bool> cookiesForDOM(const NetworkStorageSession&, const URL& firstParty, const SameSiteInfo&, const URL&, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies) const = 0;
    virtual void setCookiesFromDOM(const NetworkStorageSession&, const URL& firstParty, const SameSiteInfo&, const URL&, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, const String&) const = 0;
    virtual void setCookiesFromHTTPResponse(const NetworkStorageSession&, const URL&, const String&) const = 0;
    virtual bool cookiesEnabled(const NetworkStorageSession&) const = 0;
    virtual std::pair<String, bool> cookieRequestHeaderFieldValue(const NetworkStorageSession&, const URL& firstParty, const SameSiteInfo&, const URL&, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, IncludeSecureCookies) const = 0;
    virtual std::pair<String, bool> cookieRequestHeaderFieldValue(const NetworkStorageSession&, const CookieRequestHeaderFieldProxy&) const = 0;
    virtual bool getRawCookies(const NetworkStorageSession&, const URL& firstParty, const SameSiteInfo&, const URL&, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, Vector<Cookie>&) const = 0;
    virtual void deleteCookie(const NetworkStorageSession&, const URL&, const String&) const = 0;
    virtual void getHostnamesWithCookies(const NetworkStorageSession&, HashSet<String>& hostnames) const = 0;
    virtual void deleteCookiesForHostnames(const NetworkStorageSession&, const Vector<String>& cookieHostNames) const = 0;
    virtual void deleteAllCookies(const NetworkStorageSession&) const = 0;
    virtual void deleteAllCookiesModifiedSince(const NetworkStorageSession&, WallTime) const = 0;
};

} // namespace WebCore
