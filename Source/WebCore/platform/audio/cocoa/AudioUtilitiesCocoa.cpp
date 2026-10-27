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

#if PLATFORM(COCOA)
#include "AudioUtilitiesCocoa.h"

#include "AudioBus.h"
#include "AudioSession.h"
#include "CMUtilities.h"
#include "Logging.h"
#include <AudioToolbox/AudioConverter.h>
#include <AudioToolbox/AudioFormat.h>
#include <algorithm>
#include <bit>
#include <pal/cf/CoreAudioExtras.h>

#include <pal/cf/AudioToolboxSoftLink.h>

namespace WebCore {

AudioStreamBasicDescription audioStreamBasicDescriptionForAudioBus(AudioBus& bus)
{
    const int bytesPerFloat = sizeof(Float32);
    AudioStreamBasicDescription streamFormat { };
    streamFormat.mSampleRate = AudioSession::singleton().sampleRate();
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

// Returns the layout read by getProperty, allocated with the size given by getPropertySize.
template<typename GetPropertySize, typename GetProperty>
static AudioChannelLayoutWithSize channelLayoutFromProperty(const GetPropertySize& getPropertySize, const GetProperty& getProperty)
{
    UInt32 size = 0;
    if (getPropertySize(size) || size < offsetof(AudioChannelLayout, mChannelDescriptions))
        return { };
    auto layout = PAL::createAudioChannelLayout(static_cast<uint32_t>((size - offsetof(AudioChannelLayout, mChannelDescriptions)) / sizeof(AudioChannelDescription)), PAL::ShouldZeroMemory::Yes);
    size = static_cast<UInt32>(PAL::allocationSize(*layout));
    if (getProperty(size, layout.get()) || size < offsetof(AudioChannelLayout, mChannelDescriptions) + sizeof(AudioChannelDescription) * layout->mNumberChannelDescriptions)
        return { };
    return { WTF::move(layout), size };
}

// Returns the layout of the tag with its channel descriptions.
static AudioChannelLayoutWithSize channelLayoutWithDescriptionsForTag(AudioChannelLayoutTag layoutTag)
{
    return channelLayoutFromProperty([&](UInt32& size) {
        return PAL::AudioFormatGetPropertyInfo(kAudioFormatProperty_ChannelLayoutForTag, sizeof(layoutTag), &layoutTag, &size);
    }, [&](UInt32& size, AudioChannelLayout* layout) {
        return PAL::AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForTag, sizeof(layoutTag), &layoutTag, &size, layout);
    });
}

// Returns the converter's input channel layout. For a decoder, it is the layout of the decoded channels.
static AudioChannelLayoutWithSize converterInputChannelLayout(AudioConverterRef converter)
{
    return channelLayoutFromProperty([&](UInt32& size) {
        return PAL::AudioConverterGetPropertyInfo(converter, kAudioConverterInputChannelLayout, &size, nullptr);
    }, [&](UInt32& size, AudioChannelLayout* layout) {
        return PAL::AudioConverterGetProperty(converter, kAudioConverterInputChannelLayout, &size, layout);
    });
}

static bool setConverterChannelLayouts(AudioConverterRef converter, const AudioChannelLayoutWithSize& inputLayout, const AudioChannelLayoutWithSize& outputLayout)
{
    if (!inputLayout.first || !outputLayout.first)
        return false;
    if (auto error = PAL::AudioConverterSetProperty(converter, kAudioConverterInputChannelLayout, inputLayout.second, inputLayout.first.get())) {
        RELEASE_LOG_ERROR(Media, "setWebAudioChannelOrderIfNeeded: setting input channel layout failed with %d", static_cast<int>(error));
        return false;
    }
    if (auto error = PAL::AudioConverterSetProperty(converter, kAudioConverterOutputChannelLayout, outputLayout.second, outputLayout.first.get())) {
        RELEASE_LOG_ERROR(Media, "setWebAudioChannelOrderIfNeeded: setting output channel layout failed with %d", static_cast<int>(error));
        return false;
    }
    return true;
}

std::optional<MediaChannelLayout> setWebAudioChannelOrderIfNeeded(AudioConverterRef converter, AudioChannelLayoutTag layoutTag, uint32_t numberOfChannels)
{
    // When the tag doesn't define the channels, as with ADTS AAC or a program config element, the decoder's layout
    // is used. Without a speaker layout, as with PCM, the channels keep their coded order.
    bool tagDefinesChannels = layoutTag != kAudioChannelLayoutTag_UseChannelDescriptions
        && layoutTag != kAudioChannelLayoutTag_UseChannelBitmap
        && (layoutTag & kAudioChannelLayoutTag_Unknown) != kAudioChannelLayoutTag_Unknown;
    auto inputLayout = tagDefinesChannels ? channelLayoutFromChannelLayoutTag(layoutTag) : converterInputChannelLayout(converter);
    if (!inputLayout.first)
        return { };

    // Discrete and ambisonic channels have no speaker position, and a bitmap's channels are already in Web Audio
    // order; the channels keep their coded order. A layout that doesn't match the number of channels, such as
    // ambisonic components that don't fill an order, is discrete.
    auto inputLayoutTag = inputLayout.first->mChannelLayoutTag;
    if (auto layout = mediaChannelLayoutFromChannelLayout(*inputLayout.first)) {
        bool tagHasNumberOfChannels = inputLayoutTag != kAudioChannelLayoutTag_UseChannelDescriptions && inputLayoutTag != kAudioChannelLayoutTag_UseChannelBitmap;
        if (!layout->isValid(numberOfChannels) || (tagHasNumberOfChannels && (inputLayoutTag & 0xFFFF) != numberOfChannels))
            return MediaChannelLayout { .kind = MediaChannelLayout::Kind::Discrete };
        return layout;
    }

    if (numberOfChannels <= 2)
        return { };

    AudioChannelLayoutWithSize layoutWithDescriptions;
    std::span<const AudioChannelDescription> descriptions;
    if (inputLayoutTag == kAudioChannelLayoutTag_UseChannelDescriptions)
        descriptions = PAL::span(*inputLayout.first);
    else {
        layoutWithDescriptions = channelLayoutWithDescriptionsForTag(inputLayoutTag);
        if (!layoutWithDescriptions.first)
            return { };
        descriptions = PAL::span(*layoutWithDescriptions.first);
    }

    // When a channel has no Web Audio position, or two channels share one, the channels keep their coded order
    // and are discrete.
    auto channels = webAudioChannelsFromChannelDescriptions(descriptions);
    if (!channels || channels->size() != numberOfChannels)
        return MediaChannelLayout { .kind = MediaChannelLayout::Kind::Discrete };
    uint32_t mask = 0;
    for (auto channel : *channels)
        mask |= webAudioChannelBit(channel);
    if (static_cast<uint32_t>(std::popcount(mask)) != numberOfChannels)
        return MediaChannelLayout { .kind = MediaChannelLayout::Kind::Discrete };

    if (!std::ranges::is_sorted(*channels)) {
        Vector<std::pair<WebAudioChannel, AudioChannelLabel>> orderedChannels(numberOfChannels, [&](size_t index) {
            return std::pair { (*channels)[index], descriptions[index].mChannelLabel };
        });
        std::ranges::sort(orderedChannels, { }, &std::pair<WebAudioChannel, AudioChannelLabel>::first);
        auto orderedLabels = orderedChannels.map([](auto& channel) {
            return channel.second;
        });
        if (!setConverterChannelLayouts(converter, inputLayout, channelLayoutFromChannelLabels(orderedLabels.span())))
            return MediaChannelLayout { .kind = MediaChannelLayout::Kind::Discrete };
    }

    return MediaChannelLayout { .kind = MediaChannelLayout::Kind::Positional, .mask = mask };
}

}

#endif
