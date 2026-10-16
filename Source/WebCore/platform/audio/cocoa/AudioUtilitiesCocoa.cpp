/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUDIO) && PLATFORM(COCOA)
#include "AudioUtilitiesCocoa.h"

#include "AudioBus.h"
#include "AudioDestinationCocoa.h"

namespace WebCore {

AudioStreamBasicDescription audioStreamBasicDescriptionForAudioBus(AudioBus& bus)
{
    const int bytesPerFloat = sizeof(Float32);
    AudioStreamBasicDescription streamFormat { };
    streamFormat.mSampleRate = AudioDestinationCocoa::hardwareSampleRate();
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = static_cast<AudioFormatFlags>(kAudioFormatFlagsNativeFloatPacked) | static_cast<AudioFormatFlags>(kAudioFormatFlagIsNonInterleaved);
    streamFormat.mBytesPerPacket = bytesPerFloat;
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mBytesPerFrame = bytesPerFloat;
    streamFormat.mChannelsPerFrame = bus.numberOfChannels();
    streamFormat.mBitsPerChannel = 8 * bytesPerFloat;
    return streamFormat;
}

Vector<uint8_t> createESDescriptor(std::span<const uint8_t> audioSpecificConfig)
{
    // Create an ES_Descriptor that wraps the AudioSpecificConfig
    // ES_Descriptor structure:
    // - ES_DescrTag (1 byte): 0x03
    // - Length (variable): encoded length of the descriptor content
    // - ES_ID (2 bytes): elementary stream ID
    // - streamDependenceFlag (1 bit): 0
    // - URL_Flag (1 bit): 0
    // - OCRstreamFlag (1 bit): 0
    // - streamPriority (5 bits): 0
    // - DecoderConfigDescriptor

    if (audioSpecificConfig.size() > 255) // Can't be more than 256 bytes inclusive.
        return { };
    // Calculate the total size to pre-allocate the vector
    size_t audioSpecificConfigSize = audioSpecificConfig.size();
    size_t decoderSpecificInfoSize = 1 + 1 + audioSpecificConfigSize; // tag + length + data
    size_t decoderConfigDescriptorContentSize = 1 + 1 + 1 + 3 + 4 + 4 + decoderSpecificInfoSize; // fixed fields + DecoderSpecificInfo
    size_t esDescriptorContentSize = 2 + 1 + decoderConfigDescriptorContentSize; // ES_ID + flags + DecoderConfigDescriptor
    size_t totalSize = 1 + 1 + esDescriptorContentSize; // ES_DescrTag + Length + content

    // Pre-allocate the vector with the exact size needed
    Vector<uint8_t> esDescriptor;
    esDescriptor.reserveInitialCapacity(totalSize);

    // Calculate the size of DecoderConfigDescriptor content
    // DecoderConfigDescriptor structure:
    // - DecoderConfigDescrTag (1 byte): 0x04
    // - Length (variable): encoded length
    // - objectTypeIndication (1 byte): 0x40 for MPEG-4 Audio
    // - streamType (6 bits): 0x05 for audio
    // - upStream (1 bit): 0
    // - reserved (1 bit): 1
    // - bufferSizeDB (3 bytes): buffer size
    // - maxBitrate (4 bytes): maximum bitrate
    // - avgBitrate (4 bytes): average bitrate
    // - DecoderSpecificInfo

    // DecoderSpecificInfo structure:
    // - DecoderSpecificInfoTag (1 byte): 0x05
    // - Length (variable): length of AudioSpecificConfig
    // - AudioSpecificConfig data

    // ES_Descriptor
    esDescriptor.append(0x03); // ES_DescrTag
    esDescriptor.append(static_cast<uint8_t>(esDescriptorContentSize));

    // ES_ID (2 bytes)
    esDescriptor.append(0x00);
    esDescriptor.append(0x01);

    // Flags (1 byte): streamDependenceFlag=0, URL_Flag=0, OCRstreamFlag=0, streamPriority=0
    esDescriptor.append(0x00);

    // DecoderConfigDescriptor
    esDescriptor.append(0x04); // DecoderConfigDescrTag
    esDescriptor.append(static_cast<uint8_t>(decoderConfigDescriptorContentSize)); // Length

    // objectTypeIndication (1 byte): 0x40 for MPEG-4 Audio
    esDescriptor.append(0x40);

    // streamType (6 bits) = 0x05 for audio, upStream (1 bit) = 0, reserved (1 bit) = 1
    esDescriptor.append(0x15); // (0x05 << 2) | 0x01

    // bufferSizeDB (3 bytes) - use a reasonable default
    esDescriptor.append(0x00);
    esDescriptor.append(0x18);
    esDescriptor.append(0x00);

    // maxBitrate (4 bytes) - use a reasonable default
    esDescriptor.append(0x00);
    esDescriptor.append(0x02);
    esDescriptor.append(0x8B);
    esDescriptor.append(0x11);

    // avgBitrate (4 bytes) - use a reasonable default
    esDescriptor.append(0x00);
    esDescriptor.append(0x02);
    esDescriptor.append(0x8B);
    esDescriptor.append(0x11);

    // DecoderSpecificInfo
    esDescriptor.append(0x05); // DecoderSpecificInfoTag
    esDescriptor.append(static_cast<uint8_t>(audioSpecificConfigSize)); // Length

    // AudioSpecificConfig data
    esDescriptor.append(audioSpecificConfig);

    return esDescriptor;
}

// Reads the length of a descriptor, encoded as 1 to 4 bytes holding 7 bits each, the high bit meaning another byte follows.
static std::optional<size_t> readDescriptorLength(std::span<const uint8_t>& data)
{
    size_t length = 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (data.empty())
            return std::nullopt;
        uint8_t byte = data.front();
        data = data.subspan(1);
        length = (length << 7) | (byte & 0x7f);
        if (!(byte & 0x80))
            return length;
    }
    return std::nullopt;
}

