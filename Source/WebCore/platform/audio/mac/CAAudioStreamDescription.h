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

class CAAudioStreamDescription final : public AudioStreamDescription {
    WTF_MAKE_FAST_ALLOCATED;

public:

    WEBCORE_EXPORT CAAudioStreamDescription();
    WEBCORE_EXPORT CAAudioStreamDescription(const AudioStreamBasicDescription&);
    WEBCORE_EXPORT CAAudioStreamDescription(double, uint32_t, PCMFormat, bool);
    WEBCORE_EXPORT ~CAAudioStreamDescription();

    const PlatformDescription& platformDescription() const final;

    WEBCORE_EXPORT PCMFormat format() const final;

    double sampleRate() const final { return m_streamDescription.mSampleRate; }
    bool isPCM() const final { return m_streamDescription.mFormatID == kAudioFormatLinearPCM; }
    bool isInterleaved() const final { return !(m_streamDescription.mFormatFlags & kAudioFormatFlagIsNonInterleaved); }
    bool isSignedInteger() const final { return isPCM() && (m_streamDescription.mFormatFlags & kAudioFormatFlagIsSignedInteger); }
    bool isFloat() const final { return isPCM() && (m_streamDescription.mFormatFlags & kAudioFormatFlagIsFloat); }
    bool isNativeEndian() const final { return isPCM() && (m_streamDescription.mFormatFlags & kAudioFormatFlagIsBigEndian) == kAudioFormatFlagsNativeEndian; }

    uint32_t numberOfInterleavedChannels() const final { return isInterleaved() ? m_streamDescription.mChannelsPerFrame : 1; }
    uint32_t numberOfChannelStreams() const final { return isInterleaved() ? 1 : m_streamDescription.mChannelsPerFrame; }
    uint32_t numberOfChannels() const final { return m_streamDescription.mChannelsPerFrame; }
    uint32_t sampleWordSize() const final {
        return (m_streamDescription.mBytesPerFrame > 0 && numberOfInterleavedChannels()) ? m_streamDescription.mBytesPerFrame / numberOfInterleavedChannels() :  0;
    }
    uint32_t bytesPerFrame() const { return m_streamDescription.mBytesPerFrame; }
    uint32_t bytesPerPacket() const { return m_streamDescription.mBytesPerPacket; }
    uint32_t formatFlags() const { return m_streamDescription.mFormatFlags; }

    bool operator==(const AudioStreamBasicDescription& other) { return m_streamDescription == other; }
    bool operator!=(const AudioStreamBasicDescription& other) { return !operator == (other); }
    bool operator==(const AudioStreamDescription& other)
    {
        if (other.platformDescription().type != PlatformDescription::CAAudioStreamBasicType)
            return false;

        return operator==(*WTF::get<const AudioStreamBasicDescription*>(other.platformDescription().description));
    }
    bool operator!=(const AudioStreamDescription& other) { return !operator == (other); }

    const AudioStreamBasicDescription& streamDescription() const { return m_streamDescription; }
    AudioStreamBasicDescription& streamDescription() { return m_streamDescription; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, CAAudioStreamDescription&);

private:
    void calculateFormat();

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
bool CAAudioStreamDescription::decode(Decoder& decoder, CAAudioStreamDescription& description)
{
    return decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(&description.m_streamDescription), sizeof(description.m_streamDescription), 1);
}

inline CAAudioStreamDescription toCAAudioStreamDescription(const AudioStreamDescription& description)
{
    ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);
    return CAAudioStreamDescription(*WTF::get<const AudioStreamBasicDescription*>(description.platformDescription().description));
}

}
