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

// https://webaudio.github.io/web-audio-api/#ChannelOrdering
// The order is the one of the WAVE channel mask, which CoreAudio labels Left to TopBackRight follow.
enum class WebAudioChannel : uint8_t {
    FrontLeft,
    FrontRight,
    FrontCenter,
    LowFrequency,
    BackLeft,
    BackRight,
    FrontLeftOfCenter,
    FrontRightOfCenter,
    BackCenter,
    SideLeft,
    SideRight,
    TopCenter,
    TopFrontLeft,
    TopFrontCenter,
    TopFrontRight,
    TopBackLeft,
    TopBackCenter,
    TopBackRight,
};

static std::optional<WebAudioChannel> webAudioChannel(AudioChannelLabel label, bool surroundsAreSides)
{
    switch (label) {
    case kAudioChannelLabel_Left:
        return WebAudioChannel::FrontLeft;
    case kAudioChannelLabel_Right:
        return WebAudioChannel::FrontRight;
    case kAudioChannelLabel_Center:
    case kAudioChannelLabel_Mono:
        return WebAudioChannel::FrontCenter;
    case kAudioChannelLabel_LFEScreen:
        return WebAudioChannel::LowFrequency;
    case kAudioChannelLabel_LeftSurround:
        return surroundsAreSides ? WebAudioChannel::SideLeft : WebAudioChannel::BackLeft;
    case kAudioChannelLabel_RightSurround:
        return surroundsAreSides ? WebAudioChannel::SideRight : WebAudioChannel::BackRight;
    case kAudioChannelLabel_RearSurroundLeft:
    case kAudioChannelLabel_LeftBackSurround:
        return WebAudioChannel::BackLeft;
    case kAudioChannelLabel_RearSurroundRight:
    case kAudioChannelLabel_RightBackSurround:
        return WebAudioChannel::BackRight;
    case kAudioChannelLabel_LeftCenter:
        return WebAudioChannel::FrontLeftOfCenter;
    case kAudioChannelLabel_RightCenter:
        return WebAudioChannel::FrontRightOfCenter;
    case kAudioChannelLabel_CenterSurround:
        return WebAudioChannel::BackCenter;
    case kAudioChannelLabel_LeftSurroundDirect:
    case kAudioChannelLabel_LeftSideSurround:
        return WebAudioChannel::SideLeft;
    case kAudioChannelLabel_RightSurroundDirect:
    case kAudioChannelLabel_RightSideSurround:
        return WebAudioChannel::SideRight;
    case kAudioChannelLabel_TopCenterSurround:
        return WebAudioChannel::TopCenter;
    case kAudioChannelLabel_VerticalHeightLeft:
        return WebAudioChannel::TopFrontLeft;
    case kAudioChannelLabel_VerticalHeightCenter:
        return WebAudioChannel::TopFrontCenter;
    case kAudioChannelLabel_VerticalHeightRight:
        return WebAudioChannel::TopFrontRight;
    case kAudioChannelLabel_TopBackLeft:
    case kAudioChannelLabel_LeftTopRear:
        return WebAudioChannel::TopBackLeft;
    case kAudioChannelLabel_TopBackCenter:
    case kAudioChannelLabel_CenterTopRear:
        return WebAudioChannel::TopBackCenter;
    case kAudioChannelLabel_TopBackRight:
    case kAudioChannelLabel_RightTopRear:
        return WebAudioChannel::TopBackRight;
    default:
        return { };
    }
}

// Returns the channel labels in Web Audio order, or std::nullopt if they are already in that order
// or if a channel has no Web Audio position.
static std::optional<Vector<AudioChannelLabel>> webAudioReorderedChannelLabels(std::span<const AudioChannelDescription> descriptions)
{
    // Surround channels are at the back, unless the layout also has back or back center channels; they are then at the side.
    bool surroundsAreSides = std::ranges::any_of(descriptions, [](auto& description) {
        switch (description.mChannelLabel) {
        case kAudioChannelLabel_RearSurroundLeft:
        case kAudioChannelLabel_RearSurroundRight:
        case kAudioChannelLabel_LeftBackSurround:
        case kAudioChannelLabel_RightBackSurround:
        case kAudioChannelLabel_CenterSurround:
            return true;
        default:
            return false;
        }
    });

    Vector<std::pair<WebAudioChannel, AudioChannelLabel>> channels;
    channels.reserveInitialCapacity(descriptions.size());
    for (auto& description : descriptions) {
        auto channel = webAudioChannel(description.mChannelLabel, surroundsAreSides);
        if (!channel)
            return { };
        channels.append({ *channel, description.mChannelLabel });
    }
    if (std::ranges::is_sorted(channels, { }, &std::pair<WebAudioChannel, AudioChannelLabel>::first))
        return { };
    std::ranges::sort(channels, { }, &std::pair<WebAudioChannel, AudioChannelLabel>::first);
    if (std::ranges::adjacent_find(channels, { }, &std::pair<WebAudioChannel, AudioChannelLabel>::first) != channels.end())
        return { };

    return channels.map([](auto& channel) {
        return channel.second;
    });
}

