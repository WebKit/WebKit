/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "config.h"

#if USE(AVFOUNDATION) && ENABLE(MEDIA_SOURCE)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/TestNSBundleExtras.h"
#import <AVFoundation/AVFoundation.h>
#import <WebCore/AudioSampleFormat.h>
#import <WebCore/CMUtilities.h>
#import <WebCore/ContentType.h>
#import <WebCore/MediaSampleAVFObjC.h>
#import <WebCore/MediaSamplesBlock.h>
#import <WebCore/MediaSamplesDecoder.h>
#import <WebCore/MediaSourceConfiguration.h>
#import <WebCore/PacedMediaSamplesDecoder.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/SourceBufferParser.h>
#import <WebCore/TrackInfo.h>
#import <WebCore/VideoPixelFormat.h>
#if ENABLE(VP9)
#import <WebCore/VP9UtilitiesCocoa.h>
#endif
#if ENABLE(AV1)
#import <WebCore/AV1UtilitiesCocoa.h>
#endif
#import <cmath>
#import <memory>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <unistd.h>
#import <wtf/Lock.h>
#import <wtf/MonotonicTime.h>
#import <wtf/RunLoop.h>
#import <wtf/Vector.h>
#import <wtf/cf/TypeCastsCF.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

namespace TestWebKitAPI {

using namespace WebCore;

static constexpr auto allowedFilters = MediaSamplesDecoder::Filters { MediaSamplesDecoder::Filter::InProcess, MediaSamplesDecoder::Filter::SoftwareAllowed };

static Vector<UniqueRef<MediaSamplesBlock>> parseWebMFile(NSString *resourceName, const String& contentType)
{
    Vector<UniqueRef<MediaSamplesBlock>> blocks;

    RetainPtr fileData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:resourceName withExtension:@"webm"]];
    if (!fileData)
        return blocks;

    RefPtr parser = SourceBufferParser::create(ContentType { contentType }, MediaSourceConfiguration { });
    if (!parser)
        return blocks;

    parser->setCallOnClientThreadCallback([](auto&& function) {
        function();
    });
    parser->setDidProvideMediaDataCallback([&](Ref<MediaSampleAVFObjC>&& sample, uint64_t, const String&) {
        blocks.append(MediaSamplesBlock::fromMediaSample(sample.get()));
    });
    if (!parser->appendData(SharedBuffer::create(fileData.get())))
        return { };
    parser->flushPendingMediaData();
    return blocks;
}

// Demuxes the first audio track into compressed samples, as AudioFileReaderCocoa does.
static Vector<UniqueRef<MediaSamplesBlock>> parseAVFAudioFile(NSString *resourceName, NSString *extension, NSString *mimeType)
{
    Vector<UniqueRef<MediaSamplesBlock>> blocks;

    RetainPtr url = [NSBundle.test_resourcesBundle URLForResource:resourceName withExtension:extension];
    if (!url)
        return blocks;

    RetainPtr options = @{
        AVURLAssetPreferPreciseDurationAndTimingKey: @YES,
        AVURLAssetOutOfBandMIMETypeKey: mimeType
    };
    RetainPtr asset = adoptNS([PAL::allocAVURLAssetInstance() initWithURL:url.get() options:options.get()]);
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr<NSArray<AVAssetTrack *>> audioTracks = [asset tracksWithMediaType:AVMediaTypeAudio];
ALLOW_DEPRECATED_DECLARATIONS_END
    if (![audioTracks count])
        return blocks;

    RetainPtr assetReader = adoptNS([PAL::allocAVAssetReaderInstance() initWithAsset:asset.get() error:nil]);
    RetainPtr trackOutput = adoptNS([PAL::allocAVAssetReaderTrackOutputInstance() initWithTrack:[audioTracks firstObject] outputSettings:nil]);
    if (!assetReader || !trackOutput || ![assetReader canAddOutput:trackOutput.get()])
        return blocks;
    [assetReader addOutput:trackOutput.get()];
    if (![assetReader startReading])
        return blocks;

    while (RetainPtr sampleBuffer = adoptCF([trackOutput copyNextSampleBuffer])) {
        if (!PAL::CMSampleBufferGetDataBuffer(sampleBuffer.get()))
            continue;
        blocks.append(MediaSamplesBlock::fromMediaSample(MediaSampleAVFObjC::create(sampleBuffer.get(), 0)));
    }
    if ([assetReader status] == AVAssetReaderStatusFailed)
        return { };
    return blocks;
}

static Vector<UniqueRef<MediaSamplesBlock>> parseWebMAudioFile(NSString *resourceName)
{
    return parseWebMFile(resourceName, "audio/webm; codecs=opus"_s);
}

template<typename Promise>
static auto settle(Promise& promise)
{
    bool done = false;
    std::optional<typename Promise::Result> result;
    promise.whenSettled(RunLoop::mainSingleton(), [&](auto&& settledResult) {
        result = WTF::move(settledResult);
        done = true;
    });
    Util::run(&done);
    return WTF::move(*result);
}

static size_t decodedFrameCount(Vector<UniqueRef<MediaSamplesBlock>>&& blocks)
{
    if (blocks.isEmpty() || !blocks[0]->info()) {
        ADD_FAILURE() << "No samples were demuxed";
        return 0;
    }
    if (!MediaSamplesDecoder::canDecode(*blocks[0]->info(), allowedFilters)) {
        ADD_FAILURE() << "Track can't be decoded";
        return 0;
    }

    auto createPromise = MediaSamplesDecoder::create(*blocks[0]->info(), allowedFilters);
    auto createResult = settle(createPromise.get());
    if (!createResult) {
        ADD_FAILURE() << "Decoder creation failed";
        return 0;
    }
    Ref decoder = WTF::move(*createResult);

    size_t frames = 0;
    auto accumulate = [&](Vector<MediaSamplesBlock>&& outputs) {
        for (auto& output : outputs) {
            auto* info = dynamicDowncast<AudioInfo>(output.info());
            EXPECT_TRUE(info);
            if (!info)
                continue;
            for (auto& item : output)
                frames += item.data->size() / (info->channels() * sizeof(float));
        }
    };

    for (auto& block : blocks) {
        auto decodePromise = decoder->decode(WTF::move(block.get()));
        auto decodeResult = settle(decodePromise.get());
        EXPECT_TRUE(decodeResult);
        if (!decodeResult)
            return frames;
        accumulate(WTF::move(*decodeResult));
    }

    auto drainPromise = decoder->drain();
    auto drainResult = settle(drainPromise.get());
    EXPECT_TRUE(drainResult);
    if (drainResult)
        accumulate(WTF::move(*drainResult));
    return frames;
}

static Ref<AudioInfo> makeAudioInfo(FourCC codecName, Vector<uint8_t>&& cookie = { 1, 2, 3, 4 }, uint8_t bitDepth = 16, uint32_t channels = 2, std::optional<AudioSampleFormat> sampleFormat = std::nullopt)
{
    return AudioInfo::create({
        {
            .codecName = codecName
        }, {
            .rate = 48000,
            .channels = channels,
            .bitDepth = bitDepth,
            .cookieData = cookie.isEmpty() ? nullptr : RefPtr<SharedBuffer> { SharedBuffer::create(WTF::move(cookie)) },
            .sampleFormat = sampleFormat
        }
    });
}

static MediaSamplesBlock makePacketBlock(const AudioInfo& info, Vector<uint8_t>&& bytes, size_t frames)
{
    MediaSamplesBlock::SamplesVector items;
    items.append({
        .presentationTime = MediaTime::zeroTime(),
        .duration = MediaTime(frames, info.rate()),
        .data = SharedBuffer::create(WTF::move(bytes)),
        .flags = MediaSample::IsSync
    });
    return MediaSamplesBlock { &info, WTF::move(items) };
}

static Vector<float> decodeToFloats(MediaSamplesDecoder& decoder, MediaSamplesBlock&& block)
{
    Vector<float> samples;
    auto collect = [&](Vector<MediaSamplesBlock>&& outputs) {
        for (auto& output : outputs) {
            for (auto& item : output) {
                auto bytes = item.data->makeContiguous();
                for (size_t offset = 0; offset + sizeof(float) <= bytes->size(); offset += sizeof(float)) {
                    float value;
                    memcpySpan(asMutableByteSpan(value), bytes->span().subspan(offset, sizeof(float)));
                    samples.append(value);
                }
            }
        }
    };

    auto decodePromise = decoder.decode(WTF::move(block));
    auto decodeResult = settle(decodePromise.get());
    EXPECT_TRUE(decodeResult);
    if (decodeResult)
        collect(WTF::move(*decodeResult));
    auto drainPromise = decoder.drain();
    auto drainResult = settle(drainPromise.get());
    EXPECT_TRUE(drainResult);
    if (drainResult)
        collect(WTF::move(*drainResult));
    return samples;
}

static Ref<MediaSamplesDecoder> createDecoder(const TrackInfo& info)
{
    auto promise = MediaSamplesDecoder::create(info, allowedFilters);
    auto result = settle(promise.get());
    RELEASE_ASSERT(result);
    return WTF::move(*result);
}

