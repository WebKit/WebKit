/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "MediaRecorderPrivateEncoder.h"

#if ENABLE(MEDIA_RECORDER)

#include "AudioSampleBufferConverter.h"
#include "CARingBuffer.h"
#include "CMUtilities.h"
#include "ContentType.h"
#include "Logging.h"
#include "MediaRecorderPrivateOptions.h"
#include "MediaRecorderPrivateWriter.h"
#include "MediaSampleAVFObjC.h"
#include "MediaSamplesBlock.h"
#include "MediaUtilities.h"
#include "WebAudioBufferList.h"

#include <CoreAudio/CoreAudioTypes.h>
#include <CoreMedia/CMTime.h>
#include <mutex>
#include <numbers>
#include <wtf/Locker.h>
#include <wtf/MediaTime.h>

#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

class MediaRecorderPrivateEncoder::Listener : public MediaRecorderPrivateWriterListener {
public:
    static Ref<Listener> create(MediaRecorderPrivateEncoder& encoder) { return adoptRef(*new Listener(encoder)); }

private:
    Listener(MediaRecorderPrivateEncoder& encoder)
        : m_encoder(encoder)
    {
    }

    void appendData(std::span<const uint8_t> data) final
    {
        if (RefPtr protectedEncoder = m_encoder.get())
            protectedEncoder->appendData(data);
    }

    ThreadSafeWeakPtr<MediaRecorderPrivateEncoder> m_encoder;
};

RefPtr<MediaRecorderPrivateEncoder> MediaRecorderPrivateEncoder::create(bool hasAudio, bool hasVideo, const MediaRecorderPrivateOptions& options)
{
#if PLATFORM(COCOA)
    ContentType mimeType(options.mimeType);
    auto containerType = mimeType.containerType();
    Ref encoder = adoptRef(*new MediaRecorderPrivateEncoder(hasAudio, hasVideo));

    auto writer = MediaRecorderPrivateWriter::create(containerType.isEmpty() ? "video/mp4"_s : containerType, encoder->listener());
    if (!writer || !encoder->initialize(options, makeUniqueRefFromNonNullUniquePtr(WTF::move(writer))))
        return nullptr;

    return encoder;
#else
    UNUSED_VARIABLE(hasAudio);
    UNUSED_VARIABLE(hasVideo);
    UNUSED_VARIABLE(options);
    return nullptr;
#endif
}

WorkQueue& MediaRecorderPrivateEncoder::queueSingleton()
{
    static std::once_flag onceKey;
    static LazyNeverDestroyed<Ref<WorkQueue>> workQueue;
    std::call_once(onceKey, [] {
        workQueue.construct(WorkQueue::create("com.apple.MediaRecorderPrivateEncoder"_s));
    });
    return workQueue.get();
}

MediaRecorderPrivateEncoder::MediaRecorderPrivateEncoder(bool hasAudio, bool hasVideo)
    : m_hasAudio(hasAudio)
    , m_hasVideo(hasVideo)
    , m_listener(Listener::create(*this))
{
}

MediaRecorderPrivateEncoder::~MediaRecorderPrivateEncoder() = default;

static String codecStringForMediaVideoCodecId(FourCharCode codec)
{
    switch (codec) {
    case 'vp08': return "vp8"_s;
    case kCMVideoCodecType_VP9: return "vp09.00.10.08"_s; // vp9, profile 0, level 1.0, 8 bits, "VideoRange"
    case 'vp92': return "vp09.02.30.10"_s; // vp9, profile 0, level 1.0, 10 bits, "VideoRange"
    case kCMVideoCodecType_AV1: return "av01.0.01M.08"_s; // av01, "Main", "Level_2_1", "Main", 8 bits, "VideoRange";
    case kCMVideoCodecType_H264: return "avc1.42000a"_s; // AVC Baseline Level 1
    case kCMVideoCodecType_HEVC: return "hev1.1.6.L93.B0"_s; // HEVC progressive, non-packed stream, Main Profile, Main Tier, Level 3.1
    case kAudioFormatMPEG4AAC: return "mp4a.40.2"_s;
    case kAudioFormatOpus: return "opus"_s;
    case kAudioFormatLinearPCM: return "pcm"_s;
    case kAudioFormatAppleLossless: return "alac"_s;
    default:
        ASSERT_NOT_REACHED("Unsupported codec");
        return ""_s;
    }
}

void MediaRecorderPrivateEncoder::compressedAudioOutputBufferCallback(void* MediaRecorderPrivateEncoder, CMBufferQueueTriggerToken)
{
    // We can only be called from the CoreMedia callback if we are still alive.
    RefPtr encoder = static_cast<class MediaRecorderPrivateEncoder*>(MediaRecorderPrivateEncoder);

    queueSingleton().dispatch([weakEncoder = ThreadSafeWeakPtr { *encoder }] {
        if (auto strongEncoder = weakEncoder.get()) {
            assertIsCurrent(queueSingleton());
            strongEncoder->enqueueCompressedAudioSampleBuffers();
            strongEncoder->partiallyFlushEncodedQueues();
        }
    });
}

