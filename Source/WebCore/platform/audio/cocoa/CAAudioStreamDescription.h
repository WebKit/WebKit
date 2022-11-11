/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "AudioStreamDescription.h"
#include <CoreAudio/CoreAudioTypes.h>

namespace WebCore {

WEBCORE_EXPORT bool operator==(const AudioStreamBasicDescription&, const AudioStreamBasicDescription&);
inline bool operator!=(const AudioStreamBasicDescription& a, const AudioStreamBasicDescription& b) { return !(a == b); }

class WEBCORE_EXPORT CAAudioStreamDescription final : public AudioStreamDescription {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CAAudioStreamDescription(const AudioStreamBasicDescription&);
    CAAudioStreamDescription(double, uint32_t, PCMFormat, bool);
    ~CAAudioStreamDescription();

    const PlatformDescription& platformDescription() const final;

    PCMFormat format() const final;

    double sampleRate() const final;
    bool isPCM() const final;
    bool isInterleaved() const final;
    bool isSignedInteger() const final;
    bool isFloat() const final;
    bool isNativeEndian() const final;

    uint32_t numberOfInterleavedChannels() const final;
    uint32_t numberOfChannelStreams() const final;
    uint32_t numberOfChannels() const final;
    uint32_t sampleWordSize() const final;
    uint32_t bytesPerFrame() const;
    uint32_t bytesPerPacket() const;
    uint32_t formatFlags() const;

    bool operator==(const CAAudioStreamDescription& other) const { return operator==(static_cast<const AudioStreamDescription&>(other)); }
    bool operator==(const AudioStreamBasicDescription& other) const;
    bool operator!=(const AudioStreamBasicDescription& other) const;
    bool operator==(const AudioStreamDescription& other) const;
    bool operator!=(const AudioStreamDescription& other) const;

    const AudioStreamBasicDescription& streamDescription() const;
    AudioStreamBasicDescription& streamDescription();

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<CAAudioStreamDescription> decode(Decoder&);

private:
    AudioStreamBasicDescription m_streamDescription;
    mutable PlatformDescription m_platformDescription;
    mutable PCMFormat m_format { None };
};

template<class Encoder>
void CAAudioStreamDescription::encode(Encoder& encoder) const
{
    encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(&m_streamDescription), sizeof(m_streamDescription), 1);
}

template<class Decoder>
std::optional<CAAudioStreamDescription> CAAudioStreamDescription::decode(Decoder& decoder)
{
    AudioStreamBasicDescription asbd { };
    if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(&asbd), sizeof(asbd), 1))
        return std::nullopt;
    return CAAudioStreamDescription { asbd };
}

inline CAAudioStreamDescription toCAAudioStreamDescription(const AudioStreamDescription& description)
{
    ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);
    return CAAudioStreamDescription(*std::get<const AudioStreamBasicDescription*>(description.platformDescription().description));
}

}