// The magic cookie of content/test-flac.mp4: a 'dfLa' box with a STREAMINFO block (4608 frames per block, 48 kHz, stereo, 16-bit).
static Vector<uint8_t> flacCookie()
{
    return {
        0x00, 0x00, 0x00, 0x32, 0x64, 0x66, 0x4C, 0x61, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x22,
        0x12, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4A, 0x58, 0x0B, 0xB8, 0x02, 0xF0, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00
    };
}

static RefPtr<const TrackInfo> firstTrackInfo(Vector<UniqueRef<MediaSamplesBlock>>&& blocks)
{
    if (blocks.isEmpty())
        return nullptr;
    return blocks[0]->info();
}

TEST(MediaSamplesDecoder, CanDecodeFilters)
{
    RefPtr opus = firstTrackInfo(parseWebMAudioFile(@"opus"));
    ASSERT_TRUE(opus);

    EXPECT_TRUE(MediaSamplesDecoder::canDecode(*opus, allowedFilters));
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(*opus, allowedFilters | MediaSamplesDecoder::Filter::UseSharedMemory));
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(*opus, allowedFilters | MediaSamplesDecoder::Filter::HardwareAllowed));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(*opus, { MediaSamplesDecoder::Filter::InProcess }));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(*opus, { MediaSamplesDecoder::Filter::SoftwareAllowed }));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(*opus, { }));
}

TEST(MediaSamplesDecoder, CanDecodeSupportedCodecs)
{
#if PLATFORM(MAC)
    RefPtr vorbis = firstTrackInfo(parseAVFAudioFile(@"vbr-96kbps-44khz", @"ogg", @"audio/ogg"));
    ASSERT_TRUE(vorbis);
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(*vorbis, allowedFilters));
#endif
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "flac" }, flacCookie()), allowedFilters));
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { ".mp3" }, { }), allowedFilters));
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "alaw" }, { }, 8), allowedFilters));
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "ulaw" }, { }, 8), allowedFilters));
    for (uint8_t bitDepth : { 8, 16, 24, 32 })
        EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "lpcm" }, { }, bitDepth), allowedFilters));

    // AAC-LC, AAC-LD and AAC-ELD.
    for (NSString *name : { @"silence-long", @"aac-ld", @"aac-eld" }) {
        RefPtr aac = firstTrackInfo(parseAVFAudioFile(name, @"m4a", @"audio/mp4"));
        ASSERT_TRUE(aac) << name.UTF8String;
        EXPECT_TRUE(MediaSamplesDecoder::canDecode(*aac, allowedFilters)) << name.UTF8String;
    }

    EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "ac-3" }, { }), allowedFilters));
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "ec-3" }, { }), allowedFilters));
}

#if HAVE(OPUS_DECODE_MULTICHANNEL)
TEST(MediaSamplesDecoder, CanDecodeMultichannelOpus)
{
    RefPtr opus = firstTrackInfo(parseWebMAudioFile(@"opus-5.1"));
    ASSERT_TRUE(opus);
    EXPECT_EQ(downcast<AudioInfo>(*opus).channels(), 6u);
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(*opus, allowedFilters));
}

TEST(MediaSamplesDecoder, DecodeMultichannelOpusChannelLayout)
{
    auto blocks = parseWebMAudioFile(@"opus-5.1");
    ASSERT_FALSE(blocks.isEmpty());
    ASSERT_TRUE(blocks[0]->info());

    auto createPromise = MediaSamplesDecoder::create(*blocks[0]->info(), allowedFilters);
    auto createResult = settle(createPromise.get());
    ASSERT_TRUE(createResult);
    Ref decoder = WTF::move(*createResult);

    // 5.1 is decoded in Web Audio order: FL FR FC LFE BL BR.
    constexpr MediaChannelLayout expectedLayout { .kind = MediaChannelLayout::Kind::Positional, .mask = 0x3F };
    size_t decodedBlocks = 0;
    auto check = [&](Vector<MediaSamplesBlock>&& outputs) {
        for (auto& output : outputs) {
            auto* info = dynamicDowncast<AudioInfo>(output.info());
            ASSERT_TRUE(info);
            EXPECT_EQ(info->channels(), 6u);
            EXPECT_EQ(info->channelLayout(), expectedLayout);
            ++decodedBlocks;
        }
    };

    for (auto& block : blocks) {
        auto decodePromise = decoder->decode(WTF::move(block.get()));
        auto decodeResult = settle(decodePromise.get());
        ASSERT_TRUE(decodeResult);
        check(WTF::move(*decodeResult));
    }
    auto drainPromise = decoder->drain();
    auto drainResult = settle(drainPromise.get());
    ASSERT_TRUE(drainResult);
    check(WTF::move(*drainResult));

    EXPECT_GT(decodedBlocks, 0u);
}
#endif

TEST(MediaSamplesDecoder, CanDecodeRejectsUnsupportedTracks)
{
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "opus" }, { }), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "opus" }, { 1, 2, 3, 4 }, 16, 6), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "opus" }, { 1, 2, 3, 4 }, 16, 2, AudioSampleFormat::F32), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "vorb" }, { }), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "flac" }, { }), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "flac" }), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "lpcm" }, { }, 12), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "aac " }, { 0x03, 0xFF }), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "aac " }, { 0x00, 0x00, 0x00, 0x0C, 'f', 'r', 'm', 'a', 'm', 'p', '4', 'a' }), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "mp4a" }), allowedFilters));
}

TEST(MediaSamplesDecoder, CreateRejectsUnsupportedTracks)
{
    Ref unsupported = makeAudioInfo(FourCC { "mp4a" });
    auto promise = MediaSamplesDecoder::create(unsupported, allowedFilters);
    auto result = settle(promise.get());
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), PlatformMediaError::NotSupportedError);
}

#if ENABLE(ENCRYPTED_MEDIA)
TEST(MediaSamplesDecoder, CreateRejectsEncryptedTracks)
{
    Ref encrypted = AudioInfo::create({
        {
            .codecName = FourCC { "opus" },
            .encryptionData = EncryptionDataCollection {
                { EncryptionBoxType::CommonEncryptionTrackEncryptionBox, SharedBuffer::create(Vector<uint8_t> { 1, 2, 3, 4 }) },
                FourCC { "opus" }
            }
        }, {
            .rate = 48000,
            .channels = 2,
            .cookieData = SharedBuffer::create(Vector<uint8_t> { 1, 2, 3, 4 })
        }
    });

    EXPECT_FALSE(MediaSamplesDecoder::canDecode(encrypted, allowedFilters));
    auto promise = MediaSamplesDecoder::create(encrypted, allowedFilters);
    auto result = settle(promise.get());
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), PlatformMediaError::ProtectedContent);
}

TEST(MediaSamplesDecoder, DecodeRejectsEncryptedSamples)
{
    Ref info = makeAudioInfo(FourCC { "lpcm" }, { }, 16, 1);
    Ref decoder = createDecoder(info);

    auto block = makePacketBlock(info, Vector<uint8_t> { 0, 0, 0, 0 }, 2);
    auto items = block.takeSamples();
    items[0].cryptorIV = SharedBuffer::create(Vector<uint8_t> { 1, 2, 3, 4 });
    MediaSamplesBlock encryptedBlock { info.ptr(), WTF::move(items) };

    auto promise = decoder->decode(WTF::move(encryptedBlock));
    auto result = settle(promise.get());
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), PlatformMediaError::ProtectedContent);
}
#endif

TEST(MediaSamplesDecoder, DecodePCM)
{
    Ref info = makeAudioInfo(FourCC { "lpcm" }, { }, 16, 1);
    Ref decoder = createDecoder(info);

    // Little-endian signed 16-bit samples.
    Vector<uint8_t> bytes { 0x00, 0x00, 0x00, 0x40, 0x00, 0xC0, 0x00, 0x00 };
    auto samples = decodeToFloats(decoder, makePacketBlock(info, WTF::move(bytes), 4));

    ASSERT_EQ(samples.size(), 4u);
    EXPECT_NEAR(samples[0], 0.0f, 1e-4f);
    EXPECT_NEAR(samples[1], 0.5f, 1e-4f);
    EXPECT_NEAR(samples[2], -0.5f, 1e-4f);
    EXPECT_NEAR(samples[3], 0.0f, 1e-4f);
}

