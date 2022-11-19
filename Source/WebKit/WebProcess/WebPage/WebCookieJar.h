/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

#include "WebCookieCache.h"
#include <WebCore/CookieJar.h>

namespace WebCore {
struct Cookie;
}

namespace WebKit {

class WebFrame;

class WebCookieJar final : public WebCore::CookieJar {
public:
    static Ref<WebCookieJar> create() { return adoptRef(*new WebCookieJar); }
    
    String cookies(WebCore::Document&, const URL&) const final;
    void setCookies(WebCore::Document&, const URL&, const String& cookieString) final;
    bool cookiesEnabled(const WebCore::Document&) const final;
    std::pair<String, WebCore::SecureCookiesAccessed> cookieRequestHeaderFieldValue(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::IncludeSecureCookies) const final;
    bool getRawCookies(const WebCore::Document&, const URL&, Vector<WebCore::Cookie>&) const final;
    void setRawCookie(const WebCore::Document&, const WebCore::Cookie&) final;
    void deleteCookie(const WebCore::Document&, const URL&, const String& cookieName, CompletionHandler<void()>&&) final;

    void cookiesAdded(const String& host, const Vector<WebCore::Cookie>&);
    void cookiesDeleted(const String& host, const Vector<WebCore::Cookie>&);
    void allCookiesDeleted();

    void clearCache() final;

private:
    WebCookieJar();

    void clearCacheForHost(const String&) final;
    bool isEligibleForCache(WebFrame&, const URL& firstPartyForCookies, const URL& resourceURL) const;

    mutable WebCookieCache m_cache;
};

} // namespace WebKit
