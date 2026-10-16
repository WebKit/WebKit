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

#include "config.h"
#include "MediaSamplesDecoder.h"

#include "AudioDecoder.h"
#include "AudioSampleFormat.h"
#include "AudioUtilities.h"
#include "PlatformRawAudioData.h"
#include "SharedBuffer.h"
#include "SharedMemory.h"
#include "TrackInfo.h"
#include "VP9Utilities.h"
#include "VideoDecoder.h"
#include "VideoFrame.h"
#include <wtf/PointerComparison.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA) && ENABLE(AV1)
#include "AV1UtilitiesCocoa.h"
#endif

#if PLATFORM(COCOA) && ENABLE(VP9)
#include "VP9UtilitiesCocoa.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaSamplesDecoder);

static constexpr uint32_t microsecondsTimeScale = 1000000;

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
        return AudioDecoderConfiguration { WTF::move(codec), { .description = WTF::move(description), .sampleRate = info.rate(), .numberOfChannels = info.channels() } };
    };

    if (codecName == FourCC { "opus" }) {
        if (cookie.empty() || info.channels() > 2)
            return { };
        // The pre-skip is applied through the samples' trimInterval, so the decoder must not apply it too.
        return configuration("opus"_s, AudioUtilities::opusHeadWithoutPreSkip(cookie));
    }
    if (codecName == FourCC { "vorb" }) {
        if (cookie.empty())
            return { };
        return configuration("vorbis"_s, Vector<uint8_t>(cookie));
    }
    if (codecName == FourCC { "flac" }) {
        if (cookie.empty())
            return { };
        return configuration("flac"_s, Vector<uint8_t>(cookie));
    }
    if (codecName == FourCC { ".mp3" })
        return configuration("mp3"_s);
    if (codecName == FourCC { "alaw" })
        return configuration("alaw"_s);
    if (codecName == FourCC { "ulaw" })
        return configuration("ulaw"_s);

    // FIXME: AAC-LD and AAC-ELD ('aacl', 'aace') are not accepted by AudioDecoder::isCodecSupported.
    String aacCodec;
    if (codecName == FourCC { "aac " })
        aacCodec = "mp4a.40.2"_s;
    else if (codecName == FourCC { "aach" })
        aacCodec = "mp4a.40.5"_s;
    else if (codecName == FourCC { "aacp" })
        aacCodec = "mp4a.40.29"_s;
    if (!aacCodec.isNull()) {
        auto audioSpecificConfig = AudioUtilities::audioSpecificConfigFromAACCookie(cookie);
        if (!audioSpecificConfig)
            return { };
        return configuration(WTF::move(aacCodec), WTF::move(*audioSpecificConfig));
    }

    // FIXME: only 32-bit floating point is recognised for 32-bit PCM (the only producer is the WebM parser), and PCM is assumed little-endian.
    if (codecName == FourCC { "lpcm" }) {
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

static bool isProtected(const TrackInfo& info)
{
#if ENABLE(ENCRYPTED_MEDIA)
    return !!info.encryptionDataCollection();
#else
    UNUSED_PARAM(info);
    return false;
#endif
}

// FIXME: only in-process software decoding is supported.
// The GPU process, hardware decoding and non-Cocoa platforms are not supported yet.
static bool canUseInProcessDecoder(const TrackInfo& info, MediaSamplesDecoder::Filters filters)
{
#if PLATFORM(COCOA)
    using Filter = MediaSamplesDecoder::Filter;
    return filters.containsAll({ Filter::InProcess, Filter::SoftwareAllowed }) && !isProtected(info);
#else
    UNUSED_PARAM(info);
    UNUSED_PARAM(filters);
    return false;
#endif
}

static std::optional<AudioDecoderConfiguration> decoderConfiguration(const AudioInfo& info, MediaSamplesDecoder::Filters filters)
{
    if (!canUseInProcessDecoder(info, filters) || info.isDecoded() || !info.rate() || !info.channels())
        return { };

    auto configuration = audioDecoderConfiguration(info);
    if (!configuration || !AudioDecoder::isCodecSupported(configuration->codec))
        return { };
    return configuration;
}

static RefPtr<SharedBuffer> vpcCAtom(const VideoInfo& info)
{
    for (auto& [type, data] : info.extensionAtoms()) {
        if (type == FourCC { "vpcC" })
            return RefPtr<SharedBuffer> { data.ptr() };
    }
    return nullptr;
}

#if ENABLE(VP9)
static std::optional<uint8_t> vp9Profile(const VideoInfo& info)
{
    if (RefPtr atom = vpcCAtom(info)) {
        if (auto record = createVPCodecConfigurationRecordFromVPCC(atom->span()))
            return record->profile;
    }
    if (auto record = parseVPCodecParametersIgnoringColorFields(info.codecString()))
        return record->profile;
    return { };
}
#endif

// FIXME: ask the GPU process which codecs it decodes in hardware instead of using a static list.
static bool isHardwareDecodable(const VideoInfo& info)
{
    auto codecName = info.codecName();
    if (codecName == FourCC { "avc1" } || codecName == FourCC { "avc3" })
        return true;
#if PLATFORM(COCOA) && ENABLE(VP9)
    if (codecName == FourCC { "vp09" })
        return vp9Profile(info) == 0u && vp9HardwareDecoderAvailable();
#endif
#if PLATFORM(COCOA) && ENABLE(AV1)
    if (codecName == FourCC { "av01" })
        return av1HardwareDecoderAvailable();
#endif
    return false;
}

struct VideoDecoderConfiguration {
    String codec;
    VideoDecoder::Config config;
};

static std::optional<VideoDecoderConfiguration> videoDecoderConfiguration(const VideoInfo& info)
{
#if ENABLE(VP9)
    auto codecName = info.codecName();
    auto configuration = [&](String&& codec) {
        RefPtr atom = vpcCAtom(info);
        return VideoDecoderConfiguration { WTF::move(codec), {
            .description = atom ? Vector<uint8_t>(atom->span()) : Vector<uint8_t> { },
            .colorSpace = info.colorSpace(),
            .noOutputAsError = VideoDecoder::TreatNoOutputAsError::No
        } };
    };

    if (codecName == FourCC { "vp08" })
        return configuration("vp8"_s);
    if (codecName == FourCC { "vp09" } && vp9Profile(info) == 0u)
        return configuration("vp09.00"_s);
#else
    UNUSED_PARAM(info);
#endif

    // FIXME: AV1 (the software decoder only handles 8-bit content).
    return { };
}

static std::optional<VideoDecoderConfiguration> decoderConfiguration(const VideoInfo& info, MediaSamplesDecoder::Filters filters)
{
    if (!canUseInProcessDecoder(info, filters))
        return { };

    // FIXME: bit depths above 8 need VideoFrame::copyTo to produce 10 and 12-bit planar output.
    if (info.isDecoded() || info.size().isEmpty() || info.bitDepth() != 8)
        return { };

    auto configuration = videoDecoderConfiguration(info);
    if (!configuration || !VideoDecoder::isVPXSupported() || isHardwareDecodable(info))
        return { };
    return configuration;
}

static Ref<VideoInfo> outputVideoInfo(const VideoInfo& inputInfo, FloatSize size)
{
    return VideoInfo::create({
        {
            .codecName = FourCC { "I420" },
            .trackID = inputInfo.trackID()
        }, {
            .size = size,
            .displaySize = inputInfo.displaySize(),
            .bitDepth = 8,
            .colorSpace = inputInfo.colorSpace(),
            .pixelFormat = VideoPixelFormat::I420
        }
    });
}

static std::optional<uint64_t> durationInMicroseconds(const MediaSamplesBlock::MediaSampleItem& item)
{
    if (item.duration.isValid() && item.duration > MediaTime::zeroTime())
        return static_cast<uint64_t>(item.duration.toMicroseconds());
    return std::nullopt;
}

MediaSamplesDecoder::MediaSamplesDecoder(Ref<const TrackInfo>&& inputInfo, Ref<const TrackInfo>&& outputInfo, Filters filters)
    : m_queue(WorkQueue::create("MediaSamplesDecoder queue"_s))
    , m_inputInfo(WTF::move(inputInfo))
    , m_outputInfo(WTF::move(outputInfo))
    , m_filters(filters)
{
}

MediaSamplesDecoder::~MediaSamplesDecoder() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    if (RefPtr audioDecoder = m_audioDecoder)
        audioDecoder->close();
    if (RefPtr videoDecoder = m_videoDecoder)
        videoDecoder->close();
}