bool MediaRecorderPrivateEncoder::initialize(const MediaRecorderPrivateOptions& options, UniqueRef<MediaRecorderPrivateWriter>&& writer)
{
    assertIsMainThread();

    lazyInitialize(m_writer, writer.moveToUniquePtr());

    ContentType mimeType(options.mimeType);
    auto containerType = mimeType.containerType();

    bool isWebM = equalLettersIgnoringASCIICase(containerType, "audio/webm"_s) || equalLettersIgnoringASCIICase(containerType, "video/webm"_s);

    m_videoCodec = isWebM ? 'vp08' : kCMVideoCodecType_H264;
    m_audioCodec = isWebM ? kAudioFormatOpus : kAudioFormatMPEG4AAC;

    for (auto& codec : mimeType.codecs()) {
        if (isWebM && (codec.startsWith("vp09.02"_s)))
            m_videoCodec = 'vp92';
        else if (isWebM && (codec.startsWith("vp09"_s) || equal(codec, "vp9"_s) || equal(codec, "vp9.0"_s)))
            m_videoCodec = kCMVideoCodecType_VP9;
        else if (isWebM && (codec.startsWith("vp08"_s) || equal(codec, "vp8"_s) || equal(codec, "vp8.0"_s)))
            m_videoCodec = 'vp08';
        else if (codec == "opus"_s)
            m_audioCodec = kAudioFormatOpus;
        else if (codec == "pcm"_s)
            m_audioCodec = kAudioFormatLinearPCM;
        else if (!isWebM && codec == "alac"_s)
            m_audioCodec = kAudioFormatAppleLossless;
        else if (startsWithLettersIgnoringASCIICase(codec, "avc1"_s))
            m_videoCodec = kCMVideoCodecType_H264;
        else if (!isWebM && (codec.startsWith("hev1."_s) || codec.startsWith("hvc1."_s)))
            m_videoCodec = kCMVideoCodecType_HEVC;
        else if (!isWebM && codec.startsWith("av01.0"_s))
            m_videoCodec = kCMVideoCodecType_AV1;
        else if (!isWebM && codec.startsWith("mp4a.40.2"_s))
            m_audioCodec = kAudioFormatMPEG4AAC;
        else if (!isWebM && codec.startsWith("mp4a.40.5"_s))
            m_audioCodec = kAudioFormatMPEG4AAC_HE;
        else if (!isWebM && codec.startsWith("mp4a.40.23"_s))
            m_audioCodec = kAudioFormatMPEG4AAC_LD;
        else if (!isWebM && codec.startsWith("mp4a.40.29"_s))
            m_audioCodec = kAudioFormatMPEG4AAC_HE_V2;
        else if (!isWebM && codec.startsWith("mp4a.40.39"_s))
            m_audioCodec = kAudioFormatMPEG4AAC_ELD;
        else if (!isWebM && startsWithLettersIgnoringASCIICase(codec, "mp4a"_s))
            m_audioCodec = kAudioFormatMPEG4AAC;
        else
            return false; // unsupported codec.
    }

    RELEASE_LOG(WebRTC, "MediaRecorderPrivateEncoder::initialize isWebM=%d, audioCodec=%d, videCodec=%d", isWebM, hasAudio() ? (int)m_audioCodec : -1, hasVideo() ? (int)m_videoCodec : -1);

    m_audioBitsPerSecond = options.audioBitsPerSecond.value_or(0);
    m_videoBitsPerSecond = options.videoBitsPerSecond.value_or(0);

    if (hasAudio())
        m_audioCodecMimeType = codecStringForMediaVideoCodecId(m_audioCodec);
    if (hasVideo())
        m_videoCodecMimeType = codecStringForMediaVideoCodecId(m_videoCodec);
    m_mimeType = makeString(containerType);

    generateMIMEType();

    return true;
}

void MediaRecorderPrivateEncoder::generateMIMEType()
{
    assertIsMainThread();

    ContentType mimeType(m_mimeType);
    m_mimeType = makeString(mimeType.containerType(), "; codecs="_s, m_videoCodecMimeType, hasVideo() && hasAudio() ? ","_s : ""_s, m_audioCodecMimeType);
}

void MediaRecorderPrivateEncoder::pause()
{
    assertIsMainThread();

    queueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }, this] {
        assertIsCurrent(queueSingleton());
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        m_isPaused = true;
        m_previousSegmentVideoDurationUs = currentEndTime().toMicroseconds();
        RefPtr converter = audioConverter();
        if (!converter)
            return;
        converter->drain()->whenSettled(queueSingleton(), [weakThis] {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            assertIsCurrent(queueSingleton());
            if (auto promise = std::exchange(protectedThis->m_pendingAudioFramePromise, std::nullopt)) {
                promise->second.reject();
                LOG(MediaStream, "MediaRecorderPrivateEncoder::stopRecording rejecting m_pendingAudioFramePromise");
            }
        });
        LOG(MediaStream, "MediaRecorderPrivateEncoder::pause m_currentVideoDuration:%f", m_previousSegmentVideoDurationUs / 1000000.0);
    });
}

void MediaRecorderPrivateEncoder::resume()
{
    assertIsMainThread();

    queueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }, this] {
        assertIsCurrent(queueSingleton());
        if (RefPtr protectedThis = weakThis.get()) {
            m_currentVideoSegmentStartTime.reset();
            m_isPaused = false;
            m_needKeyFrame = true;
            LOG(MediaStream, "MediaRecorderPrivateEncoder:resume at:%f", m_previousSegmentVideoDurationUs / 1000000.0);
        }
    });
}

void MediaRecorderPrivateEncoder::close()
{
    stopRecording();
}

String MediaRecorderPrivateEncoder::mimeType() const
{
    assertIsMainThread();
    return m_mimeType;
}

unsigned MediaRecorderPrivateEncoder::audioBitRate() const
{
    return m_audioBitsPerSecond;
}

unsigned MediaRecorderPrivateEncoder::videoBitRate() const
{
    return m_videoBitsPerSecond;
}


void MediaRecorderPrivateEncoder::appendAudioSampleBuffer(const PlatformAudioData& data, const AudioStreamDescription& description, const WTF::MediaTime&, size_t sampleCount)
{
    if (m_isStopped)
        return;

    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

    if (m_currentStreamDescription != description) {
        LOG(MediaStream, "description changed");
        m_currentStreamDescription = toCAAudioStreamDescription(description);
        addRingBuffer(description);
        m_currentAudioSampleCount = 0;
    } else
        clearRingBuffersIfPossible();

    auto currentAudioTime = m_currentAudioTime;
    m_lastEnqueuedAudioTimeUs = m_currentAudioTime.toMicroseconds();

    m_currentAudioTime += MediaTime(sampleCount, description.sampleRate());
    m_currentAudioTimeUs = m_currentAudioTime.toMicroseconds();

    writeDataToRingBuffer(downcast<WebAudioBufferList>(data).list(), sampleCount, m_currentAudioSampleCount);

    queueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }, currentAudioTime, sampleCount, currentAudioSampleCount = m_currentAudioSampleCount] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->audioSamplesAvailable(currentAudioTime, sampleCount, currentAudioSampleCount);
    });

    m_currentAudioSampleCount += sampleCount;
}

