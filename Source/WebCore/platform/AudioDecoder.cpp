/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2023 Igalia S.L
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
#include "AudioDecoder.h"

#if USE(AVFOUNDATION)
#include "AudioDecoderCocoa.h"
#endif

#if USE(GSTREAMER)
#include "AudioDecoderGStreamer.h"
#include "GStreamerRegistryScanner.h"
#endif

#include "AudioUtilities.h"
#include "SharedBuffer.h"
#include "TrackInfo.h"
#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

#if !USE(AVFOUNDATION)
static constexpr FourCC opusCodecName { "opus" };
static constexpr FourCC vorbisCodecName { "vorb" };
static constexpr FourCC flacCodecName { "flac" };
static constexpr FourCC mp3CodecName { ".mp3" };
static constexpr FourCC alawCodecName { "alaw" };
static constexpr FourCC ulawCodecName { "ulaw" };
static constexpr FourCC aacCodecName { "aac " };
static constexpr FourCC heAACCodecName { "aach" };
static constexpr FourCC heAACv2CodecName { "aacp" };
static constexpr FourCC lpcmCodecName { "lpcm" };

struct AudioDecoderConfiguration {
    String codec;
    AudioDecoder::Config config;
};

static std::optional<AudioDecoderConfiguration> audioDecoderConfiguration(const AudioInfo& info)
{
    RefPtr cookieData = info.cookieData();
    auto cookie = cookieData ? cookieData->span() : std::span<const uint8_t> { };
    auto codecName = info.codecName();

    auto configuration = [&](String&& codec, Vector<uint8_t>&& description = { }) {
        return AudioDecoderConfiguration { WTF::move(codec), { .description = WTF::move(description), .sampleRate = info.rate(), .numberOfChannels = info.channels(), .useWebAudioChannelOrder = false } };
    };

    if (codecName == opusCodecName) {
        if (cookie.empty())
            return { };
        // The pre-skip is applied through the samples' trimInterval, so the decoder must not apply it too.
        return configuration("opus"_s, AudioUtilities::opusHeadWithoutPreSkip(cookie));
    }
    if (codecName == vorbisCodecName) {
        if (cookie.empty())
            return { };
        return configuration("vorbis"_s, Vector<uint8_t>(cookie));
    }
    if (codecName == flacCodecName) {
        if (cookie.empty())
            return { };
        return configuration("flac"_s, Vector<uint8_t>(cookie));
    }
    if (codecName == mp3CodecName)
        return configuration("mp3"_s);
    if (codecName == alawCodecName)
        return configuration("alaw"_s);
    if (codecName == ulawCodecName)
        return configuration("ulaw"_s);

    // FIXME: AAC-LD and AAC-ELD ('aacl', 'aace') are not accepted by AudioDecoder::isCodecSupported.
    String aacCodec;
    if (codecName == aacCodecName)
        aacCodec = "mp4a.40.2"_s;
    else if (codecName == heAACCodecName)
        aacCodec = "mp4a.40.5"_s;
    else if (codecName == heAACv2CodecName)
        aacCodec = "mp4a.40.29"_s;
    if (!aacCodec.isNull()) {
        // AudioDecoder falls back to the track's sample rate and channel count when it has no AudioSpecificConfig.
        if (cookie.empty())
            return configuration(WTF::move(aacCodec));

        // An MPEG-4 audio cookie is an ES_Descriptor.
        auto audioSpecificConfig = AudioUtilities::extractAudioSpecificConfigFromESDescriptor(cookie);
        if (audioSpecificConfig.isEmpty())
            return { };
        return configuration(WTF::move(aacCodec), WTF::move(audioSpecificConfig));
    }

    // FIXME: only 32-bit floating point is recognised for 32-bit PCM, and PCM is assumed little-endian.
    if (codecName == lpcmCodecName) {
        switch (info.bitDepth()) {
        case 8:
            return configuration("pcm-u8"_s);
        case 16:
            return configuration("pcm-s16"_s);
        case 24:
            return configuration("pcm-s24"_s);
        case 32:
            return configuration("pcm-f32"_s);
        default:
            return { };
        }
    }

    return { };
}
#endif

bool AudioDecoder::isCodecSupported(const StringView& codec)
{
    bool isMPEG4AAC = codec == "mp4a.40.2"_s || codec == "mp4a.40.02"_s || codec == "mp4a.40.5"_s
        || codec == "mp4a.40.05"_s || codec == "mp4a.40.29"_s || codec == "mp4a.40.42"_s;
    bool isCodecAllowed = isMPEG4AAC || codec == "mp3"_s || codec == "opus"_s
        || codec == "alaw"_s || codec == "ulaw"_s || codec == "flac"_s
        || codec == "vorbis"_s || codec.startsWith("pcm-"_s);

    if (!isCodecAllowed)
        return false;

    bool result = false;
#if USE(GSTREAMER)
    auto& scanner = GStreamerRegistryScanner::singleton();
    result = scanner.isCodecSupported(GStreamerRegistryScanner::Configuration::Decoding, codec.toString());
#elif USE(AVFOUNDATION)
    result = !!AudioDecoderCocoa::isCodecSupported(codec);
#endif

    return result;
}

Ref<AudioDecoder::CreatePromise> AudioDecoder::create(const String& codecName, const Config& config, OutputCallback&& outputCallback)
{
#if USE(GSTREAMER)
    CreatePromise::Producer producer;
    Ref promise = producer.promise();
    CreateCallback callback = [producer = WTF::move(producer)] (auto&& result) mutable {
        producer.settle(WTF::move(result));
    };
    GStreamerAudioDecoder::create(codecName, config, WTF::move(callback), WTF::move(outputCallback));
    return promise;
#elif USE(AVFOUNDATION)
    return AudioDecoderCocoa::create(codecName, config, WTF::move(outputCallback));
#else
    UNUSED_PARAM(codecName);
    UNUSED_PARAM(config);
    UNUSED_PARAM(outputCallback);

    return CreatePromise::createAndReject("Not supported"_s);
#endif
}

bool AudioDecoder::canDecode(const AudioInfo& info)
{
    if (info.isDecoded() || !info.rate() || !info.channels())
        return false;

#if USE(AVFOUNDATION)
    return AudioDecoderCocoa::canDecode(info);
#else
    auto configuration = audioDecoderConfiguration(info);
    return configuration && isCodecSupported(configuration->codec);
#endif
}

Ref<AudioDecoder::CreatePromise> AudioDecoder::create(const AudioInfo& info, OutputCallback&& outputCallback)
{
#if USE(AVFOUNDATION)
    return AudioDecoderCocoa::create(info, WTF::move(outputCallback));
#else
    auto configuration = audioDecoderConfiguration(info);
    if (!configuration)
        return CreatePromise::createAndReject("Not supported"_s);
    return create(configuration->codec, configuration->config, WTF::move(outputCallback));
#endif
}

AudioDecoder::AudioDecoder() = default;
AudioDecoder::~AudioDecoder() = default;

}