bool MediaSamplesDecoder::canDecode(const TrackInfo& info, Filters filters)
{
    if (RefPtr audioInfo = dynamicDowncast<AudioInfo>(info))
        return !!decoderConfiguration(*audioInfo, filters);
    if (RefPtr videoInfo = dynamicDowncast<VideoInfo>(info))
        return !!decoderConfiguration(*videoInfo, filters);
    return false;
}

Ref<MediaSamplesDecoder::CreatePromise> MediaSamplesDecoder::create(const TrackInfo& info, Filters filters)
{
    if (RefPtr audioInfo = dynamicDowncast<AudioInfo>(info))
        return createAudio(*audioInfo, filters);
    if (RefPtr videoInfo = dynamicDowncast<VideoInfo>(info))
        return createVideo(*videoInfo, filters);
    return CreatePromise::createAndReject(isProtected(info) ? PlatformMediaError::ProtectedContent : PlatformMediaError::NotSupportedError);
}

Ref<MediaSamplesDecoder::CreatePromise> MediaSamplesDecoder::createAudio(const AudioInfo& audioInfo, Filters filters)
{
    auto configuration = decoderConfiguration(audioInfo, filters);
    if (!configuration)
        return CreatePromise::createAndReject(isProtected(audioInfo) ? PlatformMediaError::ProtectedContent : PlatformMediaError::NotSupportedError);

    Ref outputInfo = AudioInfo::create({
        {
            .codecName = FourCC { "lpcm" },
            .trackID = audioInfo.trackID()
        }, {
            .rate = audioInfo.rate(),
            .channels = audioInfo.channels(),
            .framesPerPacket = 1,
            .bitDepth = 32,
            .sampleFormat = AudioSampleFormat::F32
        }
    });

    Ref decoder = adoptRef(*new MediaSamplesDecoder(Ref<const TrackInfo> { audioInfo }, WTF::move(outputInfo), filters));

    auto outputCallback = [weakDecoder = ThreadSafeWeakPtr { decoder.get() }](std::expected<AudioDecoder::DecodedData, String>&& result) {
        RefPtr protectedDecoder = weakDecoder;
        if (!protectedDecoder)
            return;
        protectedDecoder->m_queue->dispatch([protectedDecoder, result = WTF::move(result)] mutable {
            if (!result) {
                protectedDecoder->didFailDecoding();
                return;
            }
            protectedDecoder->didDecodeAudio(WTF::move(result->data));
        });
    };

    return AudioDecoder::create(configuration->codec, configuration->config, WTF::move(outputCallback))->whenSettled(decoder->m_queue.get(), [decoder](auto&& result) -> Ref<CreatePromise> {
        if (!result)
            return CreatePromise::createAndReject(PlatformMediaError::DecoderCreationError);
        assertIsCurrent(decoder->m_queue.get());
        decoder->m_audioDecoder = WTF::move(*result);
        return CreatePromise::createAndResolve(decoder);
    });
}