void MediaRecorderPrivateEncoder::audioSamplesDescriptionChanged(const AudioStreamBasicDescription& description, InProcessCARingBuffer* newRingBuffer, size_t ringBufferId)
{
    assertIsCurrent(queueSingleton());

    if (!newRingBuffer) {
        m_hadError = true;
        return;
    }
    if (!m_originalOutputDescription) {
        if (m_audioCodec != kAudioFormatLinearPCM) {
            AudioStreamBasicDescription outputDescription = { };
            outputDescription.mFormatID = m_audioCodec;
            outputDescription.mChannelsPerFrame = description.mChannelsPerFrame;
            outputDescription.mSampleRate = description.mSampleRate;
            m_originalOutputDescription = outputDescription;
        } else
            m_originalOutputDescription = CAAudioStreamDescription { description.mSampleRate, description.mChannelsPerFrame, AudioStreamDescription::Float32, CAAudioStreamDescription::IsInterleaved::Yes }.streamDescription();
    }

    CMFormatDescriptionRef newFormat = nullptr;
    if (auto error = PAL::CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &description, 0, nullptr, 0, nullptr, nullptr, &newFormat)) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateEncoder::audioSamplesAvailable: CMAudioFormatDescriptionCreate failed with %u", error);
        m_hadError = true;
        return;
    }
    m_audioFormatDescription = adoptCF(newFormat);

    if (m_audioConverter) {
        audioConverter()->finish();
        m_formatChangedOccurred = true;
    }

    AudioSampleBufferConverter::Options options = {
        .format = m_audioCodec,
        .description = m_originalOutputDescription,
        .outputBitRate = m_audioBitsPerSecond ? std::optional { m_audioBitsPerSecond } : std::nullopt,
        .generateTimestamp = true
    };
    m_audioConverter = AudioSampleBufferConverter::create(compressedAudioOutputBufferCallback, this, options);
    if (!m_audioConverter) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateEncoder::audioSamplesDescriptionChanged: creation of converter failed");
        m_hadError = true;
        return;
    }

    m_currentRingBuffer = newRingBuffer;
    m_currentRingBufferId = ringBufferId;
}

void MediaRecorderPrivateEncoder::addRingBuffer(const AudioStreamDescription& description)
{
    auto asbd = *std::get<const AudioStreamBasicDescription*>(description.platformDescription().description);
    m_ringBuffers.append(std::make_pair(InProcessCARingBuffer::allocate(asbd, description.sampleRate() * 2), ++m_lastRingBufferId)); // allocate 2s of buffer.
    queueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }, description = asbd, newRingBuffer = m_ringBuffers.last().first.get(), lastRingBufferId = m_lastRingBufferId] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->audioSamplesDescriptionChanged(description, newRingBuffer, lastRingBufferId);
    });
}

void MediaRecorderPrivateEncoder::writeDataToRingBuffer(AudioBufferList* list, size_t sampleCount, size_t totalSampleCount)
{
    ASSERT(!m_ringBuffers.isEmpty());
    if (!m_ringBuffers.last().first)
        return;
    m_ringBuffers.last().first->store(list, sampleCount, totalSampleCount);
}

void MediaRecorderPrivateEncoder::clearRingBuffersIfPossible()
{
    if (m_ringBuffers.size() == 1)
        return;
    size_t currentRingBufferId = m_currentRingBufferId;
    while (m_ringBuffers.size() > 1) {
        if (m_ringBuffers.first().second < currentRingBufferId)
            m_ringBuffers.removeFirst();
        else
            break;
    };
}

void MediaRecorderPrivateEncoder::audioSamplesAvailable(const MediaTime& time, size_t sampleCount, size_t totalSampleCount)
{
    assertIsCurrent(queueSingleton());

    ASSERT(m_audioFormatDescription);

    if (m_hadError)
        return;

    auto* asbd = PAL::CMAudioFormatDescriptionGetStreamBasicDescription(m_audioFormatDescription.get());
    ASSERT(asbd);
    if (!asbd) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateEncoder::audioSamplesAvailable: inconsistent running state");
        m_hadError = true;
        return;
    }

    auto result = WebAudioBufferList::createWebAudioBufferListWithBlockBuffer(*asbd, sampleCount);
    if (!result) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateEncoder::audioSamplesAvailable: failed to create block buffer");
        m_hadError = true;
        return;
    }
    auto [list, block] = WTF::move(*result);

    ASSERT(m_currentRingBuffer);
    m_currentRingBuffer->fetch(list->list(), sampleCount, totalSampleCount);

    CMSampleBufferRef sampleBuffer = nullptr;
    if (auto error = PAL::CMAudioSampleBufferCreateWithPacketDescriptions(kCFAllocatorDefault, block.get(), true, nullptr, nullptr, m_audioFormatDescription.get(), sampleCount, PAL::toCMTime(time), nullptr, &sampleBuffer)) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateEncoder::audioSamplesAvailable: CMAudioSampleBufferCreateWithPacketDescriptions failed with error %d", error);
        m_hadError = true;
        return;
    }
    RetainPtr sample = adoptCF(sampleBuffer);

    m_lastRawAudioSample = time;
    audioConverter()->addSampleBuffer(sampleBuffer);
}

void MediaRecorderPrivateEncoder::appendVideoFrame(VideoFrame& frame)
{
    if (m_isStopped)
        return;

    queueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }, this, frame = Ref { frame }, audioTime = lastEnqueuedAudioTime(), now = MonotonicTime::now()] mutable {
        assertIsCurrent(queueSingleton());
        if (RefPtr protectedThis = weakThis.get()) {
            if (m_isPaused)
                return;
            auto nextVideoFrameTime = currentTime(audioTime, now);
            if (!m_currentVideoSegmentStartTime) {
                m_currentVideoSegmentStartTime = now;
                // We take the time before m_previousSegmentVideoDurationUs is set so that the first frame will always appear to have a timestamp of 0 but with a longer duration.
                nextVideoFrameTime = MediaTime(m_previousSegmentVideoDurationUs, 1000000);
            }
            m_lastRawVideoFrameReceived = nextVideoFrameTime;
            appendVideoFrame(nextVideoFrameTime, WTF::move(frame));
        }
    });
}