static bool isOggLayout(AudioChannelLayoutTag layoutTag)
{
    switch (layoutTag) {
    case kAudioChannelLayoutTag_Ogg_3_0:
    case kAudioChannelLayoutTag_Ogg_4_0:
    case kAudioChannelLayoutTag_Ogg_5_0:
    case kAudioChannelLayoutTag_Ogg_5_1:
    case kAudioChannelLayoutTag_Ogg_6_1:
    case kAudioChannelLayoutTag_Ogg_7_1:
        return true;
    default:
        return false;
    }
}

using ChannelLayoutWithSize = decltype(channelLayoutFromChannelLayoutTag(0));

// Returns the layout read by getProperty, allocated with the size given by getPropertySize.
template<typename GetPropertySize, typename GetProperty>
static ChannelLayoutWithSize channelLayoutFromProperty(const GetPropertySize& getPropertySize, const GetProperty& getProperty)
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
static ChannelLayoutWithSize channelLayoutWithDescriptionsForTag(AudioChannelLayoutTag layoutTag)
{
    return channelLayoutFromProperty([&](UInt32& size) {
        return PAL::AudioFormatGetPropertyInfo(kAudioFormatProperty_ChannelLayoutForTag, sizeof(layoutTag), &layoutTag, &size);
    }, [&](UInt32& size, AudioChannelLayout* layout) {
        return PAL::AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForTag, sizeof(layoutTag), &layoutTag, &size, layout);
    });
}

// Returns the converter's input channel layout. For a decoder, it is the layout of the decoded channels.
static ChannelLayoutWithSize converterInputChannelLayout(AudioConverterRef converter)
{
    return channelLayoutFromProperty([&](UInt32& size) {
        return PAL::AudioConverterGetPropertyInfo(converter, kAudioConverterInputChannelLayout, &size, nullptr);
    }, [&](UInt32& size, AudioChannelLayout* layout) {
        return PAL::AudioConverterGetProperty(converter, kAudioConverterInputChannelLayout, &size, layout);
    });
}

static void setConverterChannelLayouts(AudioConverterRef converter, const ChannelLayoutWithSize& inputLayout, const ChannelLayoutWithSize& outputLayout)
{
    if (!inputLayout.first || !outputLayout.first)
        return;
    if (auto error = PAL::AudioConverterSetProperty(converter, kAudioConverterInputChannelLayout, inputLayout.second, inputLayout.first.get()))
        RELEASE_LOG_ERROR(Media, "setWebAudioChannelOrderIfNeeded: setting input channel layout failed with %d", static_cast<int>(error));
    if (auto error = PAL::AudioConverterSetProperty(converter, kAudioConverterOutputChannelLayout, outputLayout.second, outputLayout.first.get()))
        RELEASE_LOG_ERROR(Media, "setWebAudioChannelOrderIfNeeded: setting output channel layout failed with %d", static_cast<int>(error));
}

void setWebAudioChannelOrderIfNeeded(AudioConverterRef converter, AudioChannelLayoutTag layoutTag, uint32_t numberOfChannels, ChannelReorderPolicy policy)
{
    // WebAudio API requires the channels to be ordered as per https://webaudio.github.io/web-audio-api/#ChannelOrdering.
    // However WebKit has been returning the codec's channel order for as long as WebAudio has been supported and
    // sites have adopted workarounds. To avoid breaking existing behaviour, ChannelReorderPolicy::OggLayoutsOnly
    // only reorders the Ogg layouts.
    if (numberOfChannels <= 2)
        return;
    if (policy == ChannelReorderPolicy::OggLayoutsOnly && !isOggLayout(layoutTag))
        return;

    // When the tag doesn't define the channels, as with ADTS AAC or a program config element, the decoder's layout
    // is used. Without a speaker layout, as with PCM, discrete channels or a channel with no Web Audio position, the
    // channels keep their coded order.
    bool tagDefinesChannels = layoutTag != kAudioChannelLayoutTag_UseChannelDescriptions
        && layoutTag != kAudioChannelLayoutTag_UseChannelBitmap
        && (layoutTag & kAudioChannelLayoutTag_Unknown) != kAudioChannelLayoutTag_Unknown;
    auto inputLayout = tagDefinesChannels ? channelLayoutFromChannelLayoutTag(layoutTag) : converterInputChannelLayout(converter);
    if (!inputLayout.first)
        return;

    ChannelLayoutWithSize layoutWithDescriptions;
    std::span<const AudioChannelDescription> descriptions;
    if (inputLayout.first->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelDescriptions)
        descriptions = PAL::span(*inputLayout.first);
    else {
        layoutWithDescriptions = channelLayoutWithDescriptionsForTag(inputLayout.first->mChannelLayoutTag);
        if (!layoutWithDescriptions.first)
            return;
        descriptions = PAL::span(*layoutWithDescriptions.first);
    }

    auto orderedLabels = webAudioReorderedChannelLabels(descriptions);
    if (!orderedLabels || orderedLabels->size() != numberOfChannels)
        return;
    setConverterChannelLayouts(converter, inputLayout, channelLayoutFromChannelLabels(orderedLabels->span()));
}

}

#endif
