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

#include "NetworkLoadMetrics.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include <wtf/Vector.h>

namespace WebCore {

struct NetworkTransactionInformation {
    enum class Type { Redirection, Preflight };
    Type type;
    ResourceRequest request;
    ResourceResponse response;
    NetworkLoadMetrics metrics;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<NetworkTransactionInformation> decode(Decoder&);
};

struct NetworkLoadInformation {
    ResourceRequest request;
    ResourceResponse response;
    NetworkLoadMetrics metrics;
    Vector<NetworkTransactionInformation> transactions;
};

}

namespace WTF {
template<> struct EnumTraits<WebCore::NetworkTransactionInformation::Type> {
    using values = EnumValues<
        WebCore::NetworkTransactionInformation::Type,
        WebCore::NetworkTransactionInformation::Type::Redirection,
        WebCore::NetworkTransactionInformation::Type::Preflight
    >;
};
}

namespace WebCore {

template<class Encoder> inline void NetworkTransactionInformation::encode(Encoder& encoder) const
{
    encoder << type;
    encoder << request;
    encoder << response;
    encoder << metrics;
}

template<class Decoder> inline Optional<NetworkTransactionInformation> NetworkTransactionInformation::decode(Decoder& decoder)
{
    NetworkTransactionInformation information;

    if (!decoder.decode(information.type))
        return WTF::nullopt;
    if (!decoder.decode(information.request))
        return WTF::nullopt;
    if (!decoder.decode(information.response))
        return WTF::nullopt;
    if (!decoder.decode(information.metrics))
        return WTF::nullopt;

    return information;
}

}