void MediaRecorderPrivateEncoder::appendVideoFrame(MediaTime sampleTime, Ref<VideoFrame>&& frame)
{
    assertIsCurrent(queueSingleton());

    if (!m_firstVideoFrameProcessed) {
        m_firstVideoFrameProcessed = true;

        if (frame->rotation() != VideoFrame::Rotation::None || frame->isMirrored()) {
            m_videoTransform = CGAffineTransformMakeRotation(static_cast<int>(frame->rotation()) * std::numbers::pi / 180);
            if (frame->isMirrored())
                m_videoTransform = CGAffineTransformScale(*m_videoTransform, -1, 1);
        }
        VideoEncoder::Config config { static_cast<uint64_t>(frame->presentationSize().width()), static_cast<uint64_t>(frame->presentationSize().height()), false, videoBitRate() };

        Ref promise = VideoEncoder::create(codecStringForMediaVideoCodecId(m_videoCodec), config, [weakThis = ThreadSafeWeakPtr { *this }, config](auto&& configuration) mutable {
            queueSingleton().dispatch([weakThis, config = WTF::move(config), configuration = WTF::move(configuration)]() mutable {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->processVideoEncoderActiveConfiguration(config, WTF::move(configuration));
            });
        }, [weakThis = ThreadSafeWeakPtr { *this }](auto&& frame) {
            queueSingleton().dispatch([weakThis, frame = WTF::move(frame)]() mutable {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->enqueueCompressedVideoFrame(WTF::move(frame));
            });
        });
        GenericNonExclusivePromise::Producer producer;
        m_videoEncoderCreationPromise = producer.promise();
        promise->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, this](auto&& result) {
            assertIsCurrent(queueSingleton());
            if (RefPtr protectedThis = weakThis.get(); protectedThis && result) {
                m_videoEncoder = WTF::move(*result);
                Ref { *m_videoEncoder }->setRates(videoBitRate(), 0);
                m_videoEncoderCreationPromise = nullptr;
                return encodePendingVideoFrames(MediaTime::positiveInfiniteTime());
            }
            return GenericPromise::createAndResolve();
        })->chainTo(WTF::move(producer));
    }

    // FIXME: AVAssetWriter errors when we attempt to add a sample with the same time.
    // When we start and audio isn't ready, we may output multiple frame that has a timestamp of 0.
    if (sampleTime <= m_lastEnqueuedRawVideoFrame)
        sampleTime = m_lastEnqueuedRawVideoFrame + MediaTime(1, 1000000);

    m_lastEnqueuedRawVideoFrame = sampleTime;
    m_pendingVideoFrames.append({ WTF::move(frame), sampleTime });
    LOG(MediaStream, "appendVideoFrame:enqueuing raw video frame:%f queue:%zu first:%f last:%f (received audio:%d)", sampleTime.toDouble(), m_pendingVideoFrames.size(), m_pendingVideoFrames.first().second.toDouble(), m_pendingVideoFrames.last().second.toDouble(), !!m_lastEnqueuedAudioTimeUs.load());

    encodePendingVideoFrames(MediaTime::positiveInfiniteTime());
}

void MediaRecorderPrivateEncoder::appendData(std::span<const uint8_t> data)
{
    Locker locker { m_lock };

    if (!m_dataBuffer.capacity())
        m_dataBuffer.reserveInitialCapacity(s_dataBufferSize);

    if (data.size() > (m_dataBuffer.capacity() - m_dataBuffer.size()))
        flushDataBuffer();
    if (data.size() < m_dataBuffer.capacity())
        m_dataBuffer.append(data);
    else
        m_data.append(data);
}

void MediaRecorderPrivateEncoder::flushDataBuffer()
{
    assertIsHeld(m_lock);

    if (m_dataBuffer.isEmpty())
        return;
    m_data.append(std::exchange(m_dataBuffer, { }));
}

bool MediaRecorderPrivateEncoder::segmentsMustStartWithVideoKeyframe() const
{
    if (!hasVideo())
        return false;
    if (!m_writer)
        return false;
    return m_writer->segmentsMustStartWithKeyframe();
}

bool MediaRecorderPrivateEncoder::hasMuxedDataSinceEndSegment() const
{
    assertIsCurrent(queueSingleton());

    return (!hasAudio() || m_hasMuxedAudioFrameSinceEndSegment) && (!hasVideo() || m_hasMuxedVideoFrameSinceEndSegment);
}

Ref<FragmentedSharedBuffer> MediaRecorderPrivateEncoder::takeData()
{
    assertIsCurrent(queueSingleton());

    {
        Locker locker { m_lock };
        flushDataBuffer();
        return m_data.takeBuffer();
    }
}

void MediaRecorderPrivateEncoder::enqueueCompressedAudioSampleBuffers()
{
    assertIsCurrent(queueSingleton());

    ASSERT(hasAudio());

    if (!audioConverter() || audioConverter()->isEmpty())
        return;

    if (!m_audioCompressedAudioInfo) {
        RetainPtr audioFormatDescription = PAL::CMSampleBufferGetFormatDescription(audioConverter()->getOutputSampleBuffer());
        m_audioCompressedAudioInfo = createAudioInfoFromFormatDescription(audioFormatDescription.get());
        ASSERT(m_audioCompressedAudioInfo);
        if (!m_audioCompressedAudioInfo) {
            RELEASE_LOG_ERROR(MediaStream, "createAudioInfoFromFormatDescription: Failed to create AudioInfo");
            return;
        }
        if (auto result = m_writer->addAudioTrack(Ref { *m_audioCompressedAudioInfo })) {
            m_audioCompressedAudioInfo->setTrackID(*result);
            m_audioTrackIndex = result;
        } else {
            RELEASE_LOG_ERROR(MediaStream, "appendAudioFrame: Failed to create audio track");
            return;
        }
        maybeStartWriter();
    }

    if (!m_audioTrackIndex) {
        RELEASE_LOG_ERROR(MediaStream, "enqueueCompressedAudioSampleBuffers: failed following failure to create audio track");
        return;
    }

    auto processSample = [&](auto&& sample) {
        assertIsCurrent(queueSingleton());
        if (m_pendingAudioFramePromise && m_pendingAudioFramePromise->first <= sample->presentationEndTime()) {
            m_pendingAudioFramePromise->second.resolve();
            m_pendingAudioFramePromise.reset();
        }
        if (!m_hasStartedAudibleAudioFrame && sample->duration())
            m_hasStartedAudibleAudioFrame = true;
        m_encodedAudioFrames.append(samplesBlockFromCMSampleBuffer(sample->sampleBuffer(), m_audioCompressedAudioInfo.get()));
        m_lastEncodedAudioSampleRange = { sample->presentationTime(), sample->presentationEndTime() };
        LOG(MediaStream, "enqueueCompressedAudioSampleBuffers: adding compressed audio: %f-%f", sample->presentationTime().toDouble(), sample->presentationEndTime().toDouble());
    };

    while (RetainPtr sampleBlock = audioConverter()->takeOutputSampleBuffer()) {
        if (m_formatChangedOccurred) {
            // Writing audio samples requiring an edit list is forbidden by the AVAssetWriterInput when used with fMP4, remove the keys.
            PAL::CMRemoveAttachment(sampleBlock.get(), PAL::kCMSampleBufferAttachmentKey_TrimDurationAtStart);
            PAL::CMRemoveAttachment(sampleBlock.get(), PAL::kCMSampleBufferAttachmentKey_TrimDurationAtEnd);
        }

        if (m_audioCodec == kAudioFormatLinearPCM) {
            processSample(MediaSampleAVFObjC::create(sampleBlock.get(), *m_audioTrackIndex));
            continue;
        }

        for (Ref sample : MediaSampleAVFObjC::create(sampleBlock.get(), *m_audioTrackIndex)->divide())
            processSample(sample);
    }
}

void MediaRecorderPrivateEncoder::maybeStartWriter()
{
    assertIsCurrent(queueSingleton());

    if (m_writerIsStarted)
        return;

    if ((hasAudio() && !m_audioTrackIndex) || (hasVideo() && !m_videoTrackIndex))
        return;

    m_writer->allTracksAdded();
    m_writerIsStarted = true;
}