Ref<MediaSamplesDecoder::CreatePromise> MediaSamplesDecoder::createVideo(const VideoInfo& videoInfo, Filters filters)
{
    auto configuration = decoderConfiguration(videoInfo, filters);
    if (!configuration)
        return CreatePromise::createAndReject(isProtected(videoInfo) ? PlatformMediaError::ProtectedContent : PlatformMediaError::NotSupportedError);

    Ref decoder = adoptRef(*new MediaSamplesDecoder(Ref<const TrackInfo> { videoInfo }, outputVideoInfo(videoInfo, videoInfo.size()), filters));

    auto outputCallback = [weakDecoder = ThreadSafeWeakPtr { decoder.get() }](std::expected<VideoDecoder::DecodedFrame, String>&& result) {
        RefPtr protectedDecoder = weakDecoder;
        if (!protectedDecoder)
            return;
        protectedDecoder->m_queue->dispatch([protectedDecoder, result = WTF::move(result)] mutable {
            if (!result) {
                protectedDecoder->didFailDecoding();
                return;
            }
            protectedDecoder->didDecodeVideo(WTF::move(result->frame), result->timestamp);
        });
    };

    CreatePromise::Producer producer;
    Ref promise = producer.promise();
    VideoDecoder::createLocalDecoder(configuration->codec, configuration->config, [decoder, producer = WTF::move(producer)](VideoDecoder::CreateResult&& result) mutable {
        if (!result) {
            producer.reject(PlatformMediaError::DecoderCreationError);
            return;
        }
        decoder->m_queue->dispatch([decoder, videoDecoder = WTF::move(*result), producer = WTF::move(producer)] mutable {
            assertIsCurrent(decoder->m_queue.get());
            decoder->m_videoDecoder = WTF::move(videoDecoder);
            producer.resolve(WTF::move(decoder));
        });
    }, WTF::move(outputCallback));
    return promise;
}