TEST(MediaSamplesDecoder, DecodePCMTrimmed)
{
    Ref info = makeAudioInfo(FourCC { "lpcm" }, { }, 16, 1);
    Ref decoder = createDecoder(info);

    // Little-endian signed 16-bit samples: 0, 1/8, 2/8, ..., 7/8.
    Vector<uint8_t> bytes;
    for (uint8_t i = 0; i < 8; ++i) {
        bytes.append(0x00);
        bytes.append(i * 0x10);
    }
    auto items = makePacketBlock(info, WTF::move(bytes), 8).takeSamples();
    items[0].presentationTime = MediaTime(2, 48000);
    items[0].duration = MediaTime(5, 48000);
    items[0].trimInterval = { MediaTime(2, 48000), MediaTime(1, 48000) };

    auto decodePromise = decoder->decode(MediaSamplesBlock { info.ptr(), WTF::move(items) });
    auto decodeResult = settle(decodePromise.get());
    ASSERT_TRUE(decodeResult);
    ASSERT_EQ(decodeResult->size(), 1u);
    auto& output = (*decodeResult)[0];
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output[0].presentationTime, MediaTime(2, 48000));
    EXPECT_EQ(output[0].duration, MediaTime(5, 48000));

    auto data = output[0].data->makeContiguous();
    ASSERT_EQ(data->size(), 5 * sizeof(float));
    for (size_t i = 0; i < 5; ++i) {
        float value;
        memcpySpan(asMutableByteSpan(value), data->span().subspan(i * sizeof(float), sizeof(float)));
        EXPECT_NEAR(value, (i + 2) / 8.0f, 1e-4f);
    }
}

TEST(MediaSamplesDecoder, DecodeFullyTrimmedIsDropped)
{
    Ref info = makeAudioInfo(FourCC { "lpcm" }, { }, 16, 1);
    Ref decoder = createDecoder(info);

    auto items = makePacketBlock(info, Vector<uint8_t> { 0x00, 0x40, 0x00, 0xC0 }, 2).takeSamples();
    items[0].trimInterval = { items[0].duration, MediaTime::zeroTime() };
    items[0].presentationTime += items[0].duration;
    items[0].duration = MediaTime::zeroTime();

    auto samples = decodeToFloats(decoder, MediaSamplesBlock { info.ptr(), WTF::move(items) });
    EXPECT_TRUE(samples.isEmpty());
}

TEST(MediaSamplesBlock, CMSampleBufferRoundTripKeepsTrims)
{
    auto blocks = parseWebMAudioFile(@"test-mse-audio");
    ASSERT_FALSE(blocks.isEmpty());
    RefPtr info = blocks[0]->info();
    auto items = blocks[0]->takeSamples();
    ASSERT_FALSE(items.isEmpty());

    auto trimAtEnd = MediaTime(120, 48000);
    ASSERT_GT(items.last().duration, trimAtEnd);
    items.last().duration -= trimAtEnd;
    items.last().trimInterval.second = trimAtEnd;
    auto expected = items;

    auto sampleBuffer = toCMSampleBuffer(MediaSamplesBlock { info.get(), WTF::move(items) });
    ASSERT_TRUE(sampleBuffer);
    RetainPtr trimAtEndDict = dynamic_cf_cast<CFDictionaryRef>(PAL::CMGetAttachment(sampleBuffer->get(), PAL::kCMSampleBufferAttachmentKey_TrimDurationAtEnd, nullptr));
    ASSERT_TRUE(trimAtEndDict);
    EXPECT_EQ(PAL::toMediaTime(PAL::CMTimeMakeFromDictionary(trimAtEndDict.get())), trimAtEnd);

    auto roundTrip = samplesBlockFromCMSampleBuffer(sampleBuffer->get(), info.get());
    ASSERT_EQ(roundTrip->size(), expected.size());
    for (size_t index = 0; index < expected.size(); ++index) {
        EXPECT_EQ(roundTrip.get()[index].presentationTime, expected[index].presentationTime);
        EXPECT_EQ(roundTrip.get()[index].duration, expected[index].duration);
        EXPECT_EQ(roundTrip.get()[index].trimInterval, expected[index].trimInterval);
    }
}

TEST(MediaSamplesDecoder, DecodeULaw)
{
    Ref info = makeAudioInfo(FourCC { "ulaw" }, { }, 8, 1);
    Ref decoder = createDecoder(info);

    // 0xFF decodes to silence, 0x80 to the largest positive value and 0x00 to the largest negative one.
    Vector<uint8_t> bytes { 0xFF, 0x80, 0x00, 0xFF };
    auto samples = decodeToFloats(decoder, makePacketBlock(info, WTF::move(bytes), 4));

    ASSERT_EQ(samples.size(), 4u);
    EXPECT_NEAR(samples[0], 0.0f, 1e-3f);
    EXPECT_GT(samples[1], 0.9f);
    EXPECT_LT(samples[2], -0.9f);
    EXPECT_NEAR(samples[3], 0.0f, 1e-3f);
}

TEST(MediaSamplesDecoder, DecodeOpus)
{
    auto blocks = parseWebMAudioFile(@"test-mse-audio");
    ASSERT_FALSE(blocks.isEmpty());
    ASSERT_TRUE(blocks[0]->info());
    ASSERT_TRUE(MediaSamplesDecoder::canDecode(*blocks[0]->info(), allowedFilters));

    auto createPromise = MediaSamplesDecoder::create(*blocks[0]->info(), allowedFilters);
    auto createResult = settle(createPromise.get());
    ASSERT_TRUE(createResult);
    Ref decoder = WTF::move(*createResult);
    EXPECT_TRUE(decoder->canContinueWith(*blocks[0]->info()));

    MediaTime expectedDuration = MediaTime::zeroTime();
    for (auto& block : blocks) {
        for (auto& item : block.get())
            expectedDuration += item.duration;
    }
    auto* inputInfo = dynamicDowncast<AudioInfo>(blocks[0]->info());
    ASSERT_TRUE(inputInfo);
    size_t expectedFrames = static_cast<size_t>(expectedDuration.toTimeScale(inputInfo->rate()).timeValue());

    size_t decodedFrames = 0;
    std::optional<MediaTime> firstPresentationTime;
    auto accumulate = [&](Vector<MediaSamplesBlock>&& outputs) {
        for (auto& output : outputs) {
            ASSERT_TRUE(output.isAudio());
            auto* info = dynamicDowncast<AudioInfo>(output.info());
            ASSERT_TRUE(info);
            EXPECT_EQ(info->sampleFormat(), AudioSampleFormat::F32);
            for (auto& item : output) {
                ASSERT_TRUE(item.data);
                if (!firstPresentationTime)
                    firstPresentationTime = item.presentationTime;
                size_t frames = static_cast<size_t>(std::llround(item.duration.toDouble() * info->rate()));
                EXPECT_EQ(item.data->size(), frames * info->channels() * sizeof(float));
                decodedFrames += frames;
            }
        }
    };

    for (auto& block : blocks) {
        auto decodePromise = decoder->decode(WTF::move(block.get()));
        auto decodeResult = settle(decodePromise.get());
        ASSERT_TRUE(decodeResult);
        accumulate(WTF::move(*decodeResult));
    }

    auto drainPromise = decoder->drain();
    auto drainResult = settle(drainPromise.get());
    ASSERT_TRUE(drainResult);
    accumulate(WTF::move(*drainResult));

    EXPECT_GT(decodedFrames, 0u);
    EXPECT_EQ(decodedFrames, expectedFrames);
    ASSERT_TRUE(firstPresentationTime);
    EXPECT_GE(*firstPresentationTime, MediaTime::zeroTime());
}

TEST(MediaSamplesDecoder, DecodeOpusIntoSharedMemory)
{
    auto blocks = parseWebMAudioFile(@"test-mse-audio");
    ASSERT_FALSE(blocks.isEmpty());

    auto createPromise = MediaSamplesDecoder::create(*blocks[0]->info(), allowedFilters | MediaSamplesDecoder::Filter::UseSharedMemory);
    auto createResult = settle(createPromise.get());
    ASSERT_TRUE(createResult);
    Ref decoder = WTF::move(*createResult);

    size_t outputBytes = 0;
    for (auto& block : blocks) {
        auto decodePromise = decoder->decode(WTF::move(block.get()));
        auto decodeResult = settle(decodePromise.get());
        ASSERT_TRUE(decodeResult);
        for (auto& output : *decodeResult) {
            for (auto& item : output)
                outputBytes += item.data->size();
        }
    }
    auto drainPromise = decoder->drain();
    auto drainResult = settle(drainPromise.get());
    ASSERT_TRUE(drainResult);
    for (auto& output : *drainResult) {
        for (auto& item : output)
            outputBytes += item.data->size();
    }
    EXPECT_GT(outputBytes, 0u);
}

TEST(MediaSamplesDecoder, FlushDiscardsPendingOutput)
{
    auto blocks = parseWebMAudioFile(@"test-mse-audio");
    ASSERT_FALSE(blocks.isEmpty());

    auto createPromise = MediaSamplesDecoder::create(*blocks[0]->info(), allowedFilters);
    auto createResult = settle(createPromise.get());
    ASSERT_TRUE(createResult);
    Ref decoder = WTF::move(*createResult);

    auto decodePromise = decoder->decode(WTF::move(blocks[0].get()));
    EXPECT_TRUE(settle(decodePromise.get()));

    auto flushPromise = decoder->flush();
    EXPECT_TRUE(settle(flushPromise.get()));

    auto drainPromise = decoder->drain();
    auto drainResult = settle(drainPromise.get());
    ASSERT_TRUE(drainResult);
    EXPECT_TRUE(drainResult->isEmpty());
}

