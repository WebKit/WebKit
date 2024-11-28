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

#include "AudioSampleBufferCompressor.h"
#include "CARingBuffer.h"
#include "CMUtilities.h"
#include "ContentType.h"
#include "MediaRecorderPrivateOptions.h"
#include "MediaRecorderPrivateWriter.h"
#include "MediaSampleAVFObjC.h"
#include "MediaUtilities.h"

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
        else if (!isWebM && (startsWithLettersIgnoringASCIICase(codec, "avc1"_s)))
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
        if (RefPtr protectedThis = weakThis.get()) {
            m_isPaused = true;
            // We can't wait for the pending audio frame anymore, continue with what's available, we will drain the audio decoder instead.
            if (m_pendingAudioFramePromise)
                m_pendingAudioFramePromise->second.reject();
            m_pendingAudioFramePromise.reset();
            m_currentVideoDuration = currentEndTime();

            LOG(MediaStream, "MediaRecorderPrivateEncoder::pause m_currentVideoDuration:%f", m_currentVideoDuration.toDouble());
        }
    });
}

void MediaRecorderPrivateEncoder::resume()
{
    assertIsMainThread();

    queueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }, this] {
        assertIsCurrent(queueSingleton());
        if (RefPtr protectedThis = weakThis.get()) {
            m_isPaused = false;
            m_firstVideoFrameTime.reset();
            if (!hasAudio())
                m_resumeWallTime = MonotonicTime::now();
            m_needKeyFrame = true;
            LOG(MediaStream, "MediaRecorderPrivateEncoder:resume at:%f", m_currentVideoDuration.toDouble());
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
        AudioStreamBasicDescription outputDescription = { };
        outputDescription.mFormatID = m_audioCodec;
        outputDescription.mChannelsPerFrame = description.mChannelsPerFrame;
        outputDescription.mSampleRate = description.mSampleRate;
        m_originalOutputDescription = outputDescription;
    }

    CMFormatDescriptionRef newFormat = nullptr;
    if (auto error = PAL::CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &description, 0, nullptr, 0, nullptr, nullptr, &newFormat)) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateEncoder::audioSamplesAvailable: CMAudioFormatDescriptionCreate failed with %u", error);
        m_hadError = true;
        return;
    }
    m_audioFormatDescription = adoptCF(newFormat);

    if (m_audioCompressor) {
        audioCompressor()->finish();
        m_formatChangedOccurred = true;
    }

    m_audioCompressor = AudioSampleBufferCompressor::create(compressedAudioOutputBufferCallback, this, m_audioCodec, *m_originalOutputDescription);
    if (!m_audioCompressor) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateEncoder::audioSamplesDescriptionChanged: creation of compressor failed");
        m_hadError = true;
        return;
    }

    if (m_audioBitsPerSecond)
        audioCompressor()->setBitsPerSecond(m_audioBitsPerSecond);

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

    auto* absd = PAL::CMAudioFormatDescriptionGetStreamBasicDescription(m_audioFormatDescription.get());
    ASSERT(absd);
    if (!absd) {
        RELEASE_LOG_ERROR(MediaStream, "MediaRecorderPrivateEncoder::audioSamplesAvailable: inconsistent running state");
        m_hadError = true;
        return;
    }

    auto result = WebAudioBufferList::createWebAudioBufferListWithBlockBuffer(*absd, sampleCount);
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

    audioCompressor()->addSampleBuffer(sampleBuffer);
}

void MediaRecorderPrivateEncoder::appendVideoFrame(VideoFrame& frame)
{
    if (m_isStopped)
        return;

    queueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }, this, frame = Ref { frame }, time = MediaTime(m_lastEnqueuedAudioTimeUs.load(), 1000000)]() mutable {
        assertIsCurrent(queueSingleton());
        if (RefPtr protectedThis = weakThis.get()) {
            if (!hasAudio() && !m_resumeWallTime)
                m_resumeWallTime = MonotonicTime::now();
            appendVideoFrame(time, WTFMove(frame));
        }
    });
}

