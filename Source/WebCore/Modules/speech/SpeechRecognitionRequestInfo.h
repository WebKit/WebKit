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

#include "ClientOrigin.h"
#include "FrameIdentifier.h"
#include "SpeechRecognitionConnectionClientIdentifier.h"

namespace WebCore {

struct SpeechRecognitionRequestInfo {
    SpeechRecognitionConnectionClientIdentifier clientIdentifier;
    String lang;
    bool continuous { false };
    bool interimResults { false };
    uint64_t maxAlternatives { 1 };
    ClientOrigin clientOrigin;
    FrameIdentifier frameIdentifier;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<SpeechRecognitionRequestInfo> decode(Decoder&);
};

template<class Encoder>
void SpeechRecognitionRequestInfo::encode(Encoder& encoder) const
{
    encoder << clientIdentifier << lang << continuous << interimResults << maxAlternatives << clientOrigin << frameIdentifier;
}

template<class Decoder>
std::optional<SpeechRecognitionRequestInfo> SpeechRecognitionRequestInfo::decode(Decoder& decoder)
{
    std::optional<SpeechRecognitionConnectionClientIdentifier> clientIdentifier;
    decoder >> clientIdentifier;
    if (!clientIdentifier)
        return std::nullopt;

    std::optional<String> lang;
    decoder >> lang;
    if (!lang)
        return std::nullopt;

    std::optional<bool> continuous;
    decoder >> continuous;
    if (!continuous)
        return std::nullopt;

    std::optional<bool> interimResults;
    decoder >> interimResults;
    if (!interimResults)
        return std::nullopt;

    std::optional<uint64_t> maxAlternatives;
    decoder >> maxAlternatives;
    if (!maxAlternatives)
        return std::nullopt;

    std::optional<ClientOrigin> clientOrigin;
    decoder >> clientOrigin;
    if (!clientOrigin)
        return std::nullopt;

    std::optional<FrameIdentifier> frameIdentifier;
    decoder >> frameIdentifier;
    if (!frameIdentifier)
        return std::nullopt;

    return {{
        WTFMove(*clientIdentifier),
        WTFMove(*lang),
        WTFMove(*continuous),
        WTFMove(*interimResults),
        WTFMove(*maxAlternatives),
        WTFMove(*clientOrigin),
        WTFMove(*frameIdentifier)
    }};
}

} // namespace WebCore
