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

#if ENABLE(SPEECH_SYNTHESIS)

namespace WebKit {
    
struct WebSpeechSynthesisVoice {
    String voiceURI;
    String name;
    String lang;
    bool localService { false };
    bool defaultLang { false };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<WebSpeechSynthesisVoice> decode(Decoder&);
};

template<class Encoder>
void WebSpeechSynthesisVoice::encode(Encoder& encoder) const
{
    encoder << voiceURI
    << name
    << lang
    << localService
    << defaultLang;
}

template<class Decoder>
Optional<WebSpeechSynthesisVoice> WebSpeechSynthesisVoice::decode(Decoder& decoder)
{
    Optional<String> voiceURI;
    decoder >> voiceURI;
    if (!voiceURI)
        return WTF::nullopt;

    Optional<String> name;
    decoder >> name;
    if (!name)
        return WTF::nullopt;

    Optional<String> lang;
    decoder >> lang;
    if (!lang)
        return WTF::nullopt;

    Optional<bool> localService;
    decoder >> localService;
    if (!localService)
        return WTF::nullopt;

    Optional<bool> defaultLang;
    decoder >> defaultLang;
    if (!defaultLang)
        return WTF::nullopt;

    return {{ WTFMove(*voiceURI), WTFMove(*name), WTFMove(*lang), WTFMove(*localService), WTFMove(*defaultLang) }};}

} // namespace WebKit

#endif // ENABLE(SPEECH_SYNTHESIS)