Ref<GenericPromise> MediaRecorderPrivateEncoder::encodePendingVideoFrames(const MediaTime& endTime)
{
    assertIsCurrent(queueSingleton());

    if (m_videoEncoderCreationPromise) {
        GenericPromise::Producer producer;
        Ref promise = producer.promise();
        RefPtr { m_videoEncoderCreationPromise }->chainTo(WTF::move(producer));
        return promise;
    }

    size_t framesToEncode = [&] {
        assertIsCurrent(queueSingleton());
        if (m_pendingVideoFrames.isEmpty() || endTime.isPositiveInfinite())
            return m_pendingVideoFrames.size();
        size_t framesPriorEndTime = 0;
        for (auto& frame : m_pendingVideoFrames) {
            if (frame.second > endTime)
                break;
            framesPriorEndTime++;
        }
        return framesPriorEndTime;
    }();

    if (!framesToEncode)
        return GenericPromise::createAndResolve();

    GenericPromise::Producer producer;
    Ref promise = producer.promise();

    Vector<Ref<VideoEncoder::EncodePromise>> promises { framesToEncode , [&](size_t) {
        assertIsCurrent(queueSingleton());
        auto frame = m_pendingVideoFrames.takeFirst();
        bool needVideoKeyframe = false;
        // Ensure we create GOP at regular interval.
        if (((frame.second - m_lastVideoKeyframeTime) >= m_maxGOPDuration) || m_needKeyFrame) {
            needVideoKeyframe = true;
            m_lastVideoKeyframeTime = frame.second;
            m_needKeyFrame = false;
        }
        RELEASE_LOG_INFO(MediaStream, "encodePendingVideoFrames:encoding video frame:%f (us:%lld) kf:%d endTime:%f", frame.second.toDouble(), frame.second.toMicroseconds(), needVideoKeyframe, endTime.toDouble());
        return Ref { *m_videoEncoder }->encode({ WTF::move(frame.first), frame.second.toMicroseconds(), { } }, needVideoKeyframe);
    } };
    VideoEncoder::EncodePromise::all(WTF::move(promises))->chainTo(WTF::move(producer));

    return promise;
}

void MediaRecorderPrivateEncoder::processVideoEncoderActiveConfiguration(const VideoEncoder::Config& config, const VideoEncoderActiveConfiguration& configuration)
{
    assertIsCurrent(queueSingleton());

    Ref videoInfo = VideoInfo::create({
        {
            .codecName = m_videoCodec,
        }, {
            .size = configuration.visibleWidth && configuration.visibleHeight ? FloatSize { static_cast<float>(*configuration.visibleWidth), static_cast<float>(*configuration.visibleHeight) } : FloatSize { static_cast<float>(config.width), static_cast<float>(config.height) },
            .displaySize = configuration.displayWidth && configuration.displayHeight ? FloatSize { static_cast<float>(*configuration.displayWidth), static_cast<float>(*configuration.displayHeight) } : FloatSize { static_cast<float>(config.width), static_cast<float>(config.height) },
            .colorSpace = configuration.colorSpace.value_or(PlatformVideoColorSpace { }),
            .extensionAtoms = configuration.description ? Vector<TrackInfo::AtomData> { 1, { computeBoxType(m_videoCodec), SharedBuffer::create(*configuration.description) } } : Vector<TrackInfo::AtomData> { }
        }
    });
    m_videoTrackInfo = videoInfo.copyRef();
    if (auto result = m_writer->addVideoTrack(Ref { *m_videoTrackInfo }, m_videoTransform)) {
        m_videoTrackIndex = result;
        m_videoTrackInfo->setTrackID(*m_videoTrackIndex);
    } else {
        RELEASE_LOG_ERROR(MediaStream, "appendVideoFrame: Failed to create video track");
        return;
    }
    maybeStartWriter();
    if (configuration.codec.length()) {
        callOnMainThread([weakThis = ThreadSafeWeakPtr { *this }, codec = configuration.codec.isolatedCopy()]() mutable {
            if (RefPtr protectedThis = weakThis.get()) {
                assertIsMainThread();
                protectedThis->m_videoCodecMimeType = WTF::move(codec);
                protectedThis->generateMIMEType();
            }
        });
    }
    encodePendingVideoFrames(MediaTime::positiveInfiniteTime());
}

void MediaRecorderPrivateEncoder::enqueueCompressedVideoFrame(VideoEncoder::EncodedFrame&& frame)
{
    assertIsCurrent(queueSingleton());

    if (!m_videoTrackIndex) {
        RELEASE_LOG_ERROR(MediaStream, "enqueueCompressedVideoFrame: failed following failure to create video track");
        return;
    }

    MediaTime compressedFrameTime { frame.timestamp, 1000000 };

    ASSERT(m_videoTrackInfo);

    MediaSamplesBlock::SamplesVector vector(1, {
        .presentationTime = compressedFrameTime,
        .decodeTime = compressedFrameTime,
        .data = SharedBuffer::create(frame.data),
        .flags = frame.isKeyFrame ? MediaSample::SampleFlags::IsSync : MediaSample::SampleFlags::None
    });
    m_encodedVideoFrames.append(makeUniqueRef<MediaSamplesBlock>(m_videoTrackInfo.get(), WTF::move(vector)));
    LOG(MediaStream, "appendVideoFrame:Receiving compressed %svideo frame: queue:%zu first:%f last:%f", frame.isKeyFrame ? "keyframe " : "", m_encodedVideoFrames.size(), m_encodedVideoFrames.first()->presentationTime().toDouble(), m_encodedVideoFrames.last()->presentationTime().toDouble());
    partiallyFlushEncodedQueues();
}

void MediaRecorderPrivateEncoder::partiallyFlushEncodedQueues()
{
    assertIsCurrent(queueSingleton());

    if (m_pendingFlush)
        return;

    LOG(MediaStream, "partiallyFlushEncodedQueues: lastAudioReceived:%f audioqueue:%zu first:%f last:%f videoQueue:%zu first:%f (kf:%d) last:%f", lastEnqueuedAudioTime().toDouble(), m_encodedAudioFrames.size(), m_encodedAudioFrames.size() ? m_encodedAudioFrames.first()->presentationTime().toDouble() : 0, m_encodedAudioFrames.size() ? m_encodedAudioFrames.last()->presentationTime().toDouble() : 0, m_encodedVideoFrames.size(), m_encodedVideoFrames.size() ? m_encodedVideoFrames.first()->presentationTime().toDouble() : 0, m_encodedVideoFrames.size() ? m_encodedVideoFrames.first()->isSync() : 0, m_encodedVideoFrames.size() ? m_encodedVideoFrames.last()->presentationTime().toDouble() : 0);

    if (hasAudio())
        enqueueCompressedAudioSampleBuffers(); // compressedAudioOutputBufferCallback isn't always called when new frames are available. Force refresh

    if (!m_writerIsStarted)
        return;

    while ((!hasVideo() || !m_encodedVideoFrames.isEmpty()) && (!hasAudio() || !m_encodedAudioFrames.isEmpty()))
        interleaveAndEnqueueNextFrame();
}

