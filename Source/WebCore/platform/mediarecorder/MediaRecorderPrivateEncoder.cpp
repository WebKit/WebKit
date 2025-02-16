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
#include "MediaUtilities.h"
#include "WebAudioBufferList.h"

#include <CoreAudio/CoreAudioTypes.h>
#include <CoreMedia/CMTime.h>
#include <mutex>
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
    if (!writer || !encoder->initialize(options, makeUniqueRefFromNonNullUniquePtr(WTFMove(writer))))
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

    m_writer = writer.moveToUniquePtr();

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
    }

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

void MediaRecorderPrivateEncoder::audioSamplesDescriptionChanged(const AudioStreamBasicDescription& description)
{
    assertIsCurrent(queueSingleton());

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

    updateCurrentRingBufferIfNeeded();
}

void MediaRecorderPrivateEncoder::addRingBuffer(const AudioStreamDescription& description)
{
    auto asbd = *std::get<const AudioStreamBasicDescription*>(description.platformDescription().description);
    Locker locker { m_ringBuffersLock };
    m_ringBuffers.append(InProcessCARingBuffer::allocate(asbd, description.sampleRate() * 2)); // allocate 2s of buffer.
    queueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }, description = asbd] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->audioSamplesDescriptionChanged(description);
    });
}

void MediaRecorderPrivateEncoder::writeDataToRingBuffer(AudioBufferList* list, size_t sampleCount, size_t totalSampleCount)
{
    Locker locker { m_ringBuffersLock };
    if (m_ringBuffers.isEmpty() || !m_ringBuffers.last())
        return;
    m_ringBuffers.last()->store(list, sampleCount, totalSampleCount);
}

void MediaRecorderPrivateEncoder::updateCurrentRingBufferIfNeeded()
{
    assertIsCurrent(queueSingleton());

    Locker locker { m_ringBuffersLock };
    if (m_currentRingBuffer) {
        ASSERT(m_ringBuffers.size() > 1);
        m_ringBuffers.removeFirst();
    }
    m_currentRingBuffer = m_ringBuffers.first().get();
    if (!m_currentRingBuffer) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateEncoder::audioSamplesDescriptionChanged: out of memory error occurred");
        m_hadError = true;
    }
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
    auto [list, block] = WTFMove(*result);

    ASSERT(m_currentRingBuffer);
    m_currentRingBuffer->fetch(list->list(), sampleCount, totalSampleCount);

    CMSampleBufferRef sampleBuffer = nullptr;
    if (auto error = PAL::CMAudioSampleBufferCreateWithPacketDescriptions(kCFAllocatorDefault, block.get(), true, nullptr, nullptr, m_audioFormatDescription.get(), sampleCount, PAL::toCMTime(time), nullptr, &sampleBuffer)) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateEncoder::audioSamplesAvailable: CMAudioSampleBufferCreateWithPacketDescriptions failed with error %d", error);
        m_hadError = true;
        return;
    }
    RetainPtr sample = adoptCF(sampleBuffer);

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
            auto nextVideoFrameTime = hasAudio() ? audioTime : MediaTime::createWithSeconds((now - m_currentVideoSegmentStartTime.value_or(now)) + Seconds::fromMicroseconds(m_previousSegmentVideoDurationUs));
            if (!m_currentVideoSegmentStartTime) {
                m_currentVideoSegmentStartTime = now;
                // We take the time before m_previousSegmentVideoDurationUs is set so that the first frame will always appear to have a timestamp of 0 but with a longer duration.
                nextVideoFrameTime = MediaTime(m_previousSegmentVideoDurationUs, 1000000);
            }
            m_lastRawVideoFrameReceived = nextVideoFrameTime;
            appendVideoFrame(nextVideoFrameTime, WTFMove(frame));
        }
    });
}

