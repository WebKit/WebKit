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

#include "SpeechRecognitionConnectionClientIdentifier.h"
#include "SpeechRecognitionError.h"
#include "SpeechRecognitionResultData.h"
#include <variant>
#include <wtf/EnumTraits.h>

namespace WebCore {

enum class SpeechRecognitionUpdateType {
    Start,
    AudioStart,
    SoundStart,
    SpeechStart,
    SpeechEnd,
    SoundEnd,
    AudioEnd,
    Result,
    NoMatch,
    Error,
    End
};

String convertEnumerationToString(SpeechRecognitionUpdateType);

class SpeechRecognitionUpdate {
public:
    WEBCORE_EXPORT static SpeechRecognitionUpdate create(SpeechRecognitionConnectionClientIdentifier, SpeechRecognitionUpdateType);
    WEBCORE_EXPORT static SpeechRecognitionUpdate createError(SpeechRecognitionConnectionClientIdentifier, const SpeechRecognitionError&);
    WEBCORE_EXPORT static SpeechRecognitionUpdate createResult(SpeechRecognitionConnectionClientIdentifier, const Vector<SpeechRecognitionResultData>&);

    SpeechRecognitionConnectionClientIdentifier clientIdentifier() const { return m_clientIdentifier; }
    SpeechRecognitionUpdateType type() const { return m_type; }
    WEBCORE_EXPORT SpeechRecognitionError error() const;
    WEBCORE_EXPORT Vector<SpeechRecognitionResultData> result() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<SpeechRecognitionUpdate> decode(Decoder&);

private:
    using Content = std::variant<std::monostate, SpeechRecognitionError, Vector<SpeechRecognitionResultData>>;
    WEBCORE_EXPORT SpeechRecognitionUpdate(SpeechRecognitionConnectionClientIdentifier, SpeechRecognitionUpdateType, Content);

    SpeechRecognitionConnectionClientIdentifier m_clientIdentifier;
    SpeechRecognitionUpdateType m_type;
    Content m_content;
};

template<class Encoder>
void SpeechRecognitionUpdate::encode(Encoder& encoder) const
{
    encoder << m_clientIdentifier << m_type << m_content;
}

template<class Decoder>
std::optional<SpeechRecognitionUpdate> SpeechRecognitionUpdate::decode(Decoder& decoder)
{
    std::optional<SpeechRecognitionConnectionClientIdentifier> clientIdentifier;
    decoder >> clientIdentifier;
    if (!clientIdentifier)
        return std::nullopt;

    std::optional<SpeechRecognitionUpdateType> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    std::optional<Content> content;
    decoder >> content;
    if (!content)
        return std::nullopt;

    return SpeechRecognitionUpdate { WTFMove(*clientIdentifier), WTFMove(*type), WTFMove(*content) };
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::SpeechRecognitionUpdateType> {
    using values = EnumValues<
        WebCore::SpeechRecognitionUpdateType,
        WebCore::SpeechRecognitionUpdateType::Start,
        WebCore::SpeechRecognitionUpdateType::AudioStart,
        WebCore::SpeechRecognitionUpdateType::SoundStart,
        WebCore::SpeechRecognitionUpdateType::SpeechStart,
        WebCore::SpeechRecognitionUpdateType::SpeechEnd,
        WebCore::SpeechRecognitionUpdateType::SoundEnd,
        WebCore::SpeechRecognitionUpdateType::AudioEnd,
        WebCore::SpeechRecognitionUpdateType::Result,
        WebCore::SpeechRecognitionUpdateType::NoMatch,
        WebCore::SpeechRecognitionUpdateType::Error,
        WebCore::SpeechRecognitionUpdateType::End
    >;
};

template<typename> struct LogArgument;

template<> struct LogArgument<WebCore::SpeechRecognitionUpdateType> {
    static String toString(const WebCore::SpeechRecognitionUpdateType type)
    {
        return convertEnumerationToString(type);
    }
};

} // namespace WTF
