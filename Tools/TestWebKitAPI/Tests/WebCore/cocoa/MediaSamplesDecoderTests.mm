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
#import "Helpers/cocoa/TestNSBundleExtras.h"
#import <AVFoundation/AVFoundation.h>
#import <WebCore/AudioSampleFormat.h>
#import <WebCore/CMUtilities.h>
#import <WebCore/ContentType.h>
#import <WebCore/MediaSampleAVFObjC.h>
#import <WebCore/MediaSamplesBlock.h>
#import <WebCore/MediaSamplesDecoder.h>
#import <WebCore/MediaSourceConfiguration.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/SourceBufferParser.h>
#import <WebCore/TrackInfo.h>
#import <cmath>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
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

// ES_Descriptor wrapping the AudioSpecificConfig { 0x12, 0x10 } (AAC-LC, 44.1 kHz, stereo).
static Vector<uint8_t> wrappedAACCookie()
{
    return {
        0x03, 0x19, 0x00, 0x01, 0x00,
        0x04, 0x11, 0x40, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x05, 0x02, 0x12, 0x10,
        0x06, 0x01, 0x02
    };
}

TEST(MediaSamplesDecoder, CanDecodeFilters)
{
    Ref opus = makeAudioInfo(FourCC { "opus" });

    EXPECT_TRUE(MediaSamplesDecoder::canDecode(opus, allowedFilters));
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(opus, allowedFilters | MediaSamplesDecoder::Filter::UseSharedMemory));
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(opus, allowedFilters | MediaSamplesDecoder::Filter::HardwareAllowed));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(opus, { MediaSamplesDecoder::Filter::InProcess }));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(opus, { MediaSamplesDecoder::Filter::SoftwareAllowed }));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(opus, { }));
}

TEST(MediaSamplesDecoder, CanDecodeSupportedCodecs)
{
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "vorb" }), allowedFilters));
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "flac" }), allowedFilters));
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { ".mp3" }, { }), allowedFilters));
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "alaw" }, { }, 8), allowedFilters));
    EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "ulaw" }, { }, 8), allowedFilters));
    for (uint8_t bitDepth : { 8, 16, 24, 32 })
        EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "lpcm" }, { }, bitDepth), allowedFilters));

    for (auto codec : { FourCC { "aac " }, FourCC { "aach" }, FourCC { "aacp" } }) {
        EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeAudioInfo(codec, { }), allowedFilters));
        EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeAudioInfo(codec, { 0x12, 0x10 }), allowedFilters));
        EXPECT_TRUE(MediaSamplesDecoder::canDecode(makeAudioInfo(codec, wrappedAACCookie()), allowedFilters));
    }
}

TEST(MediaSamplesDecoder, CanDecodeRejectsUnsupportedTracks)
{
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "opus" }, { }), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "opus" }, { 1, 2, 3, 4 }, 16, 6), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "opus" }, { 1, 2, 3, 4 }, 16, 2, AudioSampleFormat::F32), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "vorb" }, { }), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "flac" }, { }), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "lpcm" }, { }, 12), allowedFilters));
    EXPECT_FALSE(MediaSamplesDecoder::canDecode(makeAudioInfo(FourCC { "aacl" }, { }), allowedFilters));
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

} // namespace TestWebKitAPI

#endif // USE(AVFOUNDATION) && ENABLE(MEDIA_SOURCE)
