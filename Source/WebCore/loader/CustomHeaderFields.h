/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "HTTPHeaderField.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct WEBCORE_EXPORT CustomHeaderFields {
    Vector<HTTPHeaderField> fields;
    Vector<String> thirdPartyDomains;

    bool thirdPartyDomainsMatch(const URL&) const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<CustomHeaderFields> decode(Decoder&);
};

template<class Encoder>
void CustomHeaderFields::encode(Encoder& encoder) const
{
    encoder << fields;
    encoder << thirdPartyDomains;
}

template<class Decoder>
std::optional<CustomHeaderFields> CustomHeaderFields::decode(Decoder& decoder)
{
    std::optional<Vector<HTTPHeaderField>> fields;
    decoder >> fields;
    if (!fields)
        return std::nullopt;
    
    std::optional<Vector<String>> thirdPartyDomains;
    decoder >> thirdPartyDomains;
    if (!thirdPartyDomains)
        return std::nullopt;
    
    return {{ WTFMove(*fields), WTFMove(*thirdPartyDomains) }};
}

} // namespace WebCore
