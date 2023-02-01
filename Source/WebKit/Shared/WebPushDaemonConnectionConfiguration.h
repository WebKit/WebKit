/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include <optional>
#include <wtf/UUID.h>
#include <wtf/Vector.h>

namespace WebKit::WebPushD {

struct WebPushDaemonConnectionConfiguration {
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<WebPushDaemonConnectionConfiguration> decode(Decoder&);

    bool useMockBundlesForTesting { false };
    std::optional<Vector<uint8_t>> hostAppAuditTokenData;
    String pushPartitionString;
    Markable<UUID> dataStoreIdentifier;
};

template<class Encoder>
void WebPushDaemonConnectionConfiguration::encode(Encoder& encoder) const
{
    encoder << useMockBundlesForTesting << hostAppAuditTokenData << pushPartitionString << dataStoreIdentifier;
}

template<class Decoder>
std::optional<WebPushDaemonConnectionConfiguration> WebPushDaemonConnectionConfiguration::decode(Decoder& decoder)
{
    std::optional<bool> useMockBundlesForTesting;
    decoder >> useMockBundlesForTesting;
    if (!useMockBundlesForTesting)
        return std::nullopt;

    std::optional<std::optional<Vector<uint8_t>>> hostAppAuditTokenData;
    decoder >> hostAppAuditTokenData;
    if (!hostAppAuditTokenData)
        return std::nullopt;

    std::optional<String> pushPartitionString;
    decoder >> pushPartitionString;
    if (!pushPartitionString)
        return std::nullopt;

    std::optional<std::optional<UUID>> dataStoreIdentifier;
    decoder >> dataStoreIdentifier;
    if (!dataStoreIdentifier)
        return std::nullopt;

    return { {
        WTFMove(*useMockBundlesForTesting),
        WTFMove(*hostAppAuditTokenData),
        WTFMove(*pushPartitionString),
        WTFMove(*dataStoreIdentifier)
    } };
}

} // namespace WebKit::WebPushD