void MediaRecorderPrivateEncoder::appendVideoFrame(MediaTime sampleTime, Ref<VideoFrame>&& frame)
{
    assertIsCurrent(queueSingleton());

    if (!m_firstVideoFrameProcessed) {
        m_firstVideoFrameProcessed = true;

        if (frame->rotation() != VideoFrame::Rotation::None || frame->isMirrored()) {
            m_videoTransform = CGAffineTransformMakeRotation(static_cast<int>(frame->rotation()) * M_PI / 180);
            if (frame->isMirrored())
                m_videoTransform = CGAffineTransformScale(*m_videoTransform, -1, 1);
        }
        VideoEncoder::Config config { static_cast<uint64_t>(frame->presentationSize().width()), static_cast<uint64_t>(frame->presentationSize().height()), false, videoBitRate() };

        Ref promise = VideoEncoder::create(codecStringForMediaVideoCodecId(m_videoCodec), config, [weakThis = ThreadSafeWeakPtr { *this }, config](auto&& configuration) mutable {
            queueSingleton().dispatch([weakThis, config = WTFMove(config), configuration] {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->processVideoEncoderActiveConfiguration(config, WTFMove(configuration));
            });
        }, [weakThis = ThreadSafeWeakPtr { *this }](auto&& frame) {
            queueSingleton().dispatch([weakThis, frame = WTFMove(frame)]() mutable {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->enqueueCompressedVideoFrame(WTFMove(frame));
            });
        });
        GenericNonExclusivePromise::Producer producer;
        m_videoEncoderCreationPromise = producer.promise();
        promise->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, this](auto&& result) {
            assertIsCurrent(queueSingleton());
            if (RefPtr protectedThis = weakThis.get(); protectedThis && result) {
                m_videoEncoder = WTFMove(*result);
                Ref { *m_videoEncoder }->setRates(videoBitRate(), 0);
                m_videoEncoderCreationPromise = nullptr;
                return encodePendingVideoFrames();
            }
            return GenericPromise::createAndResolve();
        })->chainTo(WTFMove(producer));
    }

    // FIXME: AVAssetWriter errors when we attempt to add a sample with the same time.
    // When we start and audio isn't ready, we may output multiple frame that has a timestamp of 0.
    if (sampleTime <= m_lastEnqueuedRawVideoFrame)
        sampleTime = m_lastEnqueuedRawVideoFrame + MediaTime(1, 1000000);

    m_lastEnqueuedRawVideoFrame = sampleTime;
    m_pendingVideoFrames.append({ WTFMove(frame), sampleTime });
    LOG(MediaStream, "appendVideoFrame:enqueuing raw video frame:%f queue:%zu first:%f last:%f (received audio:%d)", sampleTime.toDouble(), m_pendingVideoFrames.size(), m_pendingVideoFrames.first().second.toDouble(), m_pendingVideoFrames.last().second.toDouble(), !!m_lastEnqueuedAudioTimeUs.load());

    encodePendingVideoFrames();
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
        return m_data.take();
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
            m_audioCompressedAudioInfo->trackID = *result;
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

Ref<GenericPromise> MediaRecorderPrivateEncoder::encodePendingVideoFrames()
{
    assertIsCurrent(queueSingleton());

    if (m_pendingVideoFrames.isEmpty())
        return GenericPromise::createAndResolve();

    GenericPromise::Producer producer;
    Ref promise = producer.promise();

    if (m_videoEncoderCreationPromise) {
        RefPtr { m_videoEncoderCreationPromise }->chainTo(WTFMove(producer));
        return promise;
    }

    Vector<Ref<VideoEncoder::EncodePromise>> promises { m_pendingVideoFrames.size() , [&](size_t) {
        assertIsCurrent(queueSingleton());
        auto frame = m_pendingVideoFrames.takeFirst();
        bool needVideoKeyframe = false;
        // Ensure we create GOP at regular interval.
        if (((frame.second - m_lastVideoKeyframeTime) >= m_maxGOPDuration) || m_needKeyFrame) {
            needVideoKeyframe = true;
            m_lastVideoKeyframeTime = frame.second;
            m_needKeyFrame = false;
        }
        LOG(MediaStream, "encodePendingVideoFrames:encoding video frame:%f (us:%lld) kf:%d", frame.second.toDouble(), frame.second.toMicroseconds(), needVideoKeyframe);
        return Ref { *m_videoEncoder }->encode({ WTFMove(frame.first), frame.second.toMicroseconds(), { } }, needVideoKeyframe);
    } };
    VideoEncoder::EncodePromise::all(WTFMove(promises))->chainTo(WTFMove(producer));

    return promise;
}