TEST(MediaSamplesDecoder, FlushCancelsPendingOperations)
{
    auto blocks = parseWebMAudioFile(@"test-mse-audio");
    ASSERT_GE(blocks.size(), 2u);

    auto createPromise = MediaSamplesDecoder::create(*blocks[0]->info(), allowedFilters);
    auto createResult = settle(createPromise.get());
    ASSERT_TRUE(createResult);
    Ref decoder = WTF::move(*createResult);

    auto firstDecodePromise = decoder->decode(WTF::move(blocks[0].get()));
    auto pendingDrainPromise = decoder->drain();
    auto flushPromise = decoder->flush();
    auto secondDecodePromise = decoder->decode(WTF::move(blocks[1].get()));
    auto drainPromise = decoder->drain();

    auto firstDecodeResult = settle(firstDecodePromise.get());
    ASSERT_FALSE(firstDecodeResult);
    EXPECT_EQ(firstDecodeResult.error(), PlatformMediaError::Cancelled);
    auto pendingDrainResult = settle(pendingDrainPromise.get());
    ASSERT_FALSE(pendingDrainResult);
    EXPECT_EQ(pendingDrainResult.error(), PlatformMediaError::Cancelled);
    EXPECT_TRUE(settle(flushPromise.get()));
    auto secondDecodeResult = settle(secondDecodePromise.get());
    ASSERT_TRUE(secondDecodeResult);
    auto drainResult = settle(drainPromise.get());
    ASSERT_TRUE(drainResult);
    EXPECT_FALSE(secondDecodeResult->isEmpty() && drainResult->isEmpty());
}

TEST(MediaSamplesDecoder, CanContinueWithRejectsChangedFormat)
{
    auto blocks = parseWebMAudioFile(@"test-mse-audio");
    ASSERT_FALSE(blocks.isEmpty());

    auto createPromise = MediaSamplesDecoder::create(*blocks[0]->info(), allowedFilters);
    auto createResult = settle(createPromise.get());
    ASSERT_TRUE(createResult);
    Ref decoder = WTF::move(*createResult);

    auto& original = downcast<AudioInfo>(*blocks[0]->info());
    EXPECT_TRUE(decoder->canContinueWith(original));

    Ref differentRate = AudioInfo::create({
        {
            .codecName = original.codecName(),
            .codecString = original.codecString()
        }, {
            .rate = original.rate() / 2,
            .channels = original.channels(),
            .framesPerPacket = original.framesPerPacket(),
            .bitDepth = original.bitDepth(),
            .cookieData = original.cookieData()
        }
    });
    EXPECT_FALSE(decoder->canContinueWith(differentRate));
    EXPECT_FALSE(decoder->canContinueWith(makeAudioInfo(FourCC { "vorb" })));

    Ref pcm = makeAudioInfo(FourCC { "lpcm" }, { }, 16);
    Ref pcmDecoder = createDecoder(pcm);
    EXPECT_TRUE(pcmDecoder->canContinueWith(pcm));
    EXPECT_FALSE(pcmDecoder->canContinueWith(makeAudioInfo(FourCC { "lpcm" }, { }, 24)));
}

// The expected frame counts are those checked by LayoutTests/webaudio/decode-audio-data-*.html, at the file's sample rate.
TEST(MediaSamplesDecoder, FrameCountMatchesWebAudioFLAC)
{
    EXPECT_EQ(decodedFrameCount(parseAVFAudioFile(@"flac", @"flac", @"audio/flac")), 47688u);
}

TEST(MediaSamplesDecoder, FrameCountMatchesWebAudioMP3)
{
    EXPECT_EQ(decodedFrameCount(parseAVFAudioFile(@"half-a-second-48000", @"mp3", @"audio/mpeg")), 24000u);
}

TEST(MediaSamplesDecoder, FrameCountMatchesWebAudioMP3At8000)
{
    // decode-audio-data-mp3-8000.html expects 24000 frames once resampled to 48kHz.
    EXPECT_EQ(decodedFrameCount(parseAVFAudioFile(@"half-a-second-8000", @"mp3", @"audio/mpeg")), 4000u);
}

TEST(MediaSamplesDecoder, FrameCountMatchesWebAudioMP4Opus)
{
    EXPECT_EQ(decodedFrameCount(parseAVFAudioFile(@"opus", @"mp4", @"audio/mp4")), 48000u);
}

TEST(MediaSamplesDecoder, FrameCountMatchesWebAudioOggOpus)
{
    EXPECT_EQ(decodedFrameCount(parseAVFAudioFile(@"opus", @"ogg", @"audio/ogg")), 9288u);
}

#if PLATFORM(MAC)
TEST(MediaSamplesDecoder, FrameCountMatchesWebAudioOggVorbis)
{
    EXPECT_EQ(decodedFrameCount(parseAVFAudioFile(@"vbr-96kbps-44khz", @"ogg", @"audio/ogg")), 44100u);
}
#endif

TEST(MediaSamplesDecoder, FrameCountMatchesWebAudioWebMOpus)
{
    EXPECT_EQ(decodedFrameCount(parseWebMAudioFile(@"opus")), 47688u);
}

TEST(MediaSamplesDecoder, FrameCountMatchesWebAudioWAV)
{
    EXPECT_EQ(decodedFrameCount(parseAVFAudioFile(@"pinknoise", @"wav", @"audio/wav")), 176400u);
}

static Ref<VideoInfo> makeVideoInfo(FourCC codecName, const String& codecString, uint8_t bitDepth = 8, FloatSize size = { 320, 240 }, std::optional<VideoPixelFormat> pixelFormat = std::nullopt)
{
    return VideoInfo::create({
        {
            .codecName = codecName,
            .codecString = codecString
        }, {
            .size = size,
            .displaySize = size,
            .bitDepth = bitDepth,
            .pixelFormat = pixelFormat
        }
    });
}

static bool isVPXDecodingAvailable()
{
    return MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "vp08" }, "vp08.00.10.08"_s), allowedFilters);
}

static Vector<UniqueRef<MediaSamplesBlock>> parseWebMVideoFile(NSString *resourceName, const String& contentType)
{
    auto blocks = parseWebMFile(resourceName, contentType);
    blocks.removeAllMatching([](auto& block) {
        return !block->isVideo();
    });
    return blocks;
}

struct DecodedVideo {
    Vector<Vector<uint8_t>> frames;
    Vector<MediaTime> inputPresentationTimes;
    Vector<MediaTime> outputPresentationTimes;
    Vector<FloatSize> outputSizes;
    RefPtr<const TrackInfo> outputInfo;
};

static DecodedVideo decodeVideoBlocks(MediaSamplesDecoder& decoder, Vector<UniqueRef<MediaSamplesBlock>>& blocks)
{
    DecodedVideo result;
    auto collect = [&](Vector<MediaSamplesBlock>&& outputs) {
        for (auto& output : outputs) {
            if (!result.outputInfo)
                result.outputInfo = output.info();
            for (auto& item : output) {
                result.frames.append(Vector<uint8_t>(item.data->makeContiguous()->span()));
                result.outputPresentationTimes.append(item.presentationTime);
                result.outputSizes.append(downcast<VideoInfo>(*output.info()).size());
                EXPECT_TRUE(item.isSync());
            }
        }
    };

    for (auto& block : blocks) {
        for (auto& item : block.get())
            result.inputPresentationTimes.append(item.presentationTime);
        auto decodePromise = decoder.decode(WTF::move(block.get()));
        auto decodeResult = settle(decodePromise.get());
        EXPECT_TRUE(decodeResult);
        if (decodeResult)
            collect(WTF::move(*decodeResult));
    }

    auto drainPromise = decoder.drain();
    auto drainResult = settle(drainPromise.get());
    EXPECT_TRUE(drainResult);
    if (drainResult)
        collect(WTF::move(*drainResult));
    return result;
}

static constexpr auto vp8ContentType = "video/webm; codecs=vp8"_s;

#if ENABLE(VP9)
class ScopedVP9HardwareDecoder {
public:
    explicit ScopedVP9HardwareDecoder(bool available) { VP9TestingOverrides::singleton().setVP9HardwareDecoderEnabledOverride(available); }
    ~ScopedVP9HardwareDecoder() { VP9TestingOverrides::singleton().setVP9HardwareDecoderEnabledOverride(std::nullopt); }
};
#endif

#if ENABLE(AV1)
static constexpr auto av1ContentType = "video/webm; codecs=av01.0.08M.08"_s;

class ScopedAV1HardwareDecoder {
public:
    explicit ScopedAV1HardwareDecoder(bool available) { setAV1HardwareDecoderAvailableOverrideForTesting(available); }
    ~ScopedAV1HardwareDecoder() { setAV1HardwareDecoderAvailableOverrideForTesting(std::nullopt); }
};
#endif

TEST(MediaSamplesDecoder, CanDecodeVP8)
{
    if (!isVPXDecodingAvailable())
        GTEST_SKIP() << "VPx software decoding is not available";

    Ref vp8 = makeVideoInfo(FourCC { "vp08" }, "vp08.00.10.08"_s);
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(vp8, allowedFilters | MediaSamplesDecoder::Filter::UseSharedMemory));
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(vp8, allowedFilters | MediaSamplesDecoder::Filter::HardwareAllowed));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(vp8, { MediaSamplesDecoder::Filter::InProcess }));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(vp8, { MediaSamplesDecoder::Filter::SoftwareAllowed }));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "vp08" }, "vp08.00.10.10"_s, 10), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "vp08" }, "vp08.00.10.08"_s, 8, { }), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "vp08" }, "vp08.00.10.08"_s, 8, { 320, 240 }, VideoPixelFormat::I420), allowedFilters));
}

