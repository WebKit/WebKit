/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "CookiesStrategy.h"
#include "URL.h"
#include <pal/SessionID.h>

namespace WebCore {

struct CookieRequestHeaderFieldProxy {
    PAL::SessionID m_sessionID;
    URL m_firstParty;
    URL m_url;
    std::optional<uint64_t> m_frameID;
    std::optional<uint64_t> m_pageID;
    IncludeSecureCookies m_includeSecureCookies { IncludeSecureCookies::No };

    CookieRequestHeaderFieldProxy() = default;
    CookieRequestHeaderFieldProxy(PAL::SessionID&& sessionID, URL&& firstParty, URL&& url, std::optional<uint64_t>&& frameID, std::optional<uint64_t>&& pageID, IncludeSecureCookies includeSecureCookies)
        : m_sessionID(WTFMove(sessionID))
        , m_firstParty(WTFMove(firstParty))
        , m_url(WTFMove(url))
        , m_frameID(WTFMove(frameID))
        , m_pageID(WTFMove(pageID))
        , m_includeSecureCookies(includeSecureCookies)
    {
    }

    CookieRequestHeaderFieldProxy(PAL::SessionID sessionID, const URL& firstParty, const URL& url, const std::optional<uint64_t>& frameID, const std::optional<uint64_t>& pageID, IncludeSecureCookies includeSecureCookies)
        : m_sessionID(sessionID)
        , m_firstParty(firstParty)
        , m_url(url)
        , m_frameID(frameID)
        , m_pageID(pageID)
        , m_includeSecureCookies(includeSecureCookies)
    {
    }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<CookieRequestHeaderFieldProxy> decode(Decoder&);
};

template<class Encoder>
void CookieRequestHeaderFieldProxy::encode(Encoder& encoder) const
{
    encoder << m_sessionID;
    encoder << m_firstParty;
    encoder << m_url;
    encoder << m_frameID;
    encoder << m_pageID;
    encoder << m_includeSecureCookies;
}

template<class Decoder>
std::optional<CookieRequestHeaderFieldProxy> CookieRequestHeaderFieldProxy::decode(Decoder& decoder)
{
    PAL::SessionID sessionID;
    if (!decoder.decode(sessionID))
        return std::nullopt;

    URL firstParty;
    if (!decoder.decode(firstParty))
        return std::nullopt;

    URL url;
    if (!decoder.decode(url))
        return std::nullopt;

    std::optional<uint64_t> frameID;
    if (!decoder.decode(frameID))
        return std::nullopt;

    std::optional<uint64_t> pageID;
    if (!decoder.decode(pageID))
        return std::nullopt;

    IncludeSecureCookies includeSecureCookies;
    if (!decoder.decode(includeSecureCookies))
        return std::nullopt;

    return {{ WTFMove(sessionID), WTFMove(firstParty), WTFMove(url), WTFMove(*frameID), WTFMove(*pageID), includeSecureCookies }};
}

}
