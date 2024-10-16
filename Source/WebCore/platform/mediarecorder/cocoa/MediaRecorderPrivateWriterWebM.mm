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

#import "config.h"
#import "MediaRecorderPrivateWriterWebM.h"

#if ENABLE(MEDIA_RECORDER_WEBM)

#import "AudioSampleBufferCompressor.h"
#import "AudioStreamDescription.h"
#import "ContentType.h"
#import "Logging.h"
#import "MediaRecorderPrivate.h"
#import "MediaRecorderPrivateOptions.h"
#import "MediaUtilities.h"
#import "PlatformAudioData.h"
#import "SharedBuffer.h"
#import "VideoFrame.h"
#import "WebMAudioUtilitiesCocoa.h"
#import <CoreMedia/CMTime.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <webm/mkvmuxer/mkvmuxer.h>
#import <wtf/CompletionHandler.h>
#import <wtf/MainThreadDispatcher.h>
#import <wtf/NativePromise.h>

#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

class MediaRecorderPrivateWriterWebMDelegate : public mkvmuxer::IMkvWriter {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit MediaRecorderPrivateWriterWebMDelegate(MediaRecorderPrivateWriterWebM& parent)
        : m_parent(parent)
    {
        m_segment.Init(this);
        m_segment.set_mode(mkvmuxer::Segment::kFile);
        m_segment.OutputCues(true);
        auto* info = m_segment.GetSegmentInfo();
        info->set_writing_app("WebKit");
        info->set_muxing_app("WebKit");
    }

    // mkvmuxer::IMkvWriter:
    mkvmuxer::int32 Write(const void* buf, mkvmuxer::uint32 len) final
    {
        protectedParent()->appendData({ reinterpret_cast<const uint8_t*>(buf), len });
        m_position += len;
        return 0;
    }
    int64_t Position() const final { return m_position; }
    bool Seekable() const final { return false; }
    int32_t Position(int64_t) final { return -1; }
    void ElementStartNotify(uint64_t, int64_t) final { }

    uint8_t addAudioTrack(const AudioStreamBasicDescription& asbd, const char* codec)
    {
        auto trackIndex = m_segment.AddAudioTrack(asbd.mSampleRate, asbd.mChannelsPerFrame, 0);
        ASSERT(trackIndex);
        auto* audioTrack = reinterpret_cast<mkvmuxer::AudioTrack*>(m_segment.GetTrackByNumber(trackIndex));
        ASSERT(audioTrack);
        audioTrack->set_bit_depth(32u);
        audioTrack->set_codec_id(codec);
        auto opusHeader = createOpusPrivateData(asbd);
        audioTrack->SetCodecPrivate(opusHeader.data(), opusHeader.size());
        return trackIndex;
    }

    uint8_t addVideoTrack(uint32_t width, uint32_t height, const char* codec)
    {
        auto trackIndex = m_segment.AddVideoTrack(width, height, 0);
        ASSERT(trackIndex);
        auto* videoTrack = reinterpret_cast<mkvmuxer::VideoTrack*>(m_segment.GetTrackByNumber(trackIndex));
        ASSERT(videoTrack);
        videoTrack->set_codec_id(codec);
        return trackIndex;
    }

    bool addFrame(const std::span<const uint8_t>& data, uint8_t trackIndex, uint64_t timeNs, bool keyframe)
    {
        m_hasAddedFrame = true;
        return m_segment.AddFrame(data.data(), data.size(), trackIndex, timeNs, keyframe);
    }

    void forceNewClusterOnNextFrame()
    {
        m_segment.ForceNewClusterOnNextFrame();
    }

    void finalize()
    {
        m_segment.Finalize();
    }

    bool hasAddedFrame() const { return m_hasAddedFrame; }

    Ref<MediaRecorderPrivateWriterWebM> protectedParent() const { return RefPtr { m_parent.get() }.releaseNonNull(); }

private:
    ThreadSafeWeakPtr<MediaRecorderPrivateWriterWebM> m_parent;
    mkvmuxer::Segment m_segment;
    int64_t m_position { 0 };
    bool m_hasAddedFrame { false };
};

Ref<MediaRecorderPrivateWriter> MediaRecorderPrivateWriterWebM::create(bool hasAudio, bool hasVideo)
{
    return adoptRef(*new MediaRecorderPrivateWriterWebM(hasAudio, hasVideo));
}

