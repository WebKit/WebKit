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

#include "config.h"
#include "CAAudioStreamDescription.h"

namespace WebCore {

CAAudioStreamDescription::CAAudioStreamDescription()
    : m_streamDescription({ })
{
}
    
CAAudioStreamDescription::~CAAudioStreamDescription() = default;

CAAudioStreamDescription::CAAudioStreamDescription(const AudioStreamBasicDescription &desc)
    : m_streamDescription(desc)
{
}

CAAudioStreamDescription::CAAudioStreamDescription(double sampleRate, uint32_t numChannels, PCMFormat format, bool isInterleaved)
{
    m_streamDescription.mFormatID = kAudioFormatLinearPCM;
    m_streamDescription.mSampleRate = sampleRate;
    m_streamDescription.mFramesPerPacket = 1;
    m_streamDescription.mChannelsPerFrame = numChannels;
    m_streamDescription.mBytesPerFrame = 0;
    m_streamDescription.mBytesPerPacket = 0;
    m_streamDescription.mFormatFlags = kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
    m_streamDescription.mReserved = 0;

    int wordsize;
    switch (format) {
    case Int16:
        wordsize = 2;
        m_streamDescription.mFormatFlags |= kAudioFormatFlagIsSignedInteger;
        break;
    case Int32:
        wordsize = 4;
        m_streamDescription.mFormatFlags |= kAudioFormatFlagIsSignedInteger;
        break;
    case Float32:
        wordsize = 4;
        m_streamDescription.mFormatFlags |= kAudioFormatFlagIsFloat;
        break;
    case Float64:
        wordsize = 8;
        m_streamDescription.mFormatFlags |= kAudioFormatFlagIsFloat;
        break;
    case None:
        ASSERT_NOT_REACHED();
        wordsize = 0;
        break;
    }

    m_streamDescription.mBitsPerChannel = wordsize * 8;
    if (isInterleaved)
        m_streamDescription.mBytesPerFrame = m_streamDescription.mBytesPerPacket = wordsize * numChannels;
    else {
        m_streamDescription.mFormatFlags |= kAudioFormatFlagIsNonInterleaved;
        m_streamDescription.mBytesPerFrame = m_streamDescription.mBytesPerPacket = wordsize;
    }
}

const PlatformDescription& CAAudioStreamDescription::platformDescription() const
{
    m_platformDescription = { PlatformDescription::CAAudioStreamBasicType, &m_streamDescription };
    return m_platformDescription;
}

AudioStreamDescription::PCMFormat CAAudioStreamDescription::format() const
{
    if (m_format != None)
        return m_format;

    if (m_streamDescription.mFormatID != kAudioFormatLinearPCM)
        return None;
    if (m_streamDescription.mFramesPerPacket != 1)
        return None;
    if (m_streamDescription.mBytesPerFrame != m_streamDescription.mBytesPerPacket)
        return None;
    if (m_streamDescription.mBitsPerChannel / sizeof(double) > m_streamDescription.mBytesPerFrame)
        return None;
    if (!m_streamDescription.mChannelsPerFrame)
        return None;

    unsigned wordsize = m_streamDescription.mBytesPerFrame;
    if (isInterleaved()) {
        if (wordsize % m_streamDescription.mChannelsPerFrame)
            return None;

        wordsize /= m_streamDescription.mChannelsPerFrame;
    }

    if (isNativeEndian() && wordsize * sizeof(double) == m_streamDescription.mBitsPerChannel) {
        // Packed and native endian, good
        if (m_streamDescription.mFormatFlags & kLinearPCMFormatFlagIsFloat) {
            if (m_streamDescription.mFormatFlags & (kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagsSampleFractionMask))
                return None;

            if (wordsize == sizeof(float))
                return m_format = Float32;
            if (wordsize == sizeof(double))
                return m_format = Float64;

            return None;
        }

        if (m_streamDescription.mFormatFlags & kLinearPCMFormatFlagIsSignedInteger) {
            unsigned fractionBits = (m_streamDescription.mFormatFlags & kLinearPCMFormatFlagsSampleFractionMask) >> kLinearPCMFormatFlagsSampleFractionShift;
            if (!fractionBits) {
                if (wordsize == sizeof(int16_t))
                    return m_format = Int16;
                if (wordsize == sizeof(int32_t))
                    return m_format = Int32;
            }
        }
    }

    return None;
}

bool operator==(const AudioStreamBasicDescription& a, const AudioStreamBasicDescription& b)
{
    return a.mSampleRate == b.mSampleRate
        && a.mFormatID == b.mFormatID
        && a.mFormatFlags == b.mFormatFlags
        && a.mBytesPerPacket == b.mBytesPerPacket
        && a.mFramesPerPacket == b.mFramesPerPacket
        && a.mBytesPerFrame == b.mBytesPerFrame
        && a.mChannelsPerFrame == b.mChannelsPerFrame
        && a.mBitsPerChannel == b.mBitsPerChannel;
}

}