Ref<GenericPromise> MediaRecorderPrivateEncoder::waitForMatchingAudio(const MediaTime& flushTime)
{
    assertIsCurrent(queueSingleton());

    if (!hasAudio() || !hasVideo() || m_isPaused)
        return GenericPromise::createAndResolve(); // Nothing to sync.

    MediaSamplesBlock* videoFrame = nullptr;
    for (auto& frame : m_encodedVideoFrames) {
        if (frame->presentationTime() > flushTime)
            break;
        videoFrame = frame.ptr();
    }
    if (!videoFrame)
        return GenericPromise::createAndResolve();

    if (videoFrame->presentationTime() < m_lastMuxedAudioSampleEndTime)
        return GenericPromise::createAndResolve(); // video frame is directly muxable.

    if (!m_encodedAudioFrames.isEmpty() && m_encodedAudioFrames.last()->presentationEndTime() >= videoFrame->presentationTime())
        return GenericPromise::createAndResolve();

    // The audio frame matching the last video frame is still pending in the converter and requires more data to be produced.
    // It will be resolved once we receive the compressed audio sample in enqueueCompressedAudioSampleBuffers().
    m_pendingAudioFramePromise.emplace(std::min(m_lastRawVideoFrameReceived, videoFrame->presentationTime()), GenericPromise::Producer());

    LOG(MediaStream, "MediaRecorderPrivateEncoder::waitForMatchingAudio waiting for audio:%f", videoFrame->presentationTime().toDouble());

    return m_pendingAudioFramePromise->second.promise();
}

MediaTime MediaRecorderPrivateEncoder::flushToEndSegment(const MediaTime& flushTime)
{
    assertIsCurrent(queueSingleton());

    RELEASE_LOG_INFO(MediaStream, "MediaRecorderPrivateEncoder::flushToEndSegment(%f): lastAudioReceived:%f audioqueue:%zu first:%f last:%f videoQueue:%zu first:%f (kf:%d) last:%f", flushTime.toDouble(), lastEnqueuedAudioTime().toDouble(), m_encodedAudioFrames.size(), m_encodedAudioFrames.size() ? m_encodedAudioFrames.first()->presentationTime().toDouble() : 0, m_encodedAudioFrames.size() ? m_encodedAudioFrames.last()->presentationTime().toDouble() : 0, m_encodedVideoFrames.size(), m_encodedVideoFrames.size() ? m_encodedVideoFrames.first()->presentationTime().toDouble() : 0, m_encodedVideoFrames.size() ? m_encodedVideoFrames.first()->isSync() : 0, m_encodedVideoFrames.size() ? m_encodedVideoFrames.last()->presentationTime().toDouble() : 0);

    if (hasAudio())
        enqueueCompressedAudioSampleBuffers(); // compressedAudioOutputBufferCallback isn't always called when new frames are available. Force refresh

    if (!m_writerIsStarted || (hasAudio() && !m_hasStartedAudibleAudioFrame))
        return MediaTime::invalidTime();

    ASSERT(!m_videoEncoderCreationPromise);

    // Find last video keyframe in the queue.
    MediaSamplesBlock* lastVideoKeyFrame = nullptr;
    if (segmentsMustStartWithVideoKeyframe()) {
        for (auto it = m_encodedVideoFrames.rbegin(); it != m_encodedVideoFrames.rend(); ++it) {
            if ((*it)->isSync()) {
                lastVideoKeyFrame = it->ptr();
                break;
            }
        }
    }

    // Mux all video frames until we reached the end of the queue or we reached a video frame greater than flushTime or we found a keyfram (if writer requires it).
    while ((!m_encodedVideoFrames.isEmpty() || !m_encodedAudioFrames.isEmpty())) {
        auto* audioFrame = m_encodedAudioFrames.size() ? m_encodedAudioFrames.first().ptr() : nullptr;
        auto* videoFrame = m_encodedVideoFrames.size() ? m_encodedVideoFrames.first().ptr() : nullptr;
        ASSERT(audioFrame || videoFrame);
        bool takeVideo = videoFrame && (!audioFrame || videoFrame->presentationTime() < audioFrame->presentationTime());
        auto& frame = takeVideo ? *videoFrame : *audioFrame;
        if (takeVideo && &frame == lastVideoKeyFrame) {
            LOG(MediaStream, "flushToEndSegment: stopping prior video keyframe time:%f", frame.presentationTime().toDouble());
            return frame.presentationTime();
        }

        if (segmentsMustStartWithVideoKeyframe()) {
            // We are about to end the current segment and the next segment needs to start with a keyframe.
            // Don't mux the current audio frame (if any) if the next video frame is a keyframe and is to be displayed
            // while the current audio frame is playing.
            if (!takeVideo && videoFrame) {
                if ((videoFrame->presentationTime() <= frame.presentationEndTime()) && videoFrame == lastVideoKeyFrame) {
                    LOG(MediaStream, "flushToEndSegment: stopping prior audio containing video keyframe time:%f", frame.presentationTime().toDouble());
                    return frame.presentationTime();
                }
            }
        } else {
            if (takeVideo && videoFrame->presentationTime() > (hasAudio() ? std::min(m_lastEncodedAudioSampleRange.end, flushTime) : flushTime)) {
                auto endMuxedTime = hasAudio() ? std::min(m_lastEncodedAudioSampleRange.end, frame.presentationTime()) : frame.presentationTime();
                LOG(MediaStream, "flushToEndSegment: stopping with video frame:%f (> %f) lastEncodedAudioSampleRange:%f-%f lastRawAudioSample:%f endMuxedTime:%f", frame.presentationTime().toDouble(), flushTime.toDouble(), m_lastEncodedAudioSampleRange.start.toDouble(), m_lastEncodedAudioSampleRange.end.toDouble(), m_lastRawAudioSample.toDouble(), endMuxedTime.toDouble());
                return endMuxedTime;
            }
            // We are about to end the current segment.
            // Don't mux the current audio frame (if any) if the next video frame is past flushTime and is to be displayed
            // while the current audio frame is playing.
            if (!takeVideo && videoFrame) {
                if ((videoFrame->presentationTime() <= frame.presentationEndTime()) && videoFrame->presentationTime() > flushTime) {
                    RELEASE_LOG_INFO(MediaStream, "flushToEndSegment: stopping prior audio containing video keyframe time:%f", frame.presentationTime().toDouble());
                    return frame.presentationTime();
                }
            }
        }

        // If we don't have any more video frames pending (the next incoming frame will be a keyframe),
        // we write all the audio frames received with a date prior flushTime.
        if (!takeVideo && !videoFrame && frame.presentationTime() > flushTime) {
            return frame.presentationTime();
            break;
        }

        interleaveAndEnqueueNextFrame();
    };

    return hasAudio() && m_interleavedFrames.size() ? m_interleavedFrames.last()->presentationEndTime() : flushTime;
}

