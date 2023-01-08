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

#include "config.h"
#include "SpeechRecognitionUpdate.h"

namespace WebCore {

String convertEnumerationToString(SpeechRecognitionUpdateType enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("UpdateTypeStart"),
        MAKE_STATIC_STRING_IMPL("UpdateTypeAudioStart"),
        MAKE_STATIC_STRING_IMPL("UpdateTypeSoundStart"),
        MAKE_STATIC_STRING_IMPL("UpdateTypeSpeechStart"),
        MAKE_STATIC_STRING_IMPL("UpdateTypeSpeechEnd"),
        MAKE_STATIC_STRING_IMPL("UpdateTypeSoundEnd"),
        MAKE_STATIC_STRING_IMPL("UpdateTypeAudioEnd"),
        MAKE_STATIC_STRING_IMPL("UpdateTypeResult"),
        MAKE_STATIC_STRING_IMPL("UpdateTypeNoMatch"),
        MAKE_STATIC_STRING_IMPL("UpdateTypeError"),
        MAKE_STATIC_STRING_IMPL("UpdateTypeEnd"),
    };
    static_assert(static_cast<size_t>(SpeechRecognitionUpdateType::Start) == 0, "SpeechRecognitionUpdateType::Start is not 1 as expected");
    static_assert(static_cast<size_t>(SpeechRecognitionUpdateType::AudioStart) == 1, "SpeechRecognitionUpdateType::AudioStart is not 2 as expected");
    static_assert(static_cast<size_t>(SpeechRecognitionUpdateType::SoundStart) == 2, "SpeechRecognitionUpdateType::SoundStart is not 3 as expected");
    static_assert(static_cast<size_t>(SpeechRecognitionUpdateType::SpeechStart) == 3, "SpeechRecognitionUpdateType::SpeechStart is not 4 as expected");
    static_assert(static_cast<size_t>(SpeechRecognitionUpdateType::SpeechEnd) == 4, "SpeechRecognitionUpdateType::SpeechEnd is not 5 as expected");
    static_assert(static_cast<size_t>(SpeechRecognitionUpdateType::SoundEnd) == 5, "SpeechRecognitionUpdateType::SoundEnd is not 6 as expected");
    static_assert(static_cast<size_t>(SpeechRecognitionUpdateType::AudioEnd) == 6, "SpeechRecognitionUpdateType::AudioEnd is not 7 as expected");
    static_assert(static_cast<size_t>(SpeechRecognitionUpdateType::Result) == 7, "SpeechRecognitionUpdateType::Result is not 8 as expected");
    static_assert(static_cast<size_t>(SpeechRecognitionUpdateType::NoMatch) == 8, "SpeechRecognitionUpdateType::NoMatch is not 9 as expected");
    static_assert(static_cast<size_t>(SpeechRecognitionUpdateType::Error) == 9, "SpeechRecognitionUpdateType::Error is not 10 as expected");
    static_assert(static_cast<size_t>(SpeechRecognitionUpdateType::End) == 10, "SpeechRecognitionUpdateType::End is not 11 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

SpeechRecognitionUpdate SpeechRecognitionUpdate::create(SpeechRecognitionConnectionClientIdentifier clientIdentifier, SpeechRecognitionUpdateType type)
{
    return SpeechRecognitionUpdate { clientIdentifier, type, std::monostate() };
}

SpeechRecognitionUpdate SpeechRecognitionUpdate::createError(SpeechRecognitionConnectionClientIdentifier clientIdentifier, const SpeechRecognitionError& error)
{
    return SpeechRecognitionUpdate { clientIdentifier, SpeechRecognitionUpdateType::Error, error };
}

SpeechRecognitionUpdate SpeechRecognitionUpdate::createResult(SpeechRecognitionConnectionClientIdentifier clientIdentifier, const Vector<SpeechRecognitionResultData>& results)
{
    return SpeechRecognitionUpdate { clientIdentifier, SpeechRecognitionUpdateType::Result, results };
}

SpeechRecognitionUpdate::SpeechRecognitionUpdate(SpeechRecognitionConnectionClientIdentifier clientIdentifier, SpeechRecognitionUpdateType type, Content content)
    : m_clientIdentifier(clientIdentifier)
    , m_type(type)
    , m_content(content)
{
}

SpeechRecognitionError SpeechRecognitionUpdate::error() const
{
    return WTF::switchOn(m_content,
        [] (const SpeechRecognitionError& error) { return error; },
        [] (const auto&) { return SpeechRecognitionError(); }
    );
}

Vector<SpeechRecognitionResultData> SpeechRecognitionUpdate::result() const
{
    return WTF::switchOn(m_content,
        [] (const Vector<SpeechRecognitionResultData>& data) { return data; },
        [] (const auto&) { return Vector<SpeechRecognitionResultData> { }; }
    );
}

} // namespace WebCore