void MediaRecorderPrivateEncoder::appendVideoFrame(const MediaTime& audioTime, Ref<VideoFrame>&& frame)
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
                m_videoEncoder = result.value().moveToUniquePtr();
                m_videoEncoder->setRates(videoBitRate(), 0);
                m_videoEncoderCreationPromise = nullptr;
                return encodePendingVideoFrames();
            }
            return GenericPromise::createAndResolve();
        })->chainTo(WTFMove(producer));
    }

    auto sampleTime = nextVideoFrameTime(audioTime);
    // FIXME: AVAssetWriter errors when we attempt to add a sample with the same time.
    // When we start and audio isn't ready, we may output multiple frame that has a timestamp of 0.
    if (sampleTime <= m_lastEnqueuedRawVideoFrame)
        sampleTime = m_lastEnqueuedRawVideoFrame + MediaTime(1, 1000000);

    ASSERT(m_lastEnqueuedRawVideoFrame <= sampleTime);
    ASSERT(m_lastEnqueuedRawVideoFrame >= m_lastReceivedCompressedVideoFrame);
    m_lastEnqueuedRawVideoFrame = sampleTime;
    m_pendingVideoFrames.append({ WTFMove(frame), sampleTime });
    LOG(MediaStream, "appendVideoFrame:enqueuing raw video frame:%f queue:%zu first:%f last:%f (received audio:%d)", sampleTime.toDouble(), m_pendingVideoFrames.size(), m_pendingVideoFrames.first().second.toDouble(), m_pendingVideoFrames.last().second.toDouble(), !!m_lastEnqueuedAudioTimeUs.load());

    encodePendingVideoFrames();
}

MediaTime MediaRecorderPrivateEncoder::nextVideoFrameTime(const MediaTime& audioTime)
{
    assertIsCurrent(queueSingleton());

    // We take the time before m_firstVideoFrameAudioTime is set so that the first frame will always apepar to have a timestamp of 0 but with a longer duration.
    if (!m_firstVideoFrameTime) {
        m_firstVideoFrameTime = m_currentVideoDuration;
        return *m_firstVideoFrameTime;
    }
    return hasAudio() ? audioTime : currentTime();
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

    if (!audioCompressor() || audioCompressor()->isEmpty())
        return;

    if (!m_audioCompressedFormatDescription) {
        m_audioCompressedFormatDescription = PAL::CMSampleBufferGetFormatDescription(audioCompressor()->getOutputSampleBuffer());
        if (auto result = m_writer->addAudioTrack(m_audioCompressedFormatDescription.get()))
            m_audioTrackIndex = result;
        else {
            RELEASE_LOG_ERROR(MediaStream, "appendAudioFrame: Failed to create audio track");
            return;
        }
        maybeStartWriter();
    }

    if (!m_audioTrackIndex) {
        RELEASE_LOG_ERROR(MediaStream, "enqueueCompressedAudioSampleBuffers: failed following failure to create audio track");
        return;
    }

    while (RetainPtr sampleBlock = audioCompressor()->takeOutputSampleBuffer()) {
        for (Ref sample : MediaSampleAVFObjC::create(sampleBlock.get(), *m_audioTrackIndex)->divide()) {
            if (m_pendingAudioFramePromise && m_pendingAudioFramePromise->first <= sample->presentationEndTime()) {
                m_pendingAudioFramePromise->second.resolve();
                m_pendingAudioFramePromise.reset();
            }
            if (!m_hasStartedAudibleAudioFrame && sample->duration())
                m_hasStartedAudibleAudioFrame = true;
            m_encodedAudioFrames.append(WTFMove(sample));
        }
    }
}

void MediaRecorderPrivateEncoder::maybeStartWriter()
{
    assertIsCurrent(queueSingleton());

    if (m_writerIsStarted)
        return;

    if ((hasAudio() && !m_audioCompressedFormatDescription && !m_audioTrackIndex) || (hasVideo() && !m_videoFormatDescription && !m_videoTrackIndex))
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
        return m_videoEncoder->encode({ WTFMove(frame.first), frame.second.toMicroseconds(), { } }, needVideoKeyframe);
    } };
    VideoEncoder::EncodePromise::all(WTFMove(promises))->chainTo(WTFMove(producer));

    return promise;
}