void MediaRecorderPrivateEncoder::flushAllEncodedQueues()
{
    assertIsCurrent(queueSingleton());

    LOG(MediaStream, "flushAllEncodedQueues: audioqueue:%zu first:%f last:%f videoQueue:%zu first:%f (kf:%d) last:%f", m_encodedAudioFrames.size(), m_encodedAudioFrames.size() ? m_encodedAudioFrames.first()->presentationTime().toDouble() : 0, m_encodedAudioFrames.size() ? m_encodedAudioFrames.last()->presentationTime().toDouble() : 0, m_encodedVideoFrames.size(), m_encodedVideoFrames.size() ? m_encodedVideoFrames.first()->presentationTime().toDouble() : 0, m_encodedVideoFrames.size() ? m_encodedVideoFrames.first()->isSync() : 0, m_encodedVideoFrames.size() ? m_encodedVideoFrames.last()->presentationTime().toDouble() : 0);

    if (hasAudio())
        enqueueCompressedAudioSampleBuffers(); // compressedAudioOutputBufferCallback isn't always called when new frames are available. Force refresh

    if (!m_writerIsStarted)
        m_writer->allTracksAdded();

    while ((!m_encodedVideoFrames.isEmpty() || !m_encodedAudioFrames.isEmpty()))
        interleaveAndEnqueueNextFrame();
}

void MediaRecorderPrivateEncoder::interleaveAndEnqueueNextFrame()
{
    assertIsCurrent(queueSingleton());

    // Determine if we should mux a video or audio frame. We favor the audio frame first to satisfy webm muxing requirement:
    // https://www.webmproject.org/docs/container/
    // - Audio blocks that contain the video key frame's timecode SHOULD be in the same cluster as the video key frame block.
    // - Audio blocks that have same absolute timecode as video blocks SHOULD be written before the video blocks.
    auto* audioFrame = m_encodedAudioFrames.size() ? m_encodedAudioFrames.first().ptr() : nullptr;
    auto* videoFrame = m_encodedVideoFrames.size() ? m_encodedVideoFrames.first().ptr() : nullptr;
    ASSERT(audioFrame || videoFrame);
    bool takeVideo = videoFrame && (!audioFrame || videoFrame->presentationTime() < audioFrame->presentationTime());
    UniqueRef frame = (takeVideo ? m_encodedVideoFrames : m_encodedAudioFrames).takeFirst();

    ASSERT(!takeVideo || !segmentsMustStartWithVideoKeyframe() || !m_nextVideoFrameMuxedShouldBeKeyframe || frame->isSync());

    if (frame->presentationTime() < m_lastMuxedSampleStartTime)
        RELEASE_LOG_ERROR(MediaStream, "interleaveAndEnqueueNextFrame: added %s (kf:%d) frame time:%f-%f (previous:%f type:%s) lastEnqueuedRawVideoFrame:%f m_lastRawAudioSample:%f m_lastEncodedAudioSampleRange:%f-%f size:%zu with error", takeVideo ? "video" : "audio", frame->isSync(), frame->presentationTime().toDouble(), frame->presentationEndTime().toDouble(), m_lastMuxedSampleStartTime.toDouble(), m_lastMuxedSampleIsVideo ? "video" : "audio" , m_lastEnqueuedRawVideoFrame.toDouble(), m_lastRawAudioSample.toDouble(), m_lastEncodedAudioSampleRange.start.toDouble(), m_lastEncodedAudioSampleRange.end.toDouble(), m_interleavedFrames.size());
    else
        LOG(MediaStream, "interleaveAndEnqueueNextFrame: added %s (kf:%d) frame time:%f-%f (previous:%f type:%s) m_lastRawAudioSample:%f m_lastEncodedAudioSampleRange:%f-%f size:%zu", takeVideo ? "video" : "audio", frame->isSync(), frame->presentationTime().toDouble(), frame->presentationEndTime().toDouble(), m_lastMuxedSampleStartTime.toDouble(), m_lastMuxedSampleIsVideo ? "video" : "audio" , m_lastRawAudioSample.toDouble(), m_lastEncodedAudioSampleRange.start.toDouble(), m_lastEncodedAudioSampleRange.end.toDouble(), m_interleavedFrames.size());
    ASSERT(frame->presentationTime() >= m_lastMuxedSampleStartTime);
    m_lastMuxedSampleStartTime = frame->presentationTime();
    m_lastMuxedSampleIsVideo = takeVideo;
    if (takeVideo) {
        m_hasMuxedVideoFrameSinceEndSegment = true;
        m_nextVideoFrameMuxedShouldBeKeyframe = false;
    } else {
        m_hasMuxedAudioFrameSinceEndSegment = true;
        m_lastMuxedAudioSampleEndTime = frame->presentationEndTime();
    }
    m_interleavedFrames.append(WTF::move(frame));
}

void MediaRecorderPrivateEncoder::stopRecording()
{
    assertIsMainThread();

    if (m_isStopped)
        return;

    LOG(MediaStream, "MediaRecorderPrivateEncoder::stopRecording");
    m_isStopped = true;

    queueSingleton().dispatch([protectedThis = Ref { *this }, this] {
        assertIsCurrent(queueSingleton());

        m_isPaused = false;

        RefPtr converter = audioConverter();
        if (!converter)
            return;
        converter->finish()->whenSettled(queueSingleton(), [protectedThis] {
            assertIsCurrent(queueSingleton());
            if (auto promise = std::exchange(protectedThis->m_pendingAudioFramePromise, std::nullopt)) {
                promise->second.reject();
                LOG(MediaStream, "MediaRecorderPrivateEncoder::stopRecording rejecting m_pendingAudioFramePromise");
            }
        });
    });

    m_currentFlushOperations = Ref { m_currentFlushOperations }->whenSettled(queueSingleton(), [protectedThis = Ref { *this }, this] {
        return flushPendingData(MediaTime::positiveInfiniteTime())->whenSettled(queueSingleton(), [protectedThis, this] {
            assertIsCurrent(queueSingleton());
            if (m_videoEncoder)
                Ref { *m_videoEncoder }->close();

            flushAllEncodedQueues();
            return m_writer->writeFrames(std::exchange(m_interleavedFrames, { }), currentEndTime())->whenSettled(queueSingleton(), [protectedThis, this] {
                assertIsCurrent(queueSingleton());
                ASSERT(!m_videoEncoderCreationPromise && m_pendingVideoFrames.isEmpty() && m_encodedVideoFrames.isEmpty());
                ASSERT(m_encodedAudioFrames.isEmpty() && (!m_audioConverter || audioConverter()->isEmpty()));

                m_writerIsClosed = true;

                LOG(MediaStream, "FlushPendingData::close writer time:%f", currentEndTime().toDouble());

                return m_writer->close();
            });
        });
    });
}

