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

namespace WebCore {

struct SpeechRecognitionAlternativeData {
    String transcript;
    double confidence { 0.0 };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<SpeechRecognitionAlternativeData> decode(Decoder&);
};

template<class Encoder>
void SpeechRecognitionAlternativeData::encode(Encoder& encoder) const
{
    encoder << transcript << confidence;
}

template<class Decoder>
std::optional<SpeechRecognitionAlternativeData> SpeechRecognitionAlternativeData::decode(Decoder& decoder)
{
    SpeechRecognitionAlternativeData result;
    if (!decoder.decode(result.transcript))
        return std::nullopt;

    if (!decoder.decode(result.confidence))
        return std::nullopt;

    return result;
}

struct SpeechRecognitionResultData {
    Vector<SpeechRecognitionAlternativeData> alternatives;
    bool isFinal { false };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<SpeechRecognitionResultData> decode(Decoder&);
};

template<class Encoder>
void SpeechRecognitionResultData::encode(Encoder& encoder) const
{
    encoder << alternatives << isFinal;
}

template<class Decoder>
std::optional<SpeechRecognitionResultData> SpeechRecognitionResultData::decode(Decoder& decoder)
{
    SpeechRecognitionResultData result;
    if (!decoder.decode(result.alternatives))
        return std::nullopt;

    if (!decoder.decode(result.isFinal))
        return std::nullopt;

    return result;
}

} // namespace WebCore