static const char* mkvCodeIcForMediaVideoCodecId(FourCharCode codec)
{
    switch (codec) {
    case 'vp08': return mkvmuxer::Tracks::kVp8CodecId;
    case kCMVideoCodecType_VP9: return mkvmuxer::Tracks::kVp9CodecId;
    case kCMVideoCodecType_AV1: return mkvmuxer::Tracks::kAv1CodecId;
    case kAudioFormatOpus: return mkvmuxer::Tracks::kOpusCodecId;
    default:
        ASSERT_NOT_REACHED("Unsupported codec");
        return "";
    }
}

static String codecStringForMediaVideoCodecId(FourCharCode codec)
{
    switch (codec) {
    case 'vp08': return "vp8"_s;
    case kCMVideoCodecType_VP9: return "vp09.00.10.08"_s; // vp9, profile 0, level 1.0, 8 bits, "VideoRange"
    case kCMVideoCodecType_AV1: return "av01.0.01M.08"_s; // av01, "Main", "Level_2_1", "Main", 8 bits, "VideoRange";
    case kAudioFormatOpus: return "opus"_s;
    default:
        ASSERT_NOT_REACHED("Unsupported codec");
        return ""_s;
    }
}

MediaRecorderPrivateWriterWebM::MediaRecorderPrivateWriterWebM(bool hasAudio, bool hasVideo)
    : m_hasAudio(hasAudio)
    , m_hasVideo(hasVideo)
    , m_delegate(makeUniqueRef<MediaRecorderPrivateWriterWebMDelegate>(*this))
    , m_workQueue(WorkQueue::create("com.apple.MediaRecorderPrivateWriterWebM"_s))
{
}

MediaRecorderPrivateWriterWebM::~MediaRecorderPrivateWriterWebM()
{
    ASSERT(isMainThread());
}

void MediaRecorderPrivateWriterWebM::close()
{
    stopRecording();
}

void MediaRecorderPrivateWriterWebM::compressedAudioOutputBufferCallback(void* mediaRecorderPrivateWriter, CMBufferQueueTriggerToken)
{
    LOG(MediaStream, "compressedAudioOutputBufferCallback called");
    // We can only be called from the CoreMedia callback if we are still alive.
    RefPtr writer = static_cast<MediaRecorderPrivateWriterWebM*>(mediaRecorderPrivateWriter);

    writer->protectedWorkQueue()->dispatch([weakWriter = ThreadSafeWeakPtr { *writer }] {
        if (auto strongWriter = weakWriter.get()) {
            strongWriter->enqueueCompressedAudioSampleBuffers();
            strongWriter->partiallyFlushEncodedQueues();
        }
    });
}

bool MediaRecorderPrivateWriterWebM::initialize(const MediaRecorderPrivateOptions& options)
{
    assertIsMainThread();

    m_videoCodec = 'vp08';
    m_audioCodec = kAudioFormatOpus;
    ContentType mimeType(options.mimeType);

    for (auto& codec : mimeType.codecs()) {
        if (codec.startsWith("vp09"_s) || equal(codec, "vp9"_s) || equal(codec, "vp9.0"_s))
            m_videoCodec = kCMVideoCodecType_VP9;
        else if (codec.startsWith("vp08"_s) || equal(codec, "vp8"_s) || equal(codec, "vp8.0"_s))
            m_videoCodec = 'vp08';
        else if (codec == "opus"_s)
            m_audioCodec = kAudioFormatOpus;
        else
            return false; // unsupported codec.
    }

    if (m_hasAudio) {
        m_audioCompressor = AudioSampleBufferCompressor::create(compressedAudioOutputBufferCallback, this, m_audioCodec);
        if (!m_audioCompressor)
            return false;
        if (options.audioBitsPerSecond) {
            m_audioBitsPerSecond = *options.audioBitsPerSecond;
            RefPtr { m_audioCompressor }->setBitsPerSecond(m_audioBitsPerSecond);
        }
    }

    if (options.videoBitsPerSecond)
        m_videoBitsPerSecond = *options.videoBitsPerSecond;

    m_mimeType = makeString(m_hasVideo ? "video/webm"_s : "audio/webm"_s, "; codecs=\""_s, m_hasVideo ? codecStringForMediaVideoCodecId(m_videoCodec) : ""_s, m_hasVideo && m_hasAudio ? ","_s : ""_s, m_hasAudio ? codecStringForMediaVideoCodecId(m_audioCodec) : ""_s, "\""_s);
    return true;
}