void MediaRecorderPrivateEncoder::processVideoEncoderActiveConfiguration(const VideoEncoder::Config& config, const VideoEncoderActiveConfiguration& configuration)
{
    assertIsCurrent(queueSingleton());

    ASSERT(!m_videoFormatDescription);
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
    m_videoFormatDescription = createFormatDescriptionFromTrackInfo(videoInfo);
    if (auto result = m_writer->addVideoTrack(m_videoFormatDescription.get(), m_videoTransform))
        m_videoTrackIndex = result;
    else {
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

    ASSERT(m_videoTrackInfo && m_videoFormatDescription);
    m_lastReceivedCompressedVideoFrame = MediaTime(frame.timestamp, 1000000);
    ASSERT(m_lastReceivedCompressedVideoFrame <= m_lastEnqueuedRawVideoFrame);

    MediaSamplesBlock block;
    block.setInfo(m_videoTrackInfo.copyRef());
    block.append({ m_lastReceivedCompressedVideoFrame, m_lastReceivedCompressedVideoFrame, MediaTime::indefiniteTime(), MediaTime::zeroTime(), SharedBuffer::create(WTFMove(frame.data)), frame.isKeyFrame ? MediaSample::SampleFlags::IsSync : MediaSample::SampleFlags::None });
    auto sample = toCMSampleBuffer(WTFMove(block), m_videoFormatDescription.get());
    if (!sample) {
        LOG(MediaStream, "appendVideoFrame: error creation compressed sample");
        return;
    }
    m_encodedVideoFrames.append(MediaSampleAVFObjC::create(sample->get(), *m_videoTrackIndex));
    Ref sampleObjC = m_encodedVideoFrames.last();
    ASSERT(frame.isKeyFrame == sampleObjC->isSync());
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

    Result success = Result::Success;
    while ((!hasVideo() || !m_encodedVideoFrames.isEmpty()) && (!hasAudio() || !m_encodedAudioFrames.isEmpty()) && success == Result::Success)
        success = muxNextFrame();
}

Ref<GenericPromise> MediaRecorderPrivateEncoder::waitForMatchingAudio(const MediaTime& flushTime)
{
    assertIsCurrent(queueSingleton());

    if (!hasAudio() || !hasVideo() || m_isPaused)
        return GenericPromise::createAndResolve(); // Nothing to sync.

    RefPtr<MediaSample> videoFrame;
    for (auto& frame : m_encodedVideoFrames) {
        if (frame->presentationTime() > flushTime)
            break;
        videoFrame = frame.copyRef();
    }
    if (!videoFrame)
        return GenericPromise::createAndResolve();

    if (videoFrame->presentationTime() < m_lastMuxedAudioSampleEndTime)
        return GenericPromise::createAndResolve(); // video frame is directly muxable.

    if (!m_encodedAudioFrames.isEmpty() && Ref { m_encodedAudioFrames.last() }->presentationEndTime() >= videoFrame->presentationTime())
        return GenericPromise::createAndResolve();

    // The audio frame matching the last video frame is still pending in the compressor and requires more data to be produced.
    // It will be resolved once we receive the compressed audio sample in enqueueCompressedAudioSampleBuffers().
    m_pendingAudioFramePromise.emplace(videoFrame->presentationTime(), GenericPromise::Producer());

    LOG(MediaStream, "MediaRecorderPrivateEncoder::waitForMatchingAudio waiting for audio:%f", videoFrame->presentationTime().toDouble());

    return m_pendingAudioFramePromise->second.promise();
}

std::pair<MediaRecorderPrivateEncoder::Result, MediaTime> MediaRecorderPrivateEncoder::flushToEndSegment(const MediaTime& flushTime)
{
    assertIsCurrent(queueSingleton());

    LOG(MediaStream, "MediaRecorderPrivateEncoder::flushToEndSegment(%f): lastAudioReceived:%f audioqueue:%zu first:%f last:%f videoQueue:%zu first:%f (kf:%d) last:%f", flushTime.toDouble(), lastEnqueuedAudioTime().toDouble(), m_encodedAudioFrames.size(), m_encodedAudioFrames.size() ? m_encodedAudioFrames.first()->presentationTime().toDouble() : 0, m_encodedAudioFrames.size() ? m_encodedAudioFrames.last()->presentationTime().toDouble() : 0, m_encodedVideoFrames.size(), m_encodedVideoFrames.size() ? m_encodedVideoFrames.first()->presentationTime().toDouble() : 0, m_encodedVideoFrames.size() ? m_encodedVideoFrames.first()->isSync() : 0, m_encodedVideoFrames.size() ? m_encodedVideoFrames.last()->presentationTime().toDouble() : 0);

    if (hasAudio())
        enqueueCompressedAudioSampleBuffers(); // compressedAudioOutputBufferCallback isn't always called when new frames are available. Force refresh

    if (!m_writerIsStarted || (hasAudio() && !m_hasStartedAudibleAudioFrame))
        return { Result::NotReady, MediaTime::invalidTime() };

    ASSERT(!m_videoEncoderCreationPromise);

    // Find last video keyframe in the queue.
    RefPtr<MediaSample> lastVideoKeyFrame;
    for (auto it = m_encodedVideoFrames.rbegin(); it != m_encodedVideoFrames.rend(); ++it) {
        if ((*it)->isSync()) {
            lastVideoKeyFrame = (*it).copyRef();
            break;
        }
    }

    // Mux all video frames until we reached the end of the queue or we found a keyframe.
    Result success = Result::Success;
    while ((!m_encodedVideoFrames.isEmpty() || !m_encodedAudioFrames.isEmpty()) && success == Result::Success) {
        RefPtr audioFrame = m_encodedAudioFrames.size() ? RefPtr { m_encodedAudioFrames.first().ptr() } : nullptr;
        RefPtr videoFrame = m_encodedVideoFrames.size() ? RefPtr { m_encodedVideoFrames.first().ptr() } : nullptr;
        ASSERT(audioFrame || videoFrame);
        bool takeVideo = videoFrame && (!audioFrame || videoFrame->presentationTime() < audioFrame->presentationTime());
        Ref frame = takeVideo ? videoFrame.releaseNonNull() : audioFrame.releaseNonNull();
        if (takeVideo && frame.ptr() == lastVideoKeyFrame) {
            LOG(MediaStream, "flushToEndSegment: stopping prior video keyframe time:%f", frame->presentationTime().toDouble());
            return { success, frame->presentationTime() };
        }
        // We are about to end the current segment and the next segment needs to start with a keyframe.
        // Don't mux the current audio frame (if any) if the next video frame is a keyframe and is to be displayed
        // while the current audio frame is playing.
        if (!takeVideo && videoFrame) {
            if ((videoFrame->presentationTime() <= frame->presentationEndTime()) && videoFrame == lastVideoKeyFrame) {
                LOG(MediaStream, "flushToEndSegment: stopping prior audio containing video keyframe time:%f", frame->presentationTime().toDouble());
                return { success, frame->presentationTime() };
            }
        }

        // If we don't have any more video frames pending (the next incoming frame will be a keyframe),
        // we write all the audio frames received with a date prior flushTime.
        if (!takeVideo && !videoFrame && frame->presentationTime() > flushTime)
            return { success, frame->presentationTime() };

        success = muxNextFrame();
    };

    return { success, flushTime };
}

void MediaRecorderPrivateEncoder::flushAllEncodedQueues()
{
    assertIsCurrent(queueSingleton());

    LOG(MediaStream, "flushAllEncodedQueues: audioqueue:%zu first:%f last:%f videoQueue:%zu first:%f (kf:%d) last:%f", m_encodedAudioFrames.size(), m_encodedAudioFrames.size() ? m_encodedAudioFrames.first()->presentationTime().toDouble() : 0, m_encodedAudioFrames.size() ? m_encodedAudioFrames.last()->presentationTime().toDouble() : 0, m_encodedVideoFrames.size(), m_encodedVideoFrames.size() ? m_encodedVideoFrames.first()->presentationTime().toDouble() : 0, m_encodedVideoFrames.size() ? m_encodedVideoFrames.first()->isSync() : 0, m_encodedVideoFrames.size() ? m_encodedVideoFrames.last()->presentationTime().toDouble() : 0);

    if (hasAudio())
        enqueueCompressedAudioSampleBuffers(); // compressedAudioOutputBufferCallback isn't always called when new frames are available. Force refresh

    if (!m_writerIsStarted)
        m_writer->allTracksAdded();

    Result success = Result::Success;
    while ((!m_encodedVideoFrames.isEmpty() || !m_encodedAudioFrames.isEmpty()) && success == Result::Success)
        success = muxNextFrame();
}

MediaRecorderPrivateEncoder::Result MediaRecorderPrivateEncoder::muxNextFrame()
{
    assertIsCurrent(queueSingleton());

    // Determine if we should mux a video or audio frame. We favor the audio frame first to satisfy webm muxing requirement:
    // https://www.webmproject.org/docs/container/
    // - Audio blocks that contain the video key frame's timecode SHOULD be in the same cluster as the video key frame block.
    // - Audio blocks that have same absolute timecode as video blocks SHOULD be written before the video blocks.
    RefPtr audioFrame = m_encodedAudioFrames.size() ? RefPtr { m_encodedAudioFrames.first().ptr() } : nullptr;
    RefPtr videoFrame = m_encodedVideoFrames.size() ? RefPtr { m_encodedVideoFrames.first().ptr() } : nullptr;
    ASSERT(audioFrame || videoFrame);
    bool takeVideo = videoFrame && (!audioFrame || videoFrame->presentationTime() < audioFrame->presentationTime());
    Ref frame = takeVideo ? videoFrame.releaseNonNull() : audioFrame.releaseNonNull();

    ASSERT(!takeVideo || !m_nextVideoFrameMuxedShouldBeKeyframe || frame->isSync());

    if (m_formatChangedOccurred && !takeVideo) {
        RetainPtr sample = downcast<MediaSampleAVFObjC>(frame)->sampleBuffer();
        // Writing audio samples requiring an edit list is forbidden by the AVAssetWriterInput when used with fMP4, remove the keys.
        PAL::CMRemoveAttachment(sample.get(), PAL::kCMSampleBufferAttachmentKey_TrimDurationAtStart);
        PAL::CMRemoveAttachment(sample.get(), PAL::kCMSampleBufferAttachmentKey_TrimDurationAtEnd);
    }

    auto result = m_writer->muxFrame(frame, takeVideo ? *m_videoTrackIndex : *m_audioTrackIndex);
    if (result != Result::NotReady) {
        (takeVideo ? m_encodedVideoFrames : m_encodedAudioFrames).removeFirst();
        LOG(MediaStream, "muxNextFrame: wrote %s (kf:%d) frame time:%f-%f (previous:%f) success:%d", takeVideo ? "video" : "audio", frame->isSync(), frame->presentationTime().toDouble(), frame->presentationEndTime().toDouble(), m_lastMuxedSampleStartTime.toDouble(), result == Result::Success);
        ASSERT(frame->presentationTime() >= m_lastMuxedSampleStartTime);
        m_lastMuxedSampleStartTime = frame->presentationTime();
        ASSERT(result == Result::Success);
        if (takeVideo) {
            m_hasMuxedVideoFrameSinceEndSegment = true;
            m_nextVideoFrameMuxedShouldBeKeyframe = false;
        } else {
            m_hasMuxedAudioFrameSinceEndSegment = true;
            m_lastMuxedAudioSampleEndTime = frame->presentationEndTime();
        }
    } else {
        // The MediaRecorderPrivateEncoder performs the correct interleaving of samples and doesn't rely on the writer to do so.
        RELEASE_LOG_ERROR(MediaStream, "muxNextFrame: writing %s frame time:%f not ready", takeVideo ? "video" : "audio", frame->presentationTime().toDouble());
    }

    return result;
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

        // We don't need to wait for the pending audio frame anymore, the imminent call to AudioSampleBufferCompressor::finish() will output all data.
        if (m_pendingAudioFramePromise)
            m_pendingAudioFramePromise->second.reject();
        m_pendingAudioFramePromise.reset();
    });

    m_currentFlushOperations = Ref { m_currentFlushOperations }->whenSettled(queueSingleton(), [protectedThis = Ref { *this }, this] {
        if (RefPtr compressor = audioCompressor())
            compressor->finish();

        {
            Locker locker { m_ringBuffersLock };
            m_ringBuffers.clear();
        }

        return flushPendingData(MediaTime::positiveInfiniteTime())->whenSettled(queueSingleton(), [protectedThis, this] {
            assertIsCurrent(queueSingleton());
            if (m_videoEncoder)
                m_videoEncoder->close();

            flushAllEncodedQueues();

            ASSERT(!m_videoEncoderCreationPromise && m_pendingVideoFrames.isEmpty() && m_encodedVideoFrames.isEmpty());
            ASSERT(m_encodedAudioFrames.isEmpty() && (!m_audioCompressor || audioCompressor()->isEmpty()));

            m_writerIsClosed = true;

            LOG(MediaStream, "FlushPendingData::close writer time:%f", currentEndTime().toDouble());

            return m_writer->close(currentEndTime());
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
    promises.reserveInitialCapacity(size_t(!!m_videoEncoder) + size_t(!!m_audioCompressor) + 1);
    promises.append(encodePendingVideoFrames());
    if (m_videoEncoder)
        promises.append(m_videoEncoder->flush());
    if (RefPtr compressor = audioCompressor())
        promises.append(m_isPaused ? compressor->drain() : compressor->flush());

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
        if (RefPtr protectedThis = weakThis.get()) {
            if (result) {
                auto [result, endMuxedTime] = flushToEndSegment(currentTime);
                // Start a new segment if:
                // 1: We aren't stopped (all frames will be flushed and written upon the promise being resolved) and
                // 2: We have muxed data for all tracks (Ending the current segment before frames of all kind have been amended results in a broken file) and
                // 3: We have accumulated more than m_minimumSegmentDuration of content or
                // 4: We are paused.
                if (!m_isStopped && hasMuxedDataSinceEndSegment() && result == Result::Success
                    && ((endMuxedTime - m_startSegmentTime >= m_minimumSegmentDuration) || m_isPaused)) {
                    LOG(MediaStream, "FlushPendingData::forceNewSegment at time:%f", endMuxedTime.toDouble());
                    m_writer->forceNewSegment(endMuxedTime);
                    m_nextVideoFrameMuxedShouldBeKeyframe = true;
                    m_startSegmentTime = endMuxedTime;
                    m_hasMuxedAudioFrameSinceEndSegment = false;
                    m_hasMuxedVideoFrameSinceEndSegment = false;
                }
            }
            m_pendingFlush--;
        }
        return GenericPromise::createAndResolve();
    });
}

MediaTime MediaRecorderPrivateEncoder::currentTime() const
{
    assertIsCurrent(queueSingleton());

    if (hasAudio())
        return lastEnqueuedAudioTime();

    return MediaTime::createWithSeconds(MonotonicTime::now() - m_resumeWallTime.value_or(MonotonicTime::now())) + m_currentVideoDuration;
}

MediaTime MediaRecorderPrivateEncoder::currentEndTime() const
{
    assertIsCurrent(queueSingleton());

    if (hasAudio())
        return MediaTime(m_currentAudioTimeUs.load(), 1000000);

    return MediaTime::createWithSeconds(MonotonicTime::now() - m_resumeWallTime.value_or(MonotonicTime::now())) + m_currentVideoDuration;
}

RefPtr<AudioSampleBufferCompressor> MediaRecorderPrivateEncoder::audioCompressor() const
{
    assertIsCurrent(queueSingleton());

    return m_audioCompressor;
}

Ref<MediaRecorderPrivateWriterListener> MediaRecorderPrivateEncoder::listener()
{
    return m_listener;
}

} // namespae WebCore

#endif // ENABLE(MEDIA_RECORDER)