TEST(MediaSamplesDecoder, CanDecodeRejectsHardwareDecodableVideo)
{
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "avc1" }, "avc1.640028"_s), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "avc3" }, "avc3.640028"_s), allowedFilters));

#if ENABLE(AV1)
    {
        ScopedAV1HardwareDecoder hardwareDecoder { true };
        EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "av01" }, "av01.0.04M.08"_s), allowedFilters));
    }
#endif

#if ENABLE(VP9)
    ScopedVP9HardwareDecoder hardwareDecoder { true };
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "vp09" }, "vp09.00.10.08"_s), allowedFilters));
#endif
}

#if ENABLE(VP9)
TEST(MediaSamplesDecoder, CanDecodeVP9WithoutHardwareDecoder)
{
    if (!isVPXDecodingAvailable())
        GTEST_SKIP() << "VPx software decoding is not available";

    {
        ScopedVP9HardwareDecoder noHardwareDecoder { false };
        EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "vp09" }, "vp09.00.10.08"_s), allowedFilters));
        EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "vp09" }, "vp09.02.10.10"_s, 10), allowedFilters));
        EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "vp09" }, "vp09.01.10.08"_s), allowedFilters));
        EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "vp09" }, { }), allowedFilters));
    }
    {
        ScopedVP9HardwareDecoder hardwareDecoder { true };
        EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "vp09" }, "vp09.00.10.08"_s), allowedFilters));
    }
}
#endif

#if ENABLE(AV1)
TEST(MediaSamplesDecoder, CanDecodeAV1WithoutHardwareDecoder)
{
    if (!isVPXDecodingAvailable())
        GTEST_SKIP() << "VPx software decoding is not available";

    ScopedAV1HardwareDecoder noHardwareDecoder { false };
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "av01" }, "av01.0.04M.08"_s), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "av01" }, "av01.0.04M.10"_s, 10), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "av01" }, "av01.1.04M.08"_s), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "av01" }, "av01.0.04M.08.1"_s), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeVideoInfo(FourCC { "av01" }, { }), allowedFilters));
}

TEST(MediaSamplesDecoder, DecodeAV1)
{
    if (!isVPXDecodingAvailable())
        GTEST_SKIP() << "VPx software decoding is not available";

    // The WebM parser only exposes AV1 tracks when a hardware decoder is reported.
    auto blocks = [] {
        ScopedAV1HardwareDecoder hardwareDecoder { true };
        return parseWebMVideoFile(@"test-av1", av1ContentType);
    }();
    ASSERT_FALSE(blocks.isEmpty());
    ASSERT_TRUE(blocks[0]->info());

    ScopedAV1HardwareDecoder noHardwareDecoder { false };
    ASSERT_TRUE(MediaSamplesDecoder::canDecode(*blocks[0]->info(), allowedFilters));

    Ref decoder = createDecoder(*blocks[0]->info());
    auto decoded = decodeVideoBlocks(decoder, blocks);
    ASSERT_TRUE(decoded.outputInfo);
    RefPtr info = dynamicDowncast<VideoInfo>(decoded.outputInfo);
    ASSERT_TRUE(info);
    EXPECT_EQ(info->pixelFormat(), VideoPixelFormat::I420);
    EXPECT_EQ(info->size(), FloatSize(1280, 720));

    ASSERT_EQ(decoded.frames.size(), 1u);
    EXPECT_EQ(decoded.frames[0].size(), 1280u * 720u * 3 / 2);
    EXPECT_EQ(decoded.outputPresentationTimes, decoded.inputPresentationTimes);
}
#endif

TEST(MediaSamplesDecoder, CreateRejectsHardwareDecodableVideo)
{
    Ref h264 = makeVideoInfo(FourCC { "avc1" }, "avc1.640028"_s);
    auto promise = MediaSamplesDecoder::create(h264, allowedFilters);
    auto result = settle(promise.get());
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), PlatformMediaError::NotSupportedError);
}

TEST(MediaSamplesDecoder, DecodeVP8)
{
    if (!isVPXDecodingAvailable())
        GTEST_SKIP() << "VPx software decoding is not available";

    auto blocks = parseWebMVideoFile(@"test-vp8", vp8ContentType);
    ASSERT_FALSE(blocks.isEmpty());
    ASSERT_TRUE(blocks[0]->info());
    ASSERT_TRUE(MediaSamplesDecoder::canDecode(*blocks[0]->info(), allowedFilters));

    Ref decoder = createDecoder(*blocks[0]->info());
    EXPECT_TRUE(decoder->canContinueWith(*blocks[0]->info()));

    auto decoded = decodeVideoBlocks(decoder, blocks);
    ASSERT_TRUE(decoded.outputInfo);
    RefPtr info = dynamicDowncast<VideoInfo>(decoded.outputInfo);
    ASSERT_TRUE(info);
    EXPECT_TRUE(info->isDecoded());
    EXPECT_EQ(info->pixelFormat(), VideoPixelFormat::I420);

    size_t width = info->size().width();
    size_t height = info->size().height();
    size_t frameSize = width * height + 2 * ((width + 1) / 2) * ((height + 1) / 2);
    ASSERT_EQ(decoded.frames.size(), decoded.inputPresentationTimes.size());
    for (auto& frame : decoded.frames)
        EXPECT_EQ(frame.size(), frameSize);
    EXPECT_EQ(decoded.outputPresentationTimes, decoded.inputPresentationTimes);
}

TEST(MediaSamplesDecoder, DecodeVP8SizeChangeWithoutNewTrackInfo)
{
    if (!isVPXDecodingAvailable())
        GTEST_SKIP() << "VPx software decoding is not available";

    auto blocks = parseWebMVideoFile(@"400x300-red-resize-200x150-green", vp8ContentType);
    ASSERT_FALSE(blocks.isEmpty());
    RefPtr firstInfo = blocks[0]->info();
    ASSERT_TRUE(firstInfo);

    // Simulates a container that does not report the in-band size change.
    for (auto& block : blocks)
        block->setInfo(RefPtr { firstInfo });

    Ref decoder = createDecoder(*firstInfo);
    auto decoded = decodeVideoBlocks(decoder, blocks);
    ASSERT_EQ(decoded.frames.size(), decoded.inputPresentationTimes.size());

    for (size_t i = 0; i < decoded.frames.size(); ++i) {
        size_t width = decoded.outputSizes[i].width();
        size_t height = decoded.outputSizes[i].height();
        EXPECT_EQ(decoded.frames[i].size(), width * height + 2 * ((width + 1) / 2) * ((height + 1) / 2));
    }
    EXPECT_TRUE(decoded.outputSizes.contains(FloatSize { 400, 300 }));
    EXPECT_TRUE(decoded.outputSizes.contains(FloatSize { 200, 150 }));
}

TEST(MediaSamplesDecoder, DecodeVP8IntoSharedMemory)
{
    if (!isVPXDecodingAvailable())
        GTEST_SKIP() << "VPx software decoding is not available";

    auto sharedBlocks = parseWebMVideoFile(@"test-vp8", vp8ContentType);
    auto blocks = parseWebMVideoFile(@"test-vp8", vp8ContentType);
    ASSERT_FALSE(blocks.isEmpty());
    ASSERT_EQ(blocks.size(), sharedBlocks.size());

    auto sharedPromise = MediaSamplesDecoder::create(*sharedBlocks[0]->info(), allowedFilters | MediaSamplesDecoder::Filter::UseSharedMemory);
    auto sharedResult = settle(sharedPromise.get());
    ASSERT_TRUE(sharedResult);
    Ref sharedDecoder = WTF::move(*sharedResult);

    Ref decoder = createDecoder(*blocks[0]->info());

    auto shared = decodeVideoBlocks(sharedDecoder, sharedBlocks);
    auto copied = decodeVideoBlocks(decoder, blocks);
    EXPECT_FALSE(copied.frames.isEmpty());
    EXPECT_EQ(shared.frames, copied.frames);
}

TEST(MediaSamplesDecoder, VideoBlockReturnsOnlyItsLastFrame)
{
    if (!isVPXDecodingAvailable())
        GTEST_SKIP() << "VPx software decoding is not available";

    auto blocks = parseWebMVideoFile(@"test-vp8", vp8ContentType);
    ASSERT_FALSE(blocks.isEmpty());
    ASSERT_TRUE(blocks[0]->isSync());
    RefPtr info = blocks[0]->info();
    ASSERT_TRUE(info);

    // Two frames sharing a block, as the WebM parser groups frames with the same timestamp.
    auto first = blocks[0]->first();
    auto second = first;
    second.presentationTime = first.presentationTime + MediaTime(1, 1000000);
    second.decodeTime = second.presentationTime;
    MediaTime lastTime = second.presentationTime;
    MediaSamplesBlock::SamplesVector items;
    items.append(WTF::move(first));
    items.append(WTF::move(second));

    Ref decoder = createDecoder(*info);
    auto promise = decoder->decode(MediaSamplesBlock { info.get(), WTF::move(items) });
    auto result = settle(promise.get());
    ASSERT_TRUE(result);
    ASSERT_EQ(result->size(), 1u);
    ASSERT_EQ(result->first().size(), 1u);
    EXPECT_EQ(result->first().first().presentationTime, lastTime);
}