void MediaRecorderPrivateEncoder::processVideoEncoderActiveConfiguration(const VideoEncoder::Config& config, const VideoEncoderActiveConfiguration& configuration)
{
    assertIsCurrent(queueSingleton());

    Ref videoInfo = VideoInfo::create();
    if (configuration.visibleWidth && configuration.visibleHeight)
        videoInfo->size = { static_cast<float>(*configuration.visibleWidth), static_cast<float>(*configuration.visibleHeight) };
    else
        videoInfo->size = { static_cast<float>(config.width), static_cast<float>(config.height) };
    if (configuration.displayWidth && configuration.displayHeight)
        videoInfo->displaySize = { static_cast<float>(*configuration.displayWidth), static_cast<float>(*configuration.displayHeight) };
    else
        videoInfo->displaySize = { static_cast<float>(config.width), static_cast<float>(config.height) };
    if (configuration.description)
        videoInfo->atomData = SharedBuffer::create(*configuration.description);
    if (configuration.colorSpace)
        videoInfo->colorSpace = *configuration.colorSpace;
    videoInfo->codecName = m_videoCodec;
    m_videoTrackInfo = videoInfo.copyRef();
    if (auto result = m_writer->addVideoTrack(Ref { *m_videoTrackInfo }, m_videoTransform)) {
        m_videoTrackIndex = result;
        m_videoTrackInfo->trackID = *m_videoTrackIndex;
    } else {
        RELEASE_LOG_ERROR(MediaStream, "appendVideoFrame: Failed to create video track");
        return;
    }
    maybeStartWriter();
    if (configuration.codec.length()) {
        callOnMainThread([weakThis = ThreadSafeWeakPtr { *this }, codec = configuration.codec.isolatedCopy()]() mutable {
            if (RefPtr protectedThis = weakThis.get()) {
                assertIsMainThread();
                protectedThis->m_videoCodecMimeType = WTFMove(codec);
                protectedThis->generateMIMEType();
            }
        });
    }
    encodePendingVideoFrames();
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
    m_encodedVideoFrames.append(makeUniqueRef<MediaSamplesBlock>(m_videoTrackInfo.get(), WTFMove(vector)));
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

    LOG(MediaStream, "MediaRecorderPrivateEncoder::flushToEndSegment(%f): lastAudioReceived:%f audioqueue:%zu first:%f last:%f videoQueue:%zu first:%f (kf:%d) last:%f", flushTime.toDouble(), lastEnqueuedAudioTime().toDouble(), m_encodedAudioFrames.size(), m_encodedAudioFrames.size() ? m_encodedAudioFrames.first()->presentationTime().toDouble() : 0, m_encodedAudioFrames.size() ? m_encodedAudioFrames.last()->presentationTime().toDouble() : 0, m_encodedVideoFrames.size(), m_encodedVideoFrames.size() ? m_encodedVideoFrames.first()->presentationTime().toDouble() : 0, m_encodedVideoFrames.size() ? m_encodedVideoFrames.first()->isSync() : 0, m_encodedVideoFrames.size() ? m_encodedVideoFrames.last()->presentationTime().toDouble() : 0);

    if (hasAudio())
        enqueueCompressedAudioSampleBuffers(); // compressedAudioOutputBufferCallback isn't always called when new frames are available. Force refresh

    if (!m_writerIsStarted || (hasAudio() && !m_hasStartedAudibleAudioFrame))
        return MediaTime::invalidTime();

    ASSERT(!m_videoEncoderCreationPromise);

    // Find last video keyframe in the queue.
    MediaSamplesBlock* lastVideoKeyFrame = nullptr;
    for (auto it = m_encodedVideoFrames.rbegin(); it != m_encodedVideoFrames.rend(); ++it) {
        if ((*it)->isSync()) {
            lastVideoKeyFrame = it->ptr();
            break;
        }
    }

    // Mux all video frames until we reached the end of the queue or we found a keyframe.
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
        // We are about to end the current segment and the next segment needs to start with a keyframe.
        // Don't mux the current audio frame (if any) if the next video frame is a keyframe and is to be displayed
        // while the current audio frame is playing.
        if (!takeVideo && videoFrame) {
            if ((videoFrame->presentationTime() <= frame.presentationEndTime()) && videoFrame == lastVideoKeyFrame) {
                LOG(MediaStream, "flushToEndSegment: stopping prior audio containing video keyframe time:%f", frame.presentationTime().toDouble());
                return frame.presentationTime();
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

    return flushTime;
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

    ASSERT(!takeVideo || !m_nextVideoFrameMuxedShouldBeKeyframe || frame->isSync());

    ASSERT(frame->presentationTime() >= m_lastMuxedSampleStartTime);
    if (frame->presentationTime() < m_lastMuxedSampleStartTime)
        RELEASE_LOG_ERROR(MediaStream, "interleaveAndEnqueueNextFrame: added %s (kf:%d) frame time:%f-%f (previous:%f type:%s) size:%zu with error", takeVideo ? "video" : "audio", frame->isSync(), frame->presentationTime().toDouble(), frame->presentationEndTime().toDouble(), m_lastMuxedSampleStartTime.toDouble(), m_lastMuxedSampleIsVideo ? "video" : "audio" , m_interleavedFrames.size());
    else
        LOG(MediaStream, "interleaveAndEnqueueNextFrame: added %s (kf:%d) frame time:%f-%f (previous:%f type:%s) size:%zu", takeVideo ? "video" : "audio", frame->isSync(), frame->presentationTime().toDouble(), frame->presentationEndTime().toDouble(), m_lastMuxedSampleStartTime.toDouble(), m_lastMuxedSampleIsVideo ? "video" : "audio" , m_interleavedFrames.size());
    m_lastMuxedSampleStartTime = frame->presentationTime();
    m_lastMuxedSampleIsVideo = takeVideo;
    if (takeVideo) {
        m_hasMuxedVideoFrameSinceEndSegment = true;
        m_nextVideoFrameMuxedShouldBeKeyframe = false;
    } else {
        m_hasMuxedAudioFrameSinceEndSegment = true;
        m_lastMuxedAudioSampleEndTime = frame->presentationEndTime();
    }
    m_interleavedFrames.append(WTFMove(frame));

    return;
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

        {
            Locker locker { m_ringBuffersLock };
            m_ringBuffers.clear();
        }

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

void MediaRecorderPrivateEncoder::fetchData(CompletionHandler<void(RefPtr<FragmentedSharedBuffer>&&, double)>&& completionHandler)
{
    assertIsMainThread();

    m_currentFlushOperations = Ref { m_currentFlushOperations }->whenSettled(queueSingleton(), [protectedThis = Ref { *this }, this, completionHandler = WTFMove(completionHandler)]() mutable {
        auto currentTime = this->currentTime();
        return flushPendingData(currentTime)->whenSettled(queueSingleton(), [protectedThis, this, completionHandler = WTFMove(completionHandler), currentTime]() mutable {
            assertIsCurrent(queueSingleton());
            Ref data = takeData();
            LOG(MediaStream, "fetchData::returning data:%zu timeCode:%f time:%f", data->size(), m_timeCode, currentTime.toDouble());
            callOnMainThread([completionHandler = WTFMove(completionHandler), data, timeCode = m_timeCode]() mutable {
                completionHandler(WTFMove(data), timeCode);
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

    m_needKeyFrame = true;

    LOG(MediaStream, "MediaRecorderPrivateEncoder::FlushPendingData upTo:%f", currentTime.toDouble());

    Vector<Ref<GenericPromise>> promises;
    promises.reserveInitialCapacity(size_t(!!m_videoEncoder) + size_t(!!m_audioConverter) + 1);
    promises.append(encodePendingVideoFrames());
    if (m_videoEncoder)
        promises.append(Ref { *m_videoEncoder }->flush());
    if (RefPtr converter = audioConverter())
        promises.append(converter->flush());

    ASSERT(!m_pendingFlush, "flush are serialized");
    m_pendingFlush++;

    return GenericPromise::all(WTFMove(promises))->whenSettled(queueSingleton(), [weakThis = ThreadSafeWeakPtr { *this }, this, currentTime] {
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

        // Write a new segment if:
        // 1: We aren't stopped (all frames will be flushed and written upon the promise being resolved) and
        // 2: We have muxed data for all tracks (Ending the current segment before frames of all kind have been amended results in a broken file) and
        // 3: We have accumulated more than m_minimumSegmentDuration of content or
        // 4: We are paused.
        if (!m_isStopped && hasMuxedDataSinceEndSegment() && result
            && ((endMuxedTime - m_startSegmentTime >= m_minimumSegmentDuration) || m_isPaused)) {
            LOG(MediaStream, "FlushPendingData::forceNewSegment at time:%f", endMuxedTime.toDouble());
            m_nextVideoFrameMuxedShouldBeKeyframe = true;
            m_startSegmentTime = endMuxedTime;
            m_hasMuxedAudioFrameSinceEndSegment = false;
            m_hasMuxedVideoFrameSinceEndSegment = false;
            GenericPromise::Producer producer;
            Ref promise = producer.promise();
            m_writer->writeFrames(std::exchange(m_interleavedFrames, { }), endMuxedTime)->chainTo(WTFMove(producer));
            return promise;
        }
        return GenericPromise::createAndResolve();
    });
}

MediaTime MediaRecorderPrivateEncoder::currentTime() const
{
    assertIsCurrent(queueSingleton());

    if (hasAudio())
        return lastEnqueuedAudioTime();

    auto currentDuration = MediaTime::createWithSeconds(Seconds::fromMicroseconds(m_previousSegmentVideoDurationUs));

    if (!m_currentVideoSegmentStartTime)
        return currentDuration;
    return MediaTime::createWithSeconds(MonotonicTime::now() - *m_currentVideoSegmentStartTime) + currentDuration;
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
