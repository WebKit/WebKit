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

#include "CookieJar.h"
#include "SameSiteInfo.h"
#include <pal/SessionID.h>
#include <wtf/URL.h>

namespace WebCore {

struct CookieRequestHeaderFieldProxy {
    PAL::SessionID sessionID;
    URL firstParty;
    SameSiteInfo sameSiteInfo;
    URL url;
    Optional<uint64_t> frameID;
    Optional<uint64_t> pageID;
    IncludeSecureCookies includeSecureCookies { IncludeSecureCookies::No };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<CookieRequestHeaderFieldProxy> decode(Decoder&);
};

template<class Encoder>
void CookieRequestHeaderFieldProxy::encode(Encoder& encoder) const
{
    encoder << sessionID;
    encoder << firstParty;
    encoder << sameSiteInfo;
    encoder << url;
    encoder << frameID;
    encoder << pageID;
    encoder << includeSecureCookies;
}

template<class Decoder>
Optional<CookieRequestHeaderFieldProxy> CookieRequestHeaderFieldProxy::decode(Decoder& decoder)
{
    CookieRequestHeaderFieldProxy result;
    if (!decoder.decode(result.sessionID))
        return WTF::nullopt;
    if (!decoder.decode(result.firstParty))
        return WTF::nullopt;
    if (!decoder.decode(result.sameSiteInfo))
        return WTF::nullopt;
    if (!decoder.decode(result.url))
        return WTF::nullopt;
    if (!decoder.decode(result.frameID))
        return WTF::nullopt;
    if (!decoder.decode(result.pageID))
        return WTF::nullopt;
    if (!decoder.decode(result.includeSecureCookies))
        return WTF::nullopt;
    return WTFMove(result);
}

} // namespace WebCore
