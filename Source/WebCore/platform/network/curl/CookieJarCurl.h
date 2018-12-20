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

#pragma once

#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/URL.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class NetworkStorageSession;
enum class IncludeSecureCookies : bool;
struct Cookie;
struct CookieRequestHeaderFieldProxy;
struct SameSiteInfo;

class CookieJarCurl {
public:
    std::pair<String, bool> cookiesForDOM(const NetworkStorageSession&, const URL& firstParty, const SameSiteInfo&, const URL&, Optional<uint64_t> frameID, Optional<uint64_t> pageID, IncludeSecureCookies) const;
    void setCookiesFromDOM(const NetworkStorageSession&, const URL& firstParty, const SameSiteInfo&, const URL&, Optional<uint64_t> frameID, Optional<uint64_t> pageID, const String&) const;
    void setCookiesFromHTTPResponse(const NetworkStorageSession&, const URL&, const String&) const;
    bool cookiesEnabled(const NetworkStorageSession&) const;
    std::pair<String, bool> cookieRequestHeaderFieldValue(const NetworkStorageSession&, const URL& firstParty, const SameSiteInfo&, const URL&, Optional<uint64_t> frameID, Optional<uint64_t> pageID, IncludeSecureCookies) const;
    std::pair<String, bool> cookieRequestHeaderFieldValue(const NetworkStorageSession&, const CookieRequestHeaderFieldProxy&) const;
    bool getRawCookies(const NetworkStorageSession&, const URL& firstParty, const SameSiteInfo&, const URL&, Optional<uint64_t> frameID, Optional<uint64_t> pageID, Vector<Cookie>&) const;
    void deleteCookie(const NetworkStorageSession&, const URL&, const String&) const;
    void getHostnamesWithCookies(const NetworkStorageSession&, HashSet<String>& hostnames) const;
    void deleteCookiesForHostnames(const NetworkStorageSession&, const Vector<String>& cookieHostNames) const;
    void deleteAllCookies(const NetworkStorageSession&) const;
    void deleteAllCookiesModifiedSince(const NetworkStorageSession&, WallTime) const;
};

} // namespace WebCore