bool MediaSamplesDecoder::canContinueWith(const TrackInfo& info) const
{
    return canContinueWith(m_inputInfo, info);
}

bool MediaSamplesDecoder::canContinueWith(const TrackInfo& decoderInfo, const TrackInfo& info)
{
    if (RefPtr videoInfo = dynamicDowncast<VideoInfo>(info)) {
        RefPtr currentInfo = dynamicDowncast<VideoInfo>(decoderInfo);
        if (!currentInfo)
            return false;

        return videoInfo->codecName() == currentInfo->codecName()
            && videoInfo->codecString() == currentInfo->codecString()
            && videoInfo->size() == currentInfo->size()
            && videoInfo->displaySize() == currentInfo->displaySize()
            && videoInfo->bitDepth() == currentInfo->bitDepth()
            && videoInfo->colorSpace() == currentInfo->colorSpace()
            && arePointingToEqualData(vpcCAtom(*videoInfo), vpcCAtom(*currentInfo));
    }

    RefPtr audioInfo = dynamicDowncast<AudioInfo>(info);
    RefPtr currentInfo = dynamicDowncast<AudioInfo>(decoderInfo);
    if (!audioInfo || !currentInfo)
        return false;

    return audioInfo->codecName() == currentInfo->codecName()
        && audioInfo->codecString() == currentInfo->codecString()
        && audioInfo->rate() == currentInfo->rate()
        && audioInfo->channels() == currentInfo->channels()
        && audioInfo->bitDepth() == currentInfo->bitDepth()
        && arePointingToEqualData(audioInfo->cookieData(), currentInfo->cookieData());
}

Ref<MediaSamplesDecoder::DecodePromise> MediaSamplesDecoder::decode(MediaSamplesBlock&& block)
{
    return invokeAsync(m_queue.get(), [protectedThis = Ref { *this }, block = WTF::move(block)] mutable {
        return protectedThis->enqueueOperation<DecodePromise>([protectedThis, block = WTF::move(block)] mutable {
            return protectedThis->decodeOnQueue(WTF::move(block));
        });
    });
}

Ref<MediaSamplesDecoder::DecodePromise> MediaSamplesDecoder::drain()
{
    return invokeAsync(m_queue.get(), [protectedThis = Ref { *this }] {
        return protectedThis->enqueueOperation<DecodePromise>([protectedThis] {
            return protectedThis->drainOnQueue();
        });
    });
}

Ref<MediaPromise> MediaSamplesDecoder::flush()
{
    return invokeAsync(m_queue.get(), [protectedThis = Ref { *this }] {
        // The flush starts immediately and settles once the operations it cancels have.
        Ref promise = protectedThis->flushOnQueue();
        return protectedThis->enqueueOperation<MediaPromise>([promise = WTF::move(promise)] {
            return promise;
        });
    });
}

// Runs `operation` once all previously enqueued operations have settled, unless a flush has since cancelled it.
template<typename PromiseType>
Ref<PromiseType> MediaSamplesDecoder::enqueueOperation(Function<Ref<PromiseType>()>&& operation)
{
    assertIsCurrent(m_queue.get());

    GenericPromise::Producer completion;
    RefPtr previousOperation = std::exchange(m_lastOperation, completion.promise().ptr());
    Ref previous = previousOperation ? previousOperation.releaseNonNull() : GenericPromise::createAndResolve();
    return previous->whenSettled(m_queue.get(), [protectedThis = Ref { *this }, generation = m_generation, operation = WTF::move(operation), completion = WTF::move(completion)](auto&&) mutable -> Ref<PromiseType> {
        assertIsCurrent(protectedThis->m_queue.get());
        if (generation != protectedThis->m_generation) {
            completion.resolve();
            return PromiseType::createAndReject(PlatformMediaError::Cancelled);
        }
        return operation()->whenSettled(protectedThis->m_queue.get(), [completion = WTF::move(completion)](auto&& result) {
            completion.resolve();
            return PromiseType::createAndSettle(WTF::move(result));
        });
    });
}

