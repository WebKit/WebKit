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
    m_streamDescription.mFormatFlags = static_cast<AudioFormatFlags>(kAudioFormatFlagsNativeEndian) | static_cast<AudioFormatFlags>(kAudioFormatFlagIsPacked);
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
    if (!m_streamDescription.mChannelsPerFrame)
        return None;
    if (!isNativeEndian())
        return None;
    if (m_streamDescription.mBitsPerChannel % 8)
        return None;
    uint32_t bytesPerSample = m_streamDescription.mBitsPerChannel / 8;
    uint32_t numChannelsPerFrame = numberOfInterleavedChannels();
    if (m_streamDescription.mBytesPerFrame % numChannelsPerFrame)
        return None;
    if (m_streamDescription.mBytesPerFrame / numChannelsPerFrame != bytesPerSample)
        return None;
    std::optional<PCMFormat> format;
    auto asbdFormat = m_streamDescription.mFormatFlags & (kLinearPCMFormatFlagIsFloat | kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagsSampleFractionMask);
    if (asbdFormat == kLinearPCMFormatFlagIsFloat) {
        if (bytesPerSample == sizeof(float))
            format = Float32;
        else if (bytesPerSample == sizeof(double))
            format = Float64;
    } else if (asbdFormat == kLinearPCMFormatFlagIsSignedInteger) {
        if (bytesPerSample == sizeof(int16_t))
            format = Int16;
        else if (bytesPerSample == sizeof(int32_t))
            format = Int32;
    }
    if (!format)
        return None;
    m_format = *format;
    return m_format;
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

double CAAudioStreamDescription::sampleRate() const
{
    return m_streamDescription.mSampleRate;
}

bool CAAudioStreamDescription::isPCM() const
{
    return m_streamDescription.mFormatID == kAudioFormatLinearPCM;
}

bool CAAudioStreamDescription::isInterleaved() const
{
    return !(m_streamDescription.mFormatFlags & kAudioFormatFlagIsNonInterleaved);
}

bool CAAudioStreamDescription::isSignedInteger() const
{
    return isPCM() && (m_streamDescription.mFormatFlags & kAudioFormatFlagIsSignedInteger);
}

bool CAAudioStreamDescription::isFloat() const
{
    return isPCM() && (m_streamDescription.mFormatFlags & kAudioFormatFlagIsFloat);
}

bool CAAudioStreamDescription::isNativeEndian() const
{
    return isPCM() && (m_streamDescription.mFormatFlags & kAudioFormatFlagIsBigEndian) == kAudioFormatFlagsNativeEndian;
}

uint32_t CAAudioStreamDescription::numberOfInterleavedChannels() const
{
    return isInterleaved() ? m_streamDescription.mChannelsPerFrame : 1;
}

uint32_t CAAudioStreamDescription::numberOfChannelStreams() const
{
    return isInterleaved() ? 1 : m_streamDescription.mChannelsPerFrame;
}

uint32_t CAAudioStreamDescription::numberOfChannels() const
{
    return m_streamDescription.mChannelsPerFrame;
}

uint32_t CAAudioStreamDescription::sampleWordSize() const
{
    return (m_streamDescription.mBytesPerFrame > 0 && numberOfInterleavedChannels()) ? m_streamDescription.mBytesPerFrame / numberOfInterleavedChannels() :  0;
}

uint32_t CAAudioStreamDescription::bytesPerFrame() const
{
    return m_streamDescription.mBytesPerFrame;
}

uint32_t CAAudioStreamDescription::bytesPerPacket() const
{
    return m_streamDescription.mBytesPerPacket;
}

uint32_t CAAudioStreamDescription::formatFlags() const
{
    return m_streamDescription.mFormatFlags;
}

bool CAAudioStreamDescription::operator==(const AudioStreamBasicDescription& other) const
{
    return m_streamDescription == other;
}

bool CAAudioStreamDescription::operator!=(const AudioStreamBasicDescription& other) const
{
    return !operator==(other);
}

bool CAAudioStreamDescription::operator==(const AudioStreamDescription& other) const
{
    if (other.platformDescription().type != PlatformDescription::CAAudioStreamBasicType)
        return false;

    return operator==(*std::get<const AudioStreamBasicDescription*>(other.platformDescription().description));
}

bool CAAudioStreamDescription::operator!=(const AudioStreamDescription& other) const
{
    return !operator==(other);
}

const AudioStreamBasicDescription& CAAudioStreamDescription::streamDescription() const
{
    return m_streamDescription;
}

AudioStreamBasicDescription& CAAudioStreamDescription::streamDescription()
{
    return m_streamDescription;
}

}