void MediaRecorderPrivateEncoder::fetchData(CompletionHandler<void(Ref<FragmentedSharedBuffer>&&, double)>&& completionHandler)
{
    assertIsMainThread();

    m_currentFlushOperations = Ref { m_currentFlushOperations }->whenSettled(queueSingleton(), [protectedThis = Ref { *this }, this, completionHandler = WTF::move(completionHandler), audioTime = lastEnqueuedAudioTime(), now = MonotonicTime::now()]() mutable {
        auto currentTime = this->currentTime(audioTime, now);
        return flushPendingData(currentTime)->whenSettled(queueSingleton(), [protectedThis, this, completionHandler = WTF::move(completionHandler), currentTime]() mutable {
            assertIsCurrent(queueSingleton());
            Ref data = takeData();
            LOG(MediaStream, "fetchData::returning data:%zu timeCode:%f time:%f", data->size(), m_timeCode, currentTime.toDouble());
            callOnMainThread([completionHandler = WTF::move(completionHandler), data, timeCode = m_timeCode]() mutable {
                completionHandler(WTF::move(data), timeCode);
            });
            if (data->size())
                m_timeCode = currentTime.toDouble();
            return GenericPromise::createAndResolve();
        });
    });
}

Ref<GenericPromise> MediaRecorderPrivateEncoder::flushPendingData(const MediaTime& currentTime)
{
    assertIsCurrent(queueSingleton());

    m_needKeyFrame = segmentsMustStartWithVideoKeyframe();

    LOG(MediaStream, "MediaRecorderPrivateEncoder::FlushPendingData upTo:%f", currentTime.toDouble());

    Vector<Ref<GenericPromise>> promises;
    promises.reserveInitialCapacity(size_t(!!m_videoEncoder) + size_t(!!m_audioConverter) + 1);
    promises.append(encodePendingVideoFrames(currentTime));
    if (m_videoEncoder)
        promises.append(Ref { *m_videoEncoder }->flush());
    if (RefPtr converter = audioConverter())
        promises.append(converter->flush());

    ASSERT(!m_pendingFlush, "flush are serialized");
    m_pendingFlush++;

    return GenericPromise::all(WTF::move(promises))->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, this, currentTime] {
        assertIsCurrent(queueSingleton());
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return GenericPromise::createAndResolve();
        return waitForMatchingAudio(currentTime);
    })->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, this, currentTime](auto&& result) {
        assertIsCurrent(queueSingleton());
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return GenericPromise::createAndResolve();

        auto endMuxedTime = flushToEndSegment(currentTime);

        m_pendingFlush--;

        if (endMuxedTime.isInvalid())
            return GenericPromise::createAndResolve();

        // Write to MediaRecorderPrivateWriter if:
        // 1: We aren't stopped (all frames will be flushed and written upon the promise being resolved) and
        // 2: We have muxed data and
        // 3: waitForMatchingAudio succeeded and
        // If writer requires segments to start with keyframes:
        // 4: We have muxed data for all tracks (Ending the current segment before frames of all kind have been amended results in a broken file) and
        // 5: We have accumulated more than m_minimumSegmentDuration of content or we are paused.
        if (!m_isStopped && !m_interleavedFrames.isEmpty() && result
            && (m_isPaused || !segmentsMustStartWithVideoKeyframe() || (hasMuxedDataSinceEndSegment() && (endMuxedTime - m_startLastSegmentTime >= m_minimumSegmentDuration)))) {
            if (endMuxedTime - m_startLastSegmentTime >= m_minimumSegmentDuration)
                m_startLastSegmentTime = endMuxedTime;
            m_nextVideoFrameMuxedShouldBeKeyframe = segmentsMustStartWithVideoKeyframe();
            m_hasMuxedAudioFrameSinceEndSegment = false;
            m_hasMuxedVideoFrameSinceEndSegment = false;
            GenericPromise::Producer producer;
            Ref promise = producer.promise();
            LOG(MediaStream, "MediaRecorderPrivateEncoder::flushPendingData writing %zu frames start:%f end:%f", m_interleavedFrames.size(), m_interleavedFrames.first()->presentationTime().toDouble(), m_interleavedFrames.last()->presentationEndTime().toDouble());
            m_writer->writeFrames(std::exchange(m_interleavedFrames, { }), endMuxedTime)->chainTo(WTF::move(producer));
            return promise;
        }
        return GenericPromise::createAndResolve();
    });
}

MediaTime MediaRecorderPrivateEncoder::currentTime() const
{
    if (hasAudio())
        return lastEnqueuedAudioTime();

    return currentTime(lastEnqueuedAudioTime(), MonotonicTime::now());
}

MediaTime MediaRecorderPrivateEncoder::currentTime(const MediaTime& audioTime, const MonotonicTime& now) const
{
    if (hasAudio())
        return audioTime;

    assertIsCurrent(queueSingleton());

    auto currentDuration = MediaTime::createWithSeconds(Seconds::fromMicroseconds(m_previousSegmentVideoDurationUs));
    if (!m_currentVideoSegmentStartTime)
        return currentDuration;
    return MediaTime::createWithSeconds(now - *m_currentVideoSegmentStartTime) + currentDuration;
}

MediaTime MediaRecorderPrivateEncoder::currentEndTime() const
{
    if (hasAudio())
        return MediaTime(m_currentAudioTimeUs.load(), 1000000);

    return currentTime();
}

RefPtr<AudioSampleBufferConverter> MediaRecorderPrivateEncoder::audioConverter() const
{
    assertIsCurrent(queueSingleton());

    return m_audioConverter;
}

Ref<MediaRecorderPrivateWriterListener> MediaRecorderPrivateEncoder::listener()
{
    return m_listener;
}

} // namespae WebCore

#endif // ENABLE(MEDIA_RECORDER)
