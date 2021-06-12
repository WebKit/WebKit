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

#include <wtf/EnumTraits.h>

namespace WebCore {

enum class SpeechRecognitionErrorType : uint8_t {
    NoSpeech,
    Aborted,
    AudioCapture,
    Network,
    NotAllowed,
    ServiceNotAllowed,
    BadGrammar,
    LanguageNotSupported
};

struct SpeechRecognitionError {
    SpeechRecognitionErrorType type;
    String message;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<SpeechRecognitionError> decode(Decoder&);
};

template<class Encoder>
void SpeechRecognitionError::encode(Encoder& encoder) const
{
    encoder << type << message;
}

template<class Decoder>
std::optional<SpeechRecognitionError> SpeechRecognitionError::decode(Decoder& decoder)
{
    std::optional<SpeechRecognitionErrorType> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    std::optional<String> message;
    decoder >> message;
    if (!message)
        return std::nullopt;
    
    return SpeechRecognitionError { WTFMove(*type), WTFMove(*message) };
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::SpeechRecognitionErrorType> {
    using values = EnumValues<
        WebCore::SpeechRecognitionErrorType,
        WebCore::SpeechRecognitionErrorType::NoSpeech,
        WebCore::SpeechRecognitionErrorType::Aborted,
        WebCore::SpeechRecognitionErrorType::AudioCapture,
        WebCore::SpeechRecognitionErrorType::Network,
        WebCore::SpeechRecognitionErrorType::NotAllowed,
        WebCore::SpeechRecognitionErrorType::ServiceNotAllowed,
        WebCore::SpeechRecognitionErrorType::BadGrammar,
        WebCore::SpeechRecognitionErrorType::LanguageNotSupported
    >;
};

} // namespace WTF