void MediaRecorderPrivateWriterWebM::enqueueCompressedAudioSampleBuffers()
{
    assertIsCurrent(workQueue());

    ASSERT(m_hasAudio);

    if (!m_audioCompressor || RefPtr { m_audioCompressor }->isEmpty()) {
        LOG(MediaStream, "enqueueCompressedAudioSampleBuffers: empty queue");
        return;
    }

    if (!m_audioFormatDescription) {
        m_audioFormatDescription = PAL::CMSampleBufferGetFormatDescription(RefPtr { m_audioCompressor }->getOutputSampleBuffer());

        const AudioStreamBasicDescription* const asbd = PAL::CMAudioFormatDescriptionGetStreamBasicDescription(m_audioFormatDescription.get());
        if (!asbd)
            return;

        m_audioTrackIndex = m_delegate->addAudioTrack(*asbd, mkvCodeIcForMediaVideoCodecId(m_audioCodec));
        if (!m_audioTrackIndex)
            return;

        maybeStartWriting();
    }

    if (!m_hasStartedWriting)
        return;

    while (RetainPtr sampleBlock = RefPtr { m_audioCompressor }->takeOutputSampleBuffer()) {
        PAL::CMSampleBufferCallBlockForEachSample(sampleBlock.get(), [&] (CMSampleBufferRef sampleBuffer, CMItemCount) -> OSStatus {
            assertIsCurrent(workQueue());
            m_encodedAudioFrames.append(sampleBuffer);
            LOG(MediaStream, "enqueueCompressedAudioSampleBuffers:Receiving compressed audio frame: queue:%zu first:%f last:%f", m_encodedAudioFrames.size(), sampleTime(m_encodedAudioFrames.first()).value(), sampleTime(m_encodedAudioFrames.last()).value());
            return noErr;
        });
    }
}

void MediaRecorderPrivateWriterWebM::maybeStartWriting()
{
    assertIsCurrent(workQueue());

    m_hasStartedWriting = (!m_hasAudio || m_audioFormatDescription) && (!m_hasVideo || m_videoEncoder);
}

void MediaRecorderPrivateWriterWebM::appendVideoFrame(VideoFrame& frame)
{
    if (m_isStopped)
        return;

    protectedWorkQueue()->dispatch([weakThis = ThreadSafeWeakPtr { *this }, frame = Ref { frame }]() mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->appendVideoFrame(WTFMove(frame));
    });
}

