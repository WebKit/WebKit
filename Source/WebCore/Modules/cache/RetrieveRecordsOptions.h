
/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "CrossOriginEmbedderPolicy.h"
#include "ResourceRequest.h"
#include "SecurityOrigin.h"

namespace WebCore {

struct RetrieveRecordsOptions {
    RetrieveRecordsOptions isolatedCopy() const { return { request.isolatedCopy(), crossOriginEmbedderPolicy.isolatedCopy(), sourceOrigin->isolatedCopy(), ignoreSearch, ignoreMethod, ignoreVary, shouldProvideResponse }; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<RetrieveRecordsOptions> decode(Decoder&);

    ResourceRequest request;
    CrossOriginEmbedderPolicy crossOriginEmbedderPolicy;
    Ref<SecurityOrigin> sourceOrigin;
    bool ignoreSearch { false };
    bool ignoreMethod { false };
    bool ignoreVary { false };
    bool shouldProvideResponse { true };
};

template<class Encoder> inline void RetrieveRecordsOptions::encode(Encoder& encoder) const
{
    encoder << request << crossOriginEmbedderPolicy << sourceOrigin.get() << ignoreSearch << ignoreMethod << ignoreVary << shouldProvideResponse;
}

template<class Decoder> inline std::optional<RetrieveRecordsOptions> RetrieveRecordsOptions::decode(Decoder& decoder)
{
    std::optional<ResourceRequest> request;
    decoder >> request;
    if (!request)
        return std::nullopt;

    std::optional<CrossOriginEmbedderPolicy> crossOriginEmbedderPolicy;
    decoder >> crossOriginEmbedderPolicy;
    if (!crossOriginEmbedderPolicy)
        return std::nullopt;

    auto sourceOrigin = SecurityOrigin::decode(decoder);
    if (!sourceOrigin)
        return std::nullopt;

    std::optional<bool> ignoreSearch;
    decoder >> ignoreSearch;
    if (!ignoreSearch)
        return std::nullopt;

    std::optional<bool> ignoreMethod;
    decoder >> ignoreMethod;
    if (!ignoreMethod)
        return std::nullopt;

    std::optional<bool> ignoreVary;
    decoder >> ignoreVary;
    if (!ignoreVary)
        return std::nullopt;

    std::optional<bool> shouldProvideResponse;
    decoder >> shouldProvideResponse;
    if (!shouldProvideResponse)
        return std::nullopt;

    return { { WTFMove(*request), WTFMove(*crossOriginEmbedderPolicy), sourceOrigin.releaseNonNull(), WTFMove(*ignoreSearch), WTFMove(*ignoreMethod), WTFMove(*ignoreVary), WTFMove(*shouldProvideResponse) } };
}

} // namespace WebCore
