/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class NetworkStorageSession;

struct Cookie;
struct SameSiteInfo;

enum class IncludeSecureCookies : bool { No, Yes };

class CookiesStrategy {
public:
    virtual std::pair<String, bool> cookiesForDOM(const NetworkStorageSession&, const URL& firstParty, const SameSiteInfo&, const URL&, Optional<uint64_t> frameID, Optional<uint64_t> pageID, IncludeSecureCookies) = 0;
    virtual void setCookiesFromDOM(const NetworkStorageSession&, const URL& firstParty, const SameSiteInfo&, const URL&, Optional<uint64_t> frameID, Optional<uint64_t> pageID, const String& cookieString) = 0;
    virtual bool cookiesEnabled(const NetworkStorageSession&) = 0;
    virtual std::pair<String, bool> cookieRequestHeaderFieldValue(const NetworkStorageSession&, const URL& firstParty, const SameSiteInfo&, const URL&, Optional<uint64_t> frameID, Optional<uint64_t> pageID, IncludeSecureCookies) = 0;
    virtual std::pair<String, bool> cookieRequestHeaderFieldValue(PAL::SessionID, const URL& firstParty, const SameSiteInfo&, const URL&, Optional<uint64_t> frameID, Optional<uint64_t> pageID, IncludeSecureCookies) = 0;
    virtual bool getRawCookies(const NetworkStorageSession&, const URL& firstParty, const SameSiteInfo&, const URL&, Optional<uint64_t> frameID, Optional<uint64_t> pageID, Vector<Cookie>&) = 0;
    virtual void deleteCookie(const NetworkStorageSession&, const URL&, const String& cookieName) = 0;

protected:
    virtual ~CookiesStrategy() = default;
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::IncludeSecureCookies> {
    using values = EnumValues<
        WebCore::IncludeSecureCookies,
        WebCore::IncludeSecureCookies::No,
        WebCore::IncludeSecureCookies::Yes
    >;
};

} // namespace WTF