void MediaRecorderPrivateWriterWebM::appendVideoFrame(Ref<VideoFrame>&& frame)
{
    assertIsCurrent(workQueue());

    if (!m_firstVideoFrameProcessed) {
        m_firstVideoFrameProcessed = true;

        m_resumedVideoTime = MonotonicTime::now();

        VideoEncoder::Config config { static_cast<uint64_t>(frame->presentationSize().width()), static_cast<uint64_t>(frame->presentationSize().height()), false, m_videoBitsPerSecond };

        m_videoTrackIndex = m_delegate->addVideoTrack(config.width, config.height, mkvCodeIcForMediaVideoCodecId(m_videoCodec));

        Ref promise = VideoEncoder::create(codecStringForMediaVideoCodecId(m_videoCodec), config, [](auto&&) { }, [weakThis = ThreadSafeWeakPtr { *this }, workQueue = protectedWorkQueue()](auto&& frame) {
            workQueue->dispatch([weakThis, frame = WTFMove(frame)]() mutable {
                if (RefPtr protectedThis = weakThis.get()) {
                    assertIsCurrent(protectedThis->workQueue());
                    protectedThis->m_lastReceivedCompressedVideoFrame = Seconds::fromMicroseconds(frame.timestamp);
                    ASSERT(protectedThis->m_lastReceivedCompressedVideoFrame <= protectedThis->m_lastEnqueuedRawVideoFrame);
                    protectedThis->m_encodedVideoFrames.append(WTFMove(frame));
                    LOG(MediaStream, "appendVideoFrame:Receiving compressed video frame: queue:%zu first:%f last:%f", protectedThis->m_encodedVideoFrames.size(), protectedThis->m_encodedVideoFrames.first().timestamp / 1000000.0, protectedThis->m_encodedVideoFrames.last().timestamp / 1000000.0);
                    protectedThis->partiallyFlushEncodedQueues();
                }
            });
        });
        promise->whenSettled(protectedWorkQueue(), [weakThis = ThreadSafeWeakPtr { *this }](auto&& result) {
            if (RefPtr protectedThis = weakThis.get()) {
                assertIsCurrent(protectedThis->workQueue());
                protectedThis->m_videoEncoder = result.value().moveToUniquePtr();
                protectedThis->m_videoEncoder->setRates(protectedThis->m_videoBitsPerSecond, 0);
                protectedThis->maybeStartWriting();
                protectedThis->encodePendingVideoFrames();
            }
        });
    }

    // We take the time before m_firstVideoFrameAudioTime is set so that the first frame will always apepar to have a timestamp of 0 but with a longer duration.
    auto sampleTime = m_firstVideoFrameAudioTime ? nextVideoFrameTime() : resumeVideoTime();

    if (!m_firstVideoFrameAudioTime)
        m_firstVideoFrameAudioTime = sampleTime;

    ASSERT(m_lastEnqueuedRawVideoFrame <= sampleTime);
    ASSERT(m_lastEnqueuedRawVideoFrame >= m_lastReceivedCompressedVideoFrame);
    m_lastEnqueuedRawVideoFrame = sampleTime;
    m_pendingVideoFrames.append({ WTFMove(frame), sampleTime });
    LOG(MediaStream, "appendVideoFrame:enqueuing raw video frame:%f queue:%zu first:%f last:%f", sampleTime.value(), m_pendingVideoFrames.size(), m_pendingVideoFrames.first().second.value(), m_pendingVideoFrames.last().second.value());

    encodePendingVideoFrames();
}

Seconds MediaRecorderPrivateWriterWebM::nextVideoFrameTime() const
{
    assertIsCurrent(workQueue());

    ASSERT(!m_hasAudio || m_firstVideoFrameAudioTime);
    auto now = MonotonicTime::now();
    auto frameTime = now - m_resumedVideoTime;
    LOG(MediaStream, "nextVideoFrameTime: frameTime:%f nextVideoFrameTime:%f", frameTime.value(), (frameTime + *m_firstVideoFrameAudioTime).value());
    frameTime = frameTime + *m_firstVideoFrameAudioTime;
    // Take the max value from m_lastMuxedSampleTime to handle the occasional wall clock time drift and ensure
    // muxed frames times are always monotonically increasing.
    return std::max(frameTime, m_lastMuxedSampleTime);
}

Seconds MediaRecorderPrivateWriterWebM::resumeVideoTime() const
{
    assertIsCurrent(workQueue());

    return m_hasAudio ? Seconds { m_resumedAudioTime.toDouble() } : m_currentVideoDuration;
}

Seconds MediaRecorderPrivateWriterWebM::sampleTime(const RetainPtr<CMSampleBufferRef>& sample) const
{
    return Seconds { PAL::CMTimeGetSeconds(PAL::CMSampleBufferGetPresentationTimeStamp(sample.get())) };
}

Ref<GenericPromise> MediaRecorderPrivateWriterWebM::encodePendingVideoFrames()
{
    assertIsCurrent(workQueue());

    if (m_pendingVideoFrames.isEmpty() || !m_videoEncoder)
        return GenericPromise::createAndResolve();

    m_pendingVideoEncode++;

    Vector<Ref<VideoEncoder::EncodePromise>> promises { m_pendingVideoFrames.size() , [&](auto) {
        assertIsCurrent(workQueue());
        auto frame = m_pendingVideoFrames.takeFirst();
        bool needVideoKeyframe = false;
        if (frame.second - m_lastVideoKeyframeTime >= m_maxClusterDuration) {
            needVideoKeyframe = true;
            m_lastVideoKeyframeTime = frame.second;
        }
        LOG(MediaStream, "encodePendingVideoFrames:encoding video frame:%f", frame.second.value());
        return m_videoEncoder->encode({ WTFMove(frame.first), frame.second.microsecondsAs<int64_t>(), { } }, needVideoKeyframe);
    } };

    return VideoEncoder::EncodePromise::all(WTFMove(promises))->whenSettled(protectedWorkQueue(), [weakThis = ThreadSafeWeakPtr { *this }](auto&&) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return GenericPromise::createAndResolve();
        assertIsCurrent(protectedThis->workQueue());
        protectedThis->m_pendingVideoEncode--;
        return GenericPromise::createAndResolve();
    });
}