// Reads the tag and the content of the descriptor at the start of data, and moves data after the descriptor.
static std::optional<std::pair<uint8_t, std::span<const uint8_t>>> readDescriptor(std::span<const uint8_t>& data)
{
    if (data.empty())
        return std::nullopt;
    uint8_t tag = data.front();
    data = data.subspan(1);
    auto length = readDescriptorLength(data);
    if (!length || *length > data.size())
        return std::nullopt;
    auto content = data.first(*length);
    data = data.subspan(*length);
    return std::pair { tag, content };
}

Vector<uint8_t> extractAudioSpecificConfigFromESDescriptor(std::span<const uint8_t> esDescriptor)
{
    constexpr uint8_t esDescriptorTag = 0x03;
    constexpr uint8_t decoderConfigDescriptorTag = 0x04;
    constexpr uint8_t decoderSpecificInfoTag = 0x05;

    // ES_Descriptor: ES_ID (2 bytes), flags (1 byte), optional fields, then the DecoderConfigDescriptor.
    auto esDescriptorContent = readDescriptor(esDescriptor);
    if (!esDescriptorContent || esDescriptorContent->first != esDescriptorTag)
        return { };

    auto content = esDescriptorContent->second;
    if (content.size() < 3)
        return { };
    uint8_t flags = content[2];
    content = content.subspan(3);

    constexpr uint8_t streamDependenceFlag = 0x80;
    constexpr uint8_t urlFlag = 0x40;
    constexpr uint8_t ocrStreamFlag = 0x20;
    if (flags & streamDependenceFlag) {
        if (content.size() < 2)
            return { };
        content = content.subspan(2); // dependsOn_ES_ID
    }
    if (flags & urlFlag) {
        if (content.empty() || content.size() < 1u + content.front())
            return { };
        content = content.subspan(1u + content.front()); // URLlength and URLstring
    }
    if (flags & ocrStreamFlag) {
        if (content.size() < 2)
            return { };
        content = content.subspan(2); // OCR_ES_Id
    }

    // DecoderConfigDescriptor: objectTypeIndication (1 byte), streamType (1 byte), bufferSizeDB (3 bytes), maxBitrate (4 bytes), avgBitrate (4 bytes), then the DecoderSpecificInfo.
    auto decoderConfigDescriptor = readDescriptor(content);
    if (!decoderConfigDescriptor || decoderConfigDescriptor->first != decoderConfigDescriptorTag)
        return { };

    constexpr size_t decoderConfigDescriptorFixedSize = 1 + 1 + 3 + 4 + 4;
    auto decoderConfigContent = decoderConfigDescriptor->second;
    if (decoderConfigContent.size() < decoderConfigDescriptorFixedSize)
        return { };
    decoderConfigContent = decoderConfigContent.subspan(decoderConfigDescriptorFixedSize);

    auto decoderSpecificInfo = readDescriptor(decoderConfigContent);
    if (!decoderSpecificInfo || decoderSpecificInfo->first != decoderSpecificInfoTag)
        return { };

    return decoderSpecificInfo->second;
}

}

#endif
