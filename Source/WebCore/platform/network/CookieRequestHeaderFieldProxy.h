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
#include "FrameIdentifier.h"
#include "PageIdentifier.h"
#include "SameSiteInfo.h"
#include <wtf/URL.h>

namespace WebCore {

struct CookieRequestHeaderFieldProxy {
    URL firstParty;
    SameSiteInfo sameSiteInfo;
    URL url;
    std::optional<FrameIdentifier> frameID;
    std::optional<PageIdentifier> pageID;
    IncludeSecureCookies includeSecureCookies { IncludeSecureCookies::No };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<CookieRequestHeaderFieldProxy> decode(Decoder&);
};

template<class Encoder>
void CookieRequestHeaderFieldProxy::encode(Encoder& encoder) const
{
    encoder << firstParty;
    encoder << sameSiteInfo;
    encoder << url;
    encoder << frameID;
    encoder << pageID;
    encoder << includeSecureCookies;
}

template<class Decoder>
std::optional<CookieRequestHeaderFieldProxy> CookieRequestHeaderFieldProxy::decode(Decoder& decoder)
{
    URL firstParty;
    if (!decoder.decode(firstParty))
        return std::nullopt;

    SameSiteInfo sameSiteInfo;
    if (!decoder.decode(sameSiteInfo))
        return std::nullopt;

    URL url;
    if (!decoder.decode(url))
        return std::nullopt;

    std::optional<std::optional<FrameIdentifier>> frameID;
    decoder >> frameID;
    if (!frameID)
        return std::nullopt;

    std::optional<std::optional<PageIdentifier>> pageID;
    decoder >> pageID;
    if (!pageID)
        return std::nullopt;

    IncludeSecureCookies includeSecureCookies;
    if (!decoder.decode(includeSecureCookies))
        return std::nullopt;

    return CookieRequestHeaderFieldProxy { WTFMove(firstParty), WTFMove(sameSiteInfo), WTFMove(url), *frameID, *pageID, includeSecureCookies };
}

} // namespace WebCore