void MediaRecorderPrivateWriterWebM::appendAudioSampleBuffer(const PlatformAudioData& data, const AudioStreamDescription& description, const MediaTime&, size_t sampleCount)
{
    if (m_isStopped)
        return;

    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

    protectedWorkQueue()->dispatch([weakThis = ThreadSafeWeakPtr { *this }, sampleCount, sampleRate = description.sampleRate()] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        assertIsCurrent(protectedThis->workQueue());
        protectedThis->m_audioSamplesCount += sampleCount;
        LOG(MediaStream, "appendAudioSampleBuffer: sampleCount=%lu total:%lld time:%f", sampleCount, protectedThis->m_audioSamplesCount, protectedThis->m_currentAudioSampleTime.toDouble());
        protectedThis->m_currentAudioSampleTime = { protectedThis->m_audioSamplesCount, static_cast<uint32_t>(sampleRate) };
    });

    if (auto sampleBuffer = createAudioSampleBuffer(data, description, PAL::CMTimeMake(m_audioSamplesCountAudioThread, description.sampleRate()), sampleCount))
        RefPtr { m_audioCompressor }->addSampleBuffer(sampleBuffer.get());
    m_audioSamplesCountAudioThread += sampleCount;
}

void MediaRecorderPrivateWriterWebM::flushEncodedQueues()
{
    assertIsCurrent(workQueue());

    if (m_hasAudio)
        enqueueCompressedAudioSampleBuffers(); // compressedAudioOutputBufferCallback isn't always called when new frames are available. Force refresh

    bool success = true;
    while ((!m_encodedVideoFrames.isEmpty() || !m_encodedAudioFrames.isEmpty()) && success)
        success = muxNextFrame();
}

void MediaRecorderPrivateWriterWebM::partiallyFlushEncodedQueues()
{
    assertIsCurrent(workQueue());

    if (!m_hasStartedWriting)
        return;

    if (m_hasAudio)
        enqueueCompressedAudioSampleBuffers(); // compressedAudioOutputBufferCallback isn't always called when new frames are available. Force refresh

    // We can only ever mux frames with monotonic timeframes.
    // We can write all pending audio frames immediately if:
    // - no video frames are being encoded (e.g. m_pendingVideoEncode > 0)
    // - we have already received the first video frame.
    bool videoPending = m_pendingVideoEncode || !m_pendingVideoFrames.isEmpty() || !m_firstVideoFrameAudioTime;
    bool success = true;
    while (((!m_hasVideo || !m_encodedVideoFrames.isEmpty() || !videoPending) && (!m_hasAudio || !m_encodedAudioFrames.isEmpty())) && success) {
        if (m_encodedVideoFrames.isEmpty() && m_encodedAudioFrames.isEmpty())
            return;
        success = muxNextFrame();
    };
}