TEST(MediaSamplesDecoder, VideoFlushRequiresSyncSample)
{
    if (!isVPXDecodingAvailable())
        GTEST_SKIP() << "VPx software decoding is not available";

    auto blocks = parseWebMVideoFile(@"test-vp8", vp8ContentType);
    ASSERT_GE(blocks.size(), 2u);
    ASSERT_TRUE(blocks[0]->isSync());
    ASSERT_FALSE(blocks[1]->isSync());

    Ref decoder = createDecoder(*blocks[0]->info());

    auto decodePromise = decoder->decode(WTF::move(blocks[0].get()));
    EXPECT_TRUE(settle(decodePromise.get()));

    auto flushPromise = decoder->flush();
    EXPECT_TRUE(settle(flushPromise.get()));

    auto drainPromise = decoder->drain();
    auto drainResult = settle(drainPromise.get());
    ASSERT_TRUE(drainResult);
    EXPECT_TRUE(drainResult->isEmpty());

    auto deltaPromise = decoder->decode(WTF::move(blocks[1].get()));
    auto deltaResult = settle(deltaPromise.get());
    ASSERT_FALSE(deltaResult);
    EXPECT_EQ(deltaResult.error(), PlatformMediaError::InvalidState);
}

TEST(MediaSamplesDecoder, CanContinueWithRejectsChangedVideoSize)
{
    if (!isVPXDecodingAvailable())
        GTEST_SKIP() << "VPx software decoding is not available";

    auto blocks = parseWebMVideoFile(@"test-vp8", vp8ContentType);
    ASSERT_FALSE(blocks.isEmpty());

    Ref decoder = createDecoder(*blocks[0]->info());

    auto& original = downcast<VideoInfo>(*blocks[0]->info());
    EXPECT_TRUE(decoder->canContinueWith(original));

    Ref differentSize = VideoInfo::create({
        {
            .codecName = original.codecName(),
            .codecString = original.codecString()
        }, {
            .size = original.size() / 2,
            .displaySize = original.displaySize() / 2,
            .bitDepth = original.bitDepth(),
            .colorSpace = original.colorSpace(),
            .extensionAtoms = original.extensionAtoms()
        }
    });
    EXPECT_FALSE(decoder->canContinueWith(differentSize));

    Ref differentDisplaySize = VideoInfo::create({
        {
            .codecName = original.codecName(),
            .codecString = original.codecString()
        }, {
            .size = original.size(),
            .displaySize = original.displaySize() / 2,
            .bitDepth = original.bitDepth(),
            .colorSpace = original.colorSpace(),
            .extensionAtoms = original.extensionAtoms()
        }
    });
    EXPECT_FALSE(decoder->canContinueWith(differentDisplaySize));
    EXPECT_FALSE(decoder->canContinueWith(makeAudioInfo(FourCC { "opus" })));
}

static constexpr double videoDecodeWindow = 0.166;
static constexpr double settleTime = 0.3;

// The parser can hand back one block holding many samples; one sample per block makes decoding progress observable.
static Vector<UniqueRef<MediaSamplesBlock>> splitIntoSingleItemBlocks(Vector<UniqueRef<MediaSamplesBlock>>&& blocks)
{
    Vector<UniqueRef<MediaSamplesBlock>> result;
    for (auto& block : blocks) {
        const TrackInfo* info = block->info();
        auto items = block->takeSamples();
        for (auto& item : items) {
            MediaSamplesBlock::SamplesVector single;
            single.append(WTF::move(item));
            result.append(makeUniqueRef<MediaSamplesBlock>(info, WTF::move(single)));
        }
    }
    return result;
}

static Vector<UniqueRef<MediaSamplesBlock>> groupIntoBlocks(Vector<UniqueRef<MediaSamplesBlock>>&& blocks, size_t itemsPerBlock)
{
    Vector<UniqueRef<MediaSamplesBlock>> result;
    for (auto& block : blocks) {
        if (result.isEmpty() || result.last()->size() >= itemsPerBlock)
            result.append(makeUniqueRef<MediaSamplesBlock>(block->info(), MediaSamplesBlock::SamplesVector { }));
        result.last()->append(WTF::move(block.get()));
    }
    return result;
}

static bool waitUntil(const Function<bool()>& condition)
{
    auto deadline = MonotonicTime::now() + 10_s;
    while (!condition() && MonotonicTime::now() < deadline) {
        Util::spinRunLoop();
        usleep(1000);
    }
    return condition();
}

static void sleepWhilePumping(double seconds)
{
    auto deadline = MonotonicTime::now() + Seconds { seconds };
    while (MonotonicTime::now() < deadline) {
        Util::spinRunLoop();
        usleep(1000);
    }
}

class TestClock : public ThreadSafeRefCounted<TestClock> {
public:
    static Ref<TestClock> create() { return adoptRef(*new TestClock); }

    PacedMediaSamplesDecoder::PlaybackPosition position() const
    {
        Locker locker { m_lock };
        return { currentTime(), m_rate };
    }

    void set(const MediaTime& time)
    {
        Locker locker { m_lock };
        m_time = time;
        m_anchor = MonotonicTime::now();
    }

    // The time advances with the wall clock at this rate from the moment it is set.
    void setRate(double rate)
    {
        Locker locker { m_lock };
        m_time = currentTime();
        m_anchor = MonotonicTime::now();
        m_rate = rate;
    }

private:
    TestClock() = default;

    MediaTime currentTime() const WTF_REQUIRES_LOCK(m_lock)
    {
        return m_time + MediaTime::createWithDouble(m_rate * (MonotonicTime::now() - m_anchor).seconds());
    }

    mutable Lock m_lock;
    MediaTime m_time WTF_GUARDED_BY_LOCK(m_lock) { MediaTime::zeroTime() };
    MonotonicTime m_anchor WTF_GUARDED_BY_LOCK(m_lock) { MonotonicTime::now() };
    double m_rate WTF_GUARDED_BY_LOCK(m_lock) { 0 };
};

static Ref<PacedMediaSamplesDecoder> createPaced(const TrackInfo& info, TestClock& clock)
{
    auto promise = PacedMediaSamplesDecoder::create(info, allowedFilters, RunLoop::mainSingleton(), [clock = Ref { clock }] {
        return clock->position();
    });
    auto result = settle(promise.get());
    RELEASE_ASSERT(result);
    return WTF::move(*result);
}

struct Collected {
    size_t settled { 0 };
    size_t rejected { 0 };
    size_t items { 0 };
    size_t frames { 0 };
    uint32_t sampleRate { 0 };
    Vector<PlatformMediaError> errors;
    MediaTime lastOutputTime { MediaTime::invalidTime() };
};

static void collect(Ref<PacedMediaSamplesDecoder::DecodePromise>&& promise, const std::shared_ptr<Collected>& collected)
{
    promise->whenSettled(RunLoop::mainSingleton(), [collected](PacedMediaSamplesDecoder::DecodePromise::Result&& result) {
        ++collected->settled;
        if (!result) {
            ++collected->rejected;
            collected->errors.append(result.error());
            return;
        }
        for (auto& output : *result) {
            for (auto& item : output) {
                ++collected->items;
                if (collected->sampleRate)
                    collected->frames += static_cast<size_t>(std::llround(item.duration.toDouble() * collected->sampleRate));
                if (!collected->lastOutputTime.isValid() || item.presentationTime > collected->lastOutputTime)
                    collected->lastOutputTime = item.presentationTime;
            }
        }
    });
}

static double lastOutputSeconds(const Collected& collected)
{
    return collected.lastOutputTime.isValid() ? collected.lastOutputTime.toDouble() : -1;
}

TEST(PacedMediaSamplesDecoder, CreateRejectsUnsupportedTrack)
{
    Ref clock = TestClock::create();
    auto promise = PacedMediaSamplesDecoder::create(makeVideoInfo(FourCC { "avc1" }, "avc1.64001f"_s), allowedFilters, RunLoop::mainSingleton(), [clock] {
        return clock->position();
    });
    auto result = settle(promise.get());
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), PlatformMediaError::NotSupportedError);
}