Ref<MediaSamplesDecoder::DecodePromise> MediaSamplesDecoder::decodeOnQueue(MediaSamplesBlock&& block)
{
    assertIsCurrent(m_queue.get());

    if (m_error)
        return DecodePromise::createAndReject(*m_error);

    if ((!m_audioDecoder && !m_videoDecoder) || !block.info() || !canContinueWith(*protect(block.info())))
        return DecodePromise::createAndReject(PlatformMediaError::InvalidState);

    if (std::ranges::any_of(block, [](auto& item) { return !item.data; }))
        return DecodePromise::createAndReject(PlatformMediaError::InvalidState);

    if (block.isProtected())
        return DecodePromise::createAndReject(PlatformMediaError::ProtectedContent);
    if (!block.isEmpty()) {
        if (m_needsSyncSample && !block.isSync())
            return DecodePromise::createAndReject(PlatformMediaError::InvalidState);
        m_needsSyncSample = false;
    }

    if (block.discontinuity().value_or(false))
        m_discontinuity = true;

    if (RefPtr videoDecoder = m_videoDecoder) {
        int64_t firstSequence = m_nextSequence;
        Vector<Ref<VideoDecoder::DecodePromise>> promises;
        promises.reserveInitialCapacity(block.size());
        for (auto& item : block) {
            int64_t sequence = m_nextSequence++;
            auto metadata = item;
            metadata.data = nullptr;
            metadata.flags = MediaSample::IsSync | (item.flags & MediaSample::IsNonDisplaying);
            m_inFlight.add(sequence, InFlightFrame { WTF::move(metadata), firstSequence });
            promises.append(videoDecoder->decode({ protect(item.data)->makeContiguous(), item.isSync(), sequence, durationInMicroseconds(item) }));
        }

        return VideoDecoder::DecodePromise::all(promises)->whenSettled(m_queue.get(), [protectedThis = Ref { *this }, generation = m_generation, firstSequence, endSequence = m_nextSequence](auto&& result) {
            assertIsCurrent(protectedThis->m_queue.get());
            for (int64_t sequence = firstSequence; sequence < endSequence; ++sequence)
                protectedThis->m_inFlight.remove(sequence);
            return protectedThis->settleWithPendingOutput(!!result, generation);
        });
    }

    RefPtr audioDecoder = m_audioDecoder;
    Vector<Ref<AudioDecoder::DecodePromise>> promises;
    promises.reserveInitialCapacity(block.size());
    for (auto& item : block) {
        auto [trimAtStart, trimAtEnd] = item.trimInterval;
        auto startTime = item.presentationTime - trimAtStart;
        auto fullDuration = item.duration + trimAtStart + trimAtEnd;
        std::optional<uint64_t> duration;
        if (fullDuration.isValid() && fullDuration > MediaTime::zeroTime())
            duration = static_cast<uint64_t>(fullDuration.toMicroseconds());
        if (trimAtStart > MediaTime::zeroTime())
            m_trimmedRanges.append({ startTime, item.presentationTime });
        if (trimAtEnd > MediaTime::zeroTime() && duration) {
            auto endTime = item.presentationTime + item.duration;
            m_trimmedRanges.append({ endTime, endTime + trimAtEnd });
        }
        promises.append(audioDecoder->decode({ protect(item.data)->makeContiguous(), item.isSync(), startTime.toMicroseconds(), duration }));
    }

    return AudioDecoder::DecodePromise::all(promises)->whenSettled(m_queue.get(), [protectedThis = Ref { *this }, generation = m_generation](auto&& result) {
        return protectedThis->settleWithPendingOutput(!!result, generation);
    });
}

Ref<MediaSamplesDecoder::DecodePromise> MediaSamplesDecoder::drainOnQueue()
{
    assertIsCurrent(m_queue.get());

    if (m_error)
        return DecodePromise::createAndReject(*m_error);

    if (!m_audioDecoder && !m_videoDecoder)
        return DecodePromise::createAndReject(PlatformMediaError::InvalidState);

    m_needsSyncSample = true;
    return (m_videoDecoder ? protect(m_videoDecoder)->flush() : protect(m_audioDecoder)->flush())->whenSettled(m_queue.get(), [protectedThis = Ref { *this }, generation = m_generation](auto&& result) {
        assertIsCurrent(protectedThis->m_queue.get());
        if (generation == protectedThis->m_generation)
            protectedThis->m_trimmedRanges.clear();
        return protectedThis->settleWithPendingOutput(!!result, generation);
    });
}