bool MediaRecorderPrivateWriterWebM::muxNextFrame()
{
    assertIsCurrent(workQueue());

    LOG(MediaStream, "muxNextFrame:audioqueue:%zu first:%f last:%f videoQueue:%zu first:%f last:%f", m_encodedAudioFrames.size(), m_encodedAudioFrames.size() ? sampleTime(m_encodedAudioFrames.first()).value() : 0, m_encodedAudioFrames.size() ? sampleTime(m_encodedAudioFrames.last()).value() : 0, m_encodedVideoFrames.size(), m_encodedVideoFrames.size() ? m_encodedVideoFrames.first().timestamp / 1000000.0 : 0, m_encodedVideoFrames.size() ? m_encodedVideoFrames.last().timestamp / 1000000.0 : 0);

    // Determine if we should mux a video or audio frame. We favor the audio frame first to satisfy webm muxing requirement:
    // https://www.webmproject.org/docs/container/
    // - Audio blocks that contain the video key frame's timecode SHOULD be in the same cluster as the video key frame block.
    // - Audio blocks that have same absolute timecode as video blocks SHOULD be written before the video blocks.
    bool takeVideo = !m_encodedVideoFrames.isEmpty() && (m_encodedAudioFrames.isEmpty() || m_encodedVideoFrames.first().timestamp < sampleTime(m_encodedAudioFrames.first()).microsecondsAs<int64_t>());

    if (takeVideo) {
        auto frame = m_encodedVideoFrames.takeFirst();
        if (frame.isKeyFrame && frame.timestamp)
            m_delegate->forceNewClusterOnNextFrame();
        Seconds pts = Seconds::fromMicroseconds(frame.timestamp);
        LOG(MediaStream, "muxNextFrame: writing video frame time:%f (previous:%f)", pts.value(), m_lastMuxedSampleTime.value());
        ASSERT(pts >= m_lastMuxedSampleTime);
        m_lastMuxedSampleTime = pts;

        return m_delegate->addFrame({ frame.data.data(), frame.data.size() }, m_videoTrackIndex, m_lastMuxedSampleTime.nanosecondsAs<uint64_t>(), frame.isKeyFrame);
    }

    ASSERT(!m_encodedAudioFrames.isEmpty());
    RetainPtr sampleBuffer = m_encodedAudioFrames.takeFirst();
    auto buffer = PAL::CMSampleBufferGetDataBuffer(sampleBuffer.get());
    ASSERT(PAL::CMBlockBufferIsRangeContiguous(buffer, 0, 0));
    size_t srcSize = PAL::CMBlockBufferGetDataLength(buffer);
    char* srcData = nullptr;
    if (PAL::CMBlockBufferGetDataPointer(buffer, 0, nullptr, nullptr, &srcData) != noErr)
        return false;

    maybeForceNewCluster();

    auto pts = sampleTime(sampleBuffer);
    LOG(MediaStream, "muxNextFrame: writing audio frame time:%f (previous:%f)", pts.value(), m_lastMuxedSampleTime.value());
    ASSERT(pts >= m_lastMuxedSampleTime);
    m_lastMuxedSampleTime = pts;
    return m_delegate->addFrame({ reinterpret_cast<const uint8_t*>(srcData), srcSize }, m_audioTrackIndex, m_lastMuxedSampleTime.nanosecondsAs<uint64_t>(), true);
}

void MediaRecorderPrivateWriterWebM::stopRecording()
{
    assertIsMainThread();

    if (m_isStopped)
        return;

    LOG(MediaStream, "stopRecording");
    m_isStopped = true;

    m_finalOperations = invokeAsync(protectedWorkQueue(), [protectedThis = Ref { *this }, this]() mutable -> Ref<GenericPromise> {
        if (m_audioCompressor)
            RefPtr { m_audioCompressor }->finish();

        return flushPendingData()->whenSettled(protectedWorkQueue(), [protectedThis = WTFMove(protectedThis), this] {
            assertIsCurrent(workQueue());
            if (m_videoEncoder)
                m_videoEncoder->close();

            flushEncodedQueues();

            ASSERT(m_pendingVideoFrames.isEmpty() && m_encodedVideoFrames.isEmpty() && !m_pendingVideoEncode);
            ASSERT(!m_hasAudio || (m_encodedAudioFrames.isEmpty() && m_audioCompressor->isEmpty()));

            m_hasStartedWriting = false;

            m_delegate->finalize();

            return GenericPromise::createAndResolve();
        });
    });
}

void MediaRecorderPrivateWriterWebM::fetchData(CompletionHandler<void(RefPtr<FragmentedSharedBuffer>&&, double)>&& completionHandler)
{
    assertIsMainThread();

    auto finalAction = [protectedThis = Ref { *this }, this, completionHandler = WTFMove(completionHandler)]() mutable {
        assertIsCurrent(workQueue());
        auto currentTimeCode = m_timeCode;
        if (m_hasAudio)
            m_timeCode = m_currentAudioSampleTime.toDouble();
        else {
            auto sampleTime = MonotonicTime::now() - m_resumedVideoTime;
            m_timeCode = (sampleTime + m_currentVideoDuration).value();
        }
        callOnMainThread([completionHandler = WTFMove(completionHandler), data = takeData(), currentTimeCode]() mutable {
            completionHandler(WTFMove(data), currentTimeCode);
        });
    };

    // If the recorder has been stopped, we want to return the data only once the final flush has completed.
    // FIXME: MediaRecorderPrivateAVFImpl::stopRecording should not immediately call its CompletionHandler
    if (m_finalOperations) {
        m_finalOperations = RefPtr { m_finalOperations }->whenSettled(protectedWorkQueue(), [completionHandler = WTFMove(finalAction)]() mutable {
            completionHandler();
            return GenericPromise::createAndResolve();
        });
    } else {
        protectedWorkQueue()->dispatch([completionHandler = WTFMove(finalAction), this]() mutable {
            flushPendingData()->whenSettled(protectedWorkQueue(), [completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler();
            });
        });
    }
}