TEST(PacedMediaSamplesDecoder, AudioIsDecodedOneSecondAhead)
{
    auto blocks = splitIntoSingleItemBlocks(parseWebMAudioFile(@"test-mse-audio"));
    ASSERT_FALSE(blocks.isEmpty());
    ASSERT_TRUE(blocks[0]->info());
    double endSeconds = blocks.last()->presentationEndTime().toDouble();
    ASSERT_GT(endSeconds, 1.5);

    Ref clock = TestClock::create();
    Ref decoder = createPaced(*blocks[0]->info(), clock);
    EXPECT_TRUE(decoder->canContinueWith(*blocks[0]->info()));

    auto collected = std::make_shared<Collected>();
    collected->sampleRate = 48000;
    size_t blockCount = blocks.size();
    for (auto& block : blocks)
        collect(decoder->decode(WTF::move(block.get())), collected);

    ASSERT_TRUE(waitUntil([&] {
        return collected->settled > 0;
    }));
    sleepWhilePumping(settleTime);
    EXPECT_LT(collected->settled, blockCount);
    EXPECT_LT(lastOutputSeconds(*collected), 1.1);

    clock->set(MediaTime(6, 10));
    decoder->currentTimeDidChange();
    ASSERT_TRUE(waitUntil([&] {
        return lastOutputSeconds(*collected) >= 1.5;
    }));
    sleepWhilePumping(settleTime);
    EXPECT_LT(lastOutputSeconds(*collected), 1.7);
    EXPECT_LT(collected->settled, blockCount);

    clock->set(MediaTime::createWithDouble(endSeconds + 1));
    decoder->currentTimeDidChange();
    collect(decoder->drain(), collected);
    ASSERT_TRUE(waitUntil([&] {
        return collected->settled == blockCount + 1;
    }));
    EXPECT_EQ(collected->rejected, 0u);
    EXPECT_EQ(collected->frames, decodedFrameCount(parseWebMAudioFile(@"test-mse-audio")));
}

TEST(PacedMediaSamplesDecoder, AudioBlocksAreDecodedWholeUntilOneSecondIsDecoded)
{
    auto blocks = groupIntoBlocks(splitIntoSingleItemBlocks(parseWebMAudioFile(@"test-mse-audio")), 30);
    ASSERT_GE(blocks.size(), 4u);
    ASSERT_TRUE(blocks[0]->info());
    ASSERT_LT(blocks[1]->presentationTime().toDouble(), 0.9);
    ASSERT_GT(blocks[2]->presentationTime().toDouble(), 1.1);
    ASSERT_GT(blocks[3]->presentationTime().toDouble(), 1.6);
    MediaTime secondBlockEndTime = blocks[1]->presentationEndTime();

    Ref clock = TestClock::create();
    Ref decoder = createPaced(*blocks[0]->info(), clock);

    auto collected = std::make_shared<Collected>();
    for (auto& block : blocks)
        collect(decoder->decode(WTF::move(block.get())), collected);

    // The second block takes the decoded audio past one second ahead, so the third waits.
    ASSERT_TRUE(waitUntil([&] {
        return collected->settled >= 2;
    }));
    sleepWhilePumping(settleTime);
    EXPECT_EQ(collected->settled, 2u);

    clock->set(secondBlockEndTime - MediaTime(1, 2) + MediaTime(1, 100));
    decoder->currentTimeDidChange();
    ASSERT_TRUE(waitUntil([&] {
        return collected->settled >= 3;
    }));
    sleepWhilePumping(settleTime);
    EXPECT_EQ(collected->settled, 3u);
    EXPECT_EQ(collected->rejected, 0u);
}

TEST(PacedMediaSamplesDecoder, AudioWakesUpAsTheClockAdvances)
{
    auto blocks = groupIntoBlocks(splitIntoSingleItemBlocks(parseWebMAudioFile(@"test-mse-audio")), 30);
    ASSERT_GE(blocks.size(), 4u);
    ASSERT_TRUE(blocks[0]->info());
    ASSERT_GT(blocks[2]->presentationTime().toDouble(), 1.1);
    ASSERT_GT(blocks[3]->presentationTime().toDouble(), 1.6);

    Ref clock = TestClock::create();
    Ref decoder = createPaced(*blocks[0]->info(), clock);

    auto collected = std::make_shared<Collected>();
    for (auto& block : blocks)
        collect(decoder->decode(WTF::move(block.get())), collected);

    ASSERT_TRUE(waitUntil([&] {
        return collected->settled >= 2;
    }));
    sleepWhilePumping(settleTime);
    EXPECT_EQ(collected->settled, 2u);

    // With the clock running, no further notification is needed: the decoder wakes when each block comes into the window.
    clock->setRate(1);
    decoder->currentTimeDidChange();
    ASSERT_TRUE(waitUntil([&] {
        return collected->settled >= 3;
    }));
    ASSERT_TRUE(waitUntil([&] {
        return collected->settled >= 4;
    }));
    EXPECT_EQ(collected->rejected, 0u);
}

TEST(PacedMediaSamplesDecoder, DrainWaitsForPendingInput)
{
    auto blocks = splitIntoSingleItemBlocks(parseWebMAudioFile(@"test-mse-audio"));
    ASSERT_FALSE(blocks.isEmpty());
    double endSeconds = blocks.last()->presentationEndTime().toDouble();
    ASSERT_GT(endSeconds, 1.5);

    Ref clock = TestClock::create();
    Ref decoder = createPaced(*blocks[0]->info(), clock);

    auto collected = std::make_shared<Collected>();
    for (auto& block : blocks)
        collect(decoder->decode(WTF::move(block.get())), collected);
    auto drained = std::make_shared<Collected>();
    collect(decoder->drain(), drained);

    sleepWhilePumping(settleTime);
    EXPECT_EQ(drained->settled, 0u);

    clock->set(MediaTime::createWithDouble(endSeconds + 1));
    decoder->currentTimeDidChange();
    ASSERT_TRUE(waitUntil([&] {
        return drained->settled == 1;
    }));
    EXPECT_EQ(drained->rejected, 0u);
    EXPECT_EQ(collected->settled, blocks.size());
    EXPECT_EQ(collected->rejected, 0u);
}

TEST(PacedMediaSamplesDecoder, VideoIsDecodedWithinTheDecodeWindow)
{
    if (!isVPXDecodingAvailable())
        GTEST_SKIP() << "VPx software decoding is not available";

    auto blocks = parseWebMVideoFile(@"test-vp8", vp8ContentType);
    ASSERT_GT(blocks.size(), 10u);
    ASSERT_TRUE(blocks[0]->info());

    MediaTime firstTime = blocks[0]->presentationTime();
    MediaTime lastTime = blocks.last()->presentationTime();
    double frameDuration = (blocks[1]->presentationTime() - firstTime).toDouble();
    double span = (lastTime - firstTime).toDouble();
    ASSERT_GT(span, 4 * videoDecodeWindow);

    Ref clock = TestClock::create();
    clock->set(firstTime);
    Ref decoder = createPaced(*blocks[0]->info(), clock);

    auto collected = std::make_shared<Collected>();
    size_t blockCount = blocks.size();
    for (auto& block : blocks)
        collect(decoder->decode(WTF::move(block.get())), collected);

    ASSERT_TRUE(waitUntil([&] {
        return collected->settled > 0;
    }));
    sleepWhilePumping(settleTime);
    EXPECT_LT(collected->settled, blockCount);
    EXPECT_LE(lastOutputSeconds(*collected), firstTime.toDouble() + videoDecodeWindow + frameDuration + 0.001);

    MediaTime middle = firstTime + MediaTime::createWithDouble(span / 2);
    clock->set(middle);
    decoder->currentTimeDidChange();
    ASSERT_TRUE(waitUntil([&] {
        return lastOutputSeconds(*collected) >= middle.toDouble();
    }));
    sleepWhilePumping(settleTime);
    EXPECT_LT(collected->settled, blockCount);
    EXPECT_LE(lastOutputSeconds(*collected), middle.toDouble() + videoDecodeWindow + frameDuration + 0.001);

    clock->set(lastTime + MediaTime(1, 1));
    decoder->currentTimeDidChange();
    ASSERT_TRUE(waitUntil([&] {
        return collected->settled == blockCount;
    }));
    EXPECT_EQ(collected->rejected, 0u);
    EXPECT_EQ(collected->items, blockCount);
}

TEST(PacedMediaSamplesDecoder, VideoFlushCancelsPendingInputAndRequiresSyncSample)
{
    if (!isVPXDecodingAvailable())
        GTEST_SKIP() << "VPx software decoding is not available";

    auto blocks = parseWebMVideoFile(@"test-vp8", vp8ContentType);
    ASSERT_GT(blocks.size(), 10u);
    auto more = parseWebMVideoFile(@"test-vp8", vp8ContentType);
    ASSERT_GE(more.size(), 2u);
    ASSERT_FALSE(more[1]->isSync());

    Ref clock = TestClock::create();
    clock->set(blocks[0]->presentationTime());
    Ref decoder = createPaced(*blocks[0]->info(), clock);

    auto collected = std::make_shared<Collected>();
    size_t blockCount = blocks.size();
    for (auto& block : blocks)
        collect(decoder->decode(WTF::move(block.get())), collected);

    ASSERT_TRUE(waitUntil([&] {
        return collected->settled > 0;
    }));
    sleepWhilePumping(settleTime);
    ASSERT_LT(collected->settled, blockCount);

    auto flushPromise = decoder->flush();
    EXPECT_TRUE(settle(flushPromise.get()));
    ASSERT_TRUE(waitUntil([&] {
        return collected->settled == blockCount;
    }));
    EXPECT_GT(collected->rejected, 0u);
    for (auto error : collected->errors)
        EXPECT_EQ(error, PlatformMediaError::Cancelled);

    auto deltaPromise = decoder->decode(WTF::move(more[1].get()));
    auto deltaResult = settle(deltaPromise.get());
    ASSERT_FALSE(deltaResult);
    EXPECT_EQ(deltaResult.error(), PlatformMediaError::InvalidState);

    // The error is fatal: everything after it is rejected with it.
    ASSERT_TRUE(more[0]->isSync());
    auto syncPromise = decoder->decode(WTF::move(more[0].get()));
    auto syncResult = settle(syncPromise.get());
    ASSERT_FALSE(syncResult);
    EXPECT_EQ(syncResult.error(), PlatformMediaError::InvalidState);

    auto drainPromise = decoder->drain();
    auto drainResult = settle(drainPromise.get());
    ASSERT_FALSE(drainResult);
    EXPECT_EQ(drainResult.error(), PlatformMediaError::InvalidState);
}