Ref<MediaPromise> MediaSamplesDecoder::flushOnQueue()
{
    assertIsCurrent(m_queue.get());

    ++m_generation;
    m_inFlight.clear();

    if (m_error)
        return MediaPromise::createAndReject(*m_error);

    m_needsSyncSample = true;

    if (!m_audioDecoder && !m_videoDecoder)
        return MediaPromise::createAndResolve();

    return (m_videoDecoder ? protect(m_videoDecoder)->flush() : protect(m_audioDecoder)->flush())->whenSettled(m_queue.get(), [protectedThis = Ref { *this }](auto&& result) {
        return protectedThis->finishFlush(!!result);
    });
}

Ref<MediaPromise> MediaSamplesDecoder::finishFlush(bool succeeded)
{
    assertIsCurrent(m_queue.get());

    m_completedOutput.clear();
    m_lastOutputBlockSequence.reset();
    m_pendingOutput.clear();
    m_trimmedRanges.clear();
    m_discontinuity = std::nullopt;
    if (!succeeded && !m_error)
        m_error = decodingError();
    if (m_error)
        return MediaPromise::createAndReject(*m_error);
    return MediaPromise::createAndResolve();
}

Ref<MediaSamplesDecoder::DecodePromise> MediaSamplesDecoder::settleWithPendingOutput(bool succeeded, uint32_t generation)
{
    assertIsCurrent(m_queue.get());

    if (generation != m_generation)
        return DecodePromise::createAndReject(PlatformMediaError::Cancelled);

    if (!succeeded && !m_error)
        m_error = decodingError();
    if (m_error)
        return DecodePromise::createAndReject(*m_error);

    auto blocks = std::exchange(m_completedOutput, { });
    m_lastOutputBlockSequence.reset();
    if (!m_pendingOutput.isEmpty())
        blocks.append({ m_outputInfo.ptr(), std::exchange(m_pendingOutput, { }) });
    if (m_discontinuity && !blocks.isEmpty())
        blocks.first().setDiscontinuity(*std::exchange(m_discontinuity, std::nullopt));
    return DecodePromise::createAndResolve(WTF::move(blocks));
}

PlatformMediaError MediaSamplesDecoder::decodingError() const
{
    assertIsCurrent(m_queue.get());

    return m_videoDecoder ? PlatformMediaError::VideoDecodingError : PlatformMediaError::AudioDecodingError;
}

void MediaSamplesDecoder::didFailDecoding()
{
    assertIsCurrent(m_queue.get());

    if (!m_error)
        m_error = decodingError();
}

void MediaSamplesDecoder::didDecodeAudio(Ref<PlatformRawAudioData>&& data)
{
    assertIsCurrent(m_queue.get());

    size_t frames = data->numberOfFrames();
    size_t channels = data->numberOfChannels();
    uint32_t rate = data->sampleRate();
    if (!frames || !channels || !rate)
        return;

    auto startTime = MediaTime(data->timestamp(), microsecondsTimeScale);
    auto endTime = startTime + MediaTime(frames, rate);
    auto frameAt = [&](const MediaTime& time) -> size_t {
        return std::clamp<int64_t>((time - startTime).toTimeScale(rate).timeValue(), 0, static_cast<int64_t>(frames));
    };

    while (!m_trimmedRanges.isEmpty() && m_trimmedRanges.first().second <= startTime)
        m_trimmedRanges.removeFirst();

    Vector<std::pair<size_t, size_t>, 2> keptRanges;
    size_t cursor = 0;
    for (auto& [trimStart, trimEnd] : m_trimmedRanges) {
        size_t trimStartFrame = frameAt(trimStart);
        if (trimStartFrame >= frames)
            break;
        if (trimStartFrame > cursor)
            keptRanges.append({ cursor, trimStartFrame });
        cursor = std::max(cursor, frameAt(trimEnd));
    }
    if (cursor < frames)
        keptRanges.append({ cursor, frames });

    while (!m_trimmedRanges.isEmpty() && m_trimmedRanges.first().second <= endTime)
        m_trimmedRanges.removeFirst();

    for (auto [firstFrame, endFrame] : keptRanges) {
        size_t frameCount = endFrame - firstFrame;
        size_t sampleCount = frameCount * channels;
        size_t byteSize = sampleCount * sizeof(float);
        RefPtr<SharedBuffer> buffer;
        if (m_filters.contains(Filter::UseSharedMemory)) {
            RefPtr memory = SharedMemory::allocate(byteSize);
            if (!memory) {
                if (!m_error)
                    m_error = PlatformMediaError::MemoryError;
                return;
            }
            data->copyTo(memory->mutableSpan(), AudioSampleFormat::F32, 0, firstFrame, std::nullopt, sampleCount);
            buffer = memory->createSharedBuffer(byteSize);
        } else {
            Vector<uint8_t> bytes(byteSize);
            data->copyTo(bytes.mutableSpan(), AudioSampleFormat::F32, 0, firstFrame, std::nullopt, sampleCount);
            buffer = SharedBuffer::create(WTF::move(bytes));
        }

        m_pendingOutput.append({
            .presentationTime = startTime + MediaTime(firstFrame, rate),
            .duration = MediaTime(frameCount, rate),
            .data = WTF::move(buffer),
            .flags = MediaSample::IsSync
        });
    }
}