Ref<GenericPromise> MediaRecorderPrivateWriterWebM::flushPendingData()
{
    assertIsCurrent(workQueue());

    Vector<Ref<GenericPromise>> promises;
    promises.reserveInitialCapacity(size_t(!!m_videoEncoder) + size_t(!!m_audioCompressor) + 1);
    promises.append(encodePendingVideoFrames());
    if (m_videoEncoder)
        promises.append(m_videoEncoder->flush());
    if (m_audioCompressor)
        promises.append(RefPtr { m_audioCompressor }->flush());

    return GenericPromise::all(WTFMove(promises))->whenSettled(protectedWorkQueue(), [weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->partiallyFlushEncodedQueues();
        return GenericPromise::createAndResolve();
    });
}

void MediaRecorderPrivateWriterWebM::appendData(std::span<const uint8_t> data)
{
    assertIsCurrent(workQueue());

    m_lastTimestamp = MonotonicTime::now();

    if (!m_dataBuffer.capacity())
        m_dataBuffer.reserveInitialCapacity(s_dataBufferSize);

    if (data.size() > (m_dataBuffer.capacity() - m_dataBuffer.size()))
        flushDataBuffer();
    if (data.size() < m_dataBuffer.capacity())
        m_dataBuffer.append(data);
    else
        m_data.append(data);
}

void MediaRecorderPrivateWriterWebM::flushDataBuffer()
{
    assertIsCurrent(workQueue());

    if (m_dataBuffer.isEmpty())
        return;
    m_data.append(std::exchange(m_dataBuffer, { }));
}

RefPtr<FragmentedSharedBuffer> MediaRecorderPrivateWriterWebM::takeData()
{
    assertIsCurrent(workQueue());

    if (!m_delegate->hasAddedFrame())
        return FragmentedSharedBuffer::create();
    flushDataBuffer();
    return m_data.take();
}

void MediaRecorderPrivateWriterWebM::pause()
{
    protectedWorkQueue()->dispatch([weakThis = ThreadSafeWeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        assertIsCurrent(protectedThis->workQueue());
        auto recordingDuration = MonotonicTime::now() - protectedThis->m_resumedVideoTime;
        protectedThis->m_currentVideoDuration = recordingDuration + protectedThis->m_currentVideoDuration;
        LOG(MediaStream, "MediaRecorderPrivateWriterWebM::pause m_currentVideoDuration:%f", protectedThis->m_currentVideoDuration.value());
    });
}

void MediaRecorderPrivateWriterWebM::resume()
{
    protectedWorkQueue()->dispatch([weakThis = ThreadSafeWeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        assertIsCurrent(protectedThis->workQueue());
        protectedThis->m_firstVideoFrameAudioTime.reset();
        protectedThis->m_resumedVideoTime = MonotonicTime::now();
        protectedThis->m_resumedAudioTime = protectedThis->m_currentAudioSampleTime;
        LOG(MediaStream, "MediaRecorderPrivateWriterWebM:resume");
    });
}

const String& MediaRecorderPrivateWriterWebM::mimeType() const
{
    assertIsMainThread();
    return m_mimeType;
}

unsigned MediaRecorderPrivateWriterWebM::audioBitRate() const
{
    assertIsMainThread();
    return m_audioBitsPerSecond;
}

unsigned MediaRecorderPrivateWriterWebM::videoBitRate() const
{
    assertIsMainThread();
    return m_videoBitsPerSecond;
}

void MediaRecorderPrivateWriterWebM::maybeForceNewCluster()
{
    assertIsCurrent(workQueue());

    // If we have a video, a new cluster is determined with having a new keyframe.
    if (m_hasVideo || !m_maxClusterDuration || !m_lastTimestamp)
        return;

    if (MonotonicTime::now() - m_lastTimestamp >= m_maxClusterDuration)
        m_delegate->forceNewClusterOnNextFrame();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_RECORDER)
