
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

#include "ResourceRequest.h"

namespace WebCore {

struct RetrieveRecordsOptions {
    RetrieveRecordsOptions isolatedCopy() const { return { request.isolatedCopy(), ignoreSearch, ignoreMethod, ignoreVary, shouldProvideResponse }; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<RetrieveRecordsOptions> decode(Decoder&);

    ResourceRequest request;
    bool ignoreSearch { false };
    bool ignoreMethod { false };
    bool ignoreVary { false };
    bool shouldProvideResponse { true };
};

template<class Encoder> inline void RetrieveRecordsOptions::encode(Encoder& encoder) const
{
    encoder << request << ignoreSearch << ignoreMethod << ignoreVary << shouldProvideResponse;
}

template<class Decoder> inline Optional<RetrieveRecordsOptions> RetrieveRecordsOptions::decode(Decoder& decoder)
{
    Optional<ResourceRequest> request;
    decoder >> request;
    if (!request)
        return WTF::nullopt;

    Optional<bool> ignoreSearch;
    decoder >> ignoreSearch;
    if (!ignoreSearch)
        return WTF::nullopt;

    Optional<bool> ignoreMethod;
    decoder >> ignoreMethod;
    if (!ignoreMethod)
        return WTF::nullopt;

    Optional<bool> ignoreVary;
    decoder >> ignoreVary;
    if (!ignoreVary)
        return WTF::nullopt;

    Optional<bool> shouldProvideResponse;
    decoder >> shouldProvideResponse;
    if (!shouldProvideResponse)
        return WTF::nullopt;

    return { { WTFMove(*request), WTFMove(*ignoreSearch), WTFMove(*ignoreMethod), WTFMove(*ignoreVary), WTFMove(*shouldProvideResponse) } };
}

} // namespace WebCore