void MediaSamplesDecoder::didDecodeVideo(Ref<VideoFrame>&& frame, int64_t sequence)
{
    assertIsCurrent(m_queue.get());

    auto iterator = m_inFlight.find(sequence);
    if (iterator == m_inFlight.end())
        return;
    auto [metadata, blockSequence] = WTF::move(iterator->value);
    m_inFlight.remove(iterator);

    if (metadata.flags & MediaSample::IsNonDisplaying)
        return;

    auto size = frame->presentationSize();
    size_t width = size.width();
    size_t height = size.height();
    if (!width || !height) {
        didFailDecoding();
        return;
    }

    if (FloatSize { size } != downcast<VideoInfo>(m_outputInfo).size())
        m_outputInfo = outputVideoInfo(downcast<VideoInfo>(m_inputInfo), size);

    size_t chromaWidth = (width + 1) / 2;
    size_t chromaHeight = (height + 1) / 2;
    size_t lumaSize = width * height;
    size_t chromaSize = chromaWidth * chromaHeight;
    size_t byteSize = lumaSize + 2 * chromaSize;

    // VideoFrame::copyTo on Cocoa calls back before returning.
    auto copy = [&](std::span<uint8_t> destination) {
        Vector<ComputedPlaneLayout> layouts {
            { .destinationOffset = 0, .destinationStride = width, .sourceHeight = height, .sourceWidthBytes = width },
            { .destinationOffset = lumaSize, .destinationStride = chromaWidth, .sourceHeight = chromaHeight, .sourceWidthBytes = chromaWidth },
            { .destinationOffset = lumaSize + chromaSize, .destinationStride = chromaWidth, .sourceHeight = chromaHeight, .sourceWidthBytes = chromaWidth }
        };
        bool succeeded = false;
        frame->copyTo(destination, VideoPixelFormat::I420, ColorSpace::SRGB(), WTF::move(layouts), [&](std::optional<Vector<PlaneLayout>>&& planeLayouts) {
            succeeded = planeLayouts && !planeLayouts->isEmpty();
        });
        return succeeded;
    };

    RefPtr<SharedBuffer> buffer;
    if (m_filters.contains(Filter::UseSharedMemory)) {
        RefPtr memory = SharedMemory::allocate(byteSize);
        if (!memory) {
            if (!m_error)
                m_error = PlatformMediaError::MemoryError;
            return;
        }
        if (!copy(memory->mutableSpan())) {
            didFailDecoding();
            return;
        }
        buffer = memory->createSharedBuffer(byteSize);
    } else {
        Vector<uint8_t> bytes(byteSize);
        if (!copy(bytes.mutableSpan())) {
            didFailDecoding();
            return;
        }
        buffer = SharedBuffer::create(WTF::move(bytes));
    }

    metadata.data = WTF::move(buffer);
    MediaSamplesBlock::SamplesVector items;
    items.append(WTF::move(metadata));
    MediaSamplesBlock output { m_outputInfo.ptr(), WTF::move(items) };
    // A frame is immediately replaced by the next one decoded from the same block.
    if (m_lastOutputBlockSequence == blockSequence)
        m_completedOutput.last() = WTF::move(output);
    else
        m_completedOutput.append(WTF::move(output));
    m_lastOutputBlockSequence = blockSequence;
}

} // namespace WebCore