TEST(PacedMediaSamplesDecoder, VideoReadinessFollowsTheQueuedBlockCount)
{
    if (!isVPXDecodingAvailable())
        GTEST_SKIP() << "VPx software decoding is not available";

    auto blocks = parseWebMVideoFile(@"test-vp8", vp8ContentType);
    ASSERT_GT(blocks.size(), 45u);
    MediaTime lastTime = blocks.last()->presentationTime();

    Ref clock = TestClock::create();
    clock->set(blocks[0]->presentationTime());
    Ref decoder = createPaced(*blocks[0]->info(), clock);
    EXPECT_TRUE(decoder->isReadyForMoreMediaData());

    // The first block goes straight to the decoder; readiness turns false once 31 are waiting.
    auto collected = std::make_shared<Collected>();
    size_t blockCount = blocks.size();
    size_t fed = 0;
    while (fed < blockCount && decoder->isReadyForMoreMediaData())
        collect(decoder->decode(WTF::move(blocks[fed++].get())), collected);
    EXPECT_FALSE(decoder->isReadyForMoreMediaData());
    EXPECT_EQ(fed, 32u);

    // Blocks given while not ready are queued, not decoded.
    while (fed < blockCount)
        collect(decoder->decode(WTF::move(blocks[fed++].get())), collected);
    sleepWhilePumping(settleTime);
    EXPECT_FALSE(decoder->isReadyForMoreMediaData());
    EXPECT_LT(collected->settled, blockCount);

    auto stoppedPromise = decoder->requestMediaDataWhenReady();
    decoder->stopRequestingMediaData();
    EXPECT_FALSE(settle(stoppedPromise.get()));

    auto requestPromise = decoder->requestMediaDataWhenReady();
    clock->set(lastTime + MediaTime(1, 1));
    decoder->currentTimeDidChange();
    EXPECT_TRUE(settle(requestPromise.get()));
    EXPECT_TRUE(decoder->isReadyForMoreMediaData());
    ASSERT_TRUE(waitUntil([&] {
        return collected->settled == blockCount;
    }));
    EXPECT_EQ(collected->rejected, 0u);
}

TEST(PacedMediaSamplesDecoder, AudioReadinessFollowsTheQueuedBlockCount)
{
    auto blocks = splitIntoSingleItemBlocks(parseWebMAudioFile(@"test-mse-audio"));
    ASSERT_GT(blocks.size(), 40u);
    ASSERT_LT(blocks[17]->presentationTime().toDouble(), 1.0);

    Ref clock = TestClock::create();
    Ref decoder = createPaced(*blocks[0]->info(), clock);
    EXPECT_TRUE(decoder->isReadyForMoreMediaData());

    auto collected = std::make_shared<Collected>();
    size_t fed = 0;
    while (fed < blocks.size() && decoder->isReadyForMoreMediaData())
        collect(decoder->decode(WTF::move(blocks[fed++].get())), collected);
    EXPECT_FALSE(decoder->isReadyForMoreMediaData());
    EXPECT_EQ(fed, 32u);

    // Decoding up to one second ahead of the stopped clock leaves 15 blocks or fewer waiting.
    auto requestPromise = decoder->requestMediaDataWhenReady();
    EXPECT_TRUE(settle(requestPromise.get()));
    EXPECT_TRUE(decoder->isReadyForMoreMediaData());
    EXPECT_LT(lastOutputSeconds(*collected), 1.1);
}

TEST(PacedMediaSamplesDecoder, VideoUnknownMinimumUpcomingTimeDecodesBeyondTheWindow)
{
    if (!isVPXDecodingAvailable())
        GTEST_SKIP() << "VPx software decoding is not available";

    auto blocks = parseWebMVideoFile(@"test-vp8", vp8ContentType);
    ASSERT_GT(blocks.size(), 10u);

    Ref clock = TestClock::create();
    clock->set(blocks[0]->presentationTime());
    Ref decoder = createPaced(*blocks[0]->info(), clock);

    auto collected = std::make_shared<Collected>();
    size_t blockCount = blocks.size();
    for (auto& block : blocks)
        collect(decoder->decode(WTF::move(block.get()), MediaTime::indefiniteTime()), collected);

    ASSERT_TRUE(waitUntil([&] {
        return collected->settled == blockCount;
    }));
    EXPECT_EQ(collected->rejected, 0u);
}

TEST(PacedMediaSamplesDecoder, VideoOutOfOrderFramesAreDecodedRegardlessOfTheWindow)
{
    if (!isVPXDecodingAvailable())
        GTEST_SKIP() << "VPx software decoding is not available";

    auto blocks = parseWebMVideoFile(@"test-vp8", vp8ContentType);
    ASSERT_GT(blocks.size(), 10u);
    MediaTime firstTime = blocks[0]->presentationTime();

    Ref clock = TestClock::create();
    clock->set(firstTime);
    Ref decoder = createPaced(*blocks[0]->info(), clock);

    // Each frame announces one presented at the start, before everything already decoded.
    auto collected = std::make_shared<Collected>();
    size_t blockCount = blocks.size();
    for (auto& block : blocks)
        collect(decoder->decode(WTF::move(block.get()), firstTime), collected);

    ASSERT_TRUE(waitUntil([&] {
        return collected->settled == blockCount;
    }));
    EXPECT_EQ(collected->rejected, 0u);
}

TEST(PacedMediaSamplesDecoder, VideoBlockIsDecodedWhole)
{
    if (!isVPXDecodingAvailable())
        GTEST_SKIP() << "VPx software decoding is not available";

    auto blocks = parseWebMVideoFile(@"test-vp8", vp8ContentType);
    ASSERT_FALSE(blocks.isEmpty());
    ASSERT_TRUE(blocks[0]->isSync());
    RefPtr info = blocks[0]->info();
    ASSERT_TRUE(info);

    // Two frames sharing a block, as the WebM parser groups frames with the same timestamp.
    auto first = blocks[0]->first();
    auto second = first;
    second.presentationTime = first.presentationTime + MediaTime(1, 1000000);
    second.decodeTime = second.presentationTime;
    MediaTime lastTime = second.presentationTime;
    MediaSamplesBlock::SamplesVector items;
    items.append(WTF::move(first));
    items.append(WTF::move(second));

    Ref clock = TestClock::create();
    clock->set(blocks[0]->presentationTime());
    Ref decoder = createPaced(*info, clock);
    auto promise = decoder->decode(MediaSamplesBlock { info.get(), WTF::move(items) });
    auto result = settle(promise.get());
    ASSERT_TRUE(result);
    ASSERT_EQ(result->size(), 1u);
    ASSERT_EQ(result->first().size(), 1u);
    EXPECT_EQ(result->first().first().presentationTime, lastTime);
}

TEST(PacedMediaSamplesDecoder, DecodeRejectsChangedFormat)
{
    auto blocks = parseWebMAudioFile(@"test-mse-audio");
    ASSERT_FALSE(blocks.isEmpty());
    ASSERT_TRUE(blocks[0]->info());

    Ref clock = TestClock::create();
    Ref decoder = createPaced(*blocks[0]->info(), clock);

    Ref otherInfo = AudioInfo::create({
        {
            .codecName = FourCC { "opus" }
        }, {
            .rate = 44100,
            .channels = 1
        }
    });
    EXPECT_FALSE(decoder->canContinueWith(otherInfo.get()));

    MediaSamplesBlock::SamplesVector items;
    items.append({
        .presentationTime = MediaTime::zeroTime(),
        .duration = MediaTime(1, 50),
        .data = SharedBuffer::create(Vector<uint8_t> { 1, 2, 3 }),
        .flags = MediaSample::IsSync
    });
    auto promise = decoder->decode(MediaSamplesBlock { otherInfo.ptr(), WTF::move(items) });
    auto result = settle(promise.get());
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), PlatformMediaError::InvalidState);
}

} // namespace TestWebKitAPI

#endif // USE(AVFOUNDATION) && ENABLE(MEDIA_SOURCE)
