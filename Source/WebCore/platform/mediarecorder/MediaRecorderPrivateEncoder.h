/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_RECORDER)

#include "CAAudioStreamDescription.h"
#include "MediaRecorderPrivateWriter.h"
#include "PlatformTimeRanges.h"
#include "SharedBuffer.h"
#include "VideoEncoder.h"
#include <atomic>
#include <span>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/MediaTime.h>
#include <wtf/MonotonicTime.h>
#include <wtf/NativePromise.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WorkQueue.h>

typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;
typedef struct opaqueCMBufferQueueTriggerToken *CMBufferQueueTriggerToken;

namespace WebCore {

class AudioSampleBufferConverter;
class InProcessCARingBuffer;
class FragmentedSharedBuffer;
struct MediaRecorderPrivateOptions;
class MediaSample;
class PlatformAudioData;
class VideoFrame;
class AudioInfo;
class VideoInfo;

class MediaRecorderPrivateEncoder final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<MediaRecorderPrivateEncoder, WTF::DestructionThread::Main> {
public:
    static RefPtr<MediaRecorderPrivateEncoder> create(bool hasAudio, bool hasVideo, const MediaRecorderPrivateOptions&);
    ~MediaRecorderPrivateEncoder();

    void NODELETE appendVideoFrame(VideoFrame&);
    void NODELETE appendAudioSampleBuffer(const PlatformAudioData&, const AudioStreamDescription&, const WTF::MediaTime&, size_t);
    void NODELETE fetchData(CompletionHandler<void(Ref<FragmentedSharedBuffer>&&, double)>&&);

    void NODELETE pause();
    void NODELETE resume();
    void NODELETE stopRecording();
    void NODELETE close();

    String NODELETE mimeType() const;
    unsigned NODELETE audioBitRate() const;
    unsigned NODELETE videoBitRate() const;

    bool hasAudio() const { return m_hasAudio; }
    bool hasVideo() const { return m_hasVideo; }
    bool shouldApplyVideoRotation() const { return m_writer && m_writer->shouldApplyVideoRotation(); }

private:
    MediaRecorderPrivateEncoder(bool hasAudio, bool hasVideo);
    bool NODELETE initialize(const MediaRecorderPrivateOptions&, UniqueRef<MediaRecorderPrivateWriter>&&);

    static WorkQueue& NODELETE queueSingleton();

    Ref<MediaRecorderPrivateWriterListener> NODELETE listener();

    class Listener;
    friend class Listener;
    void NODELETE appendData(std::span<const uint8_t>);

    MediaTime lastEnqueuedAudioTime() const { return MediaTime(m_lastEnqueuedAudioTimeUs.load(), 1000000); }
    MediaTime NODELETE currentTime() const;
    MediaTime NODELETE currentTime(const MediaTime&, const MonotonicTime&) const;
    MediaTime NODELETE currentEndTime() const;

    void NODELETE flushDataBuffer();
    bool NODELETE segmentsMustStartWithVideoKeyframe() const;

    Ref<FragmentedSharedBuffer> NODELETE takeData();

    MediaTime lastMuxedSampleTime() const;

    void NODELETE generateMIMEType();

    void NODELETE audioSamplesDescriptionChanged(const AudioStreamBasicDescription&, InProcessCARingBuffer*, size_t);
    void NODELETE audioSamplesAvailable(const MediaTime&, size_t, size_t);
    RefPtr<AudioSampleBufferConverter> NODELETE audioConverter() const;
    void NODELETE enqueueCompressedAudioSampleBuffers();

    void NODELETE appendVideoFrame(MediaTime, Ref<VideoFrame>&&);
    Ref<GenericPromise> NODELETE encodePendingVideoFrames(const MediaTime&);
    void NODELETE processVideoEncoderActiveConfiguration(const VideoEncoder::Config&, const VideoEncoderActiveConfiguration&);
    void NODELETE enqueueCompressedVideoFrame(VideoEncoder::EncodedFrame&&);

    Ref<GenericPromise> NODELETE flushPendingData(const MediaTime&);
    void NODELETE partiallyFlushEncodedQueues();
    Ref<GenericPromise> NODELETE waitForMatchingAudio(const MediaTime&);
    using Result = MediaRecorderPrivateWriter::Result;
    MediaTime NODELETE flushToEndSegment(const MediaTime&);
    void NODELETE flushAllEncodedQueues();
    void NODELETE interleaveAndEnqueueNextFrame();

    void NODELETE maybeStartWriter();
    bool NODELETE hasMuxedDataSinceEndSegment() const;

    void NODELETE addRingBuffer(const AudioStreamDescription&);
    void NODELETE writeDataToRingBuffer(AudioBufferList*, size_t, size_t);
    void NODELETE clearRingBuffersIfPossible();

    std::atomic<bool> m_isStopped { false };
    bool m_writerIsStarted WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    bool m_writerIsClosed WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    MediaTime m_lastMuxedSampleStartTime WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    bool m_lastMuxedSampleIsVideo WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    MediaTime m_lastMuxedAudioSampleEndTime WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    bool m_hasMuxedAudioFrameSinceEndSegment WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    bool m_hasMuxedVideoFrameSinceEndSegment WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    bool m_nextVideoFrameMuxedShouldBeKeyframe WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { true };
    size_t m_pendingFlush WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { 0 };
    Ref<GenericPromise> m_currentFlushOperations WTF_GUARDED_BY_CAPABILITY(mainThread) { GenericPromise::createAndResolve() };

    String m_mimeType WTF_GUARDED_BY_CAPABILITY(mainThread);

    FourCharCode m_audioCodec { 0 }; // set on creation. const after
    String m_audioCodecMimeType WTF_GUARDED_BY_CAPABILITY(mainThread);
    std::optional<uint8_t> m_audioTrackIndex WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    RetainPtr<CMFormatDescriptionRef> m_audioFormatDescription WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    RefPtr<AudioInfo> m_audioCompressedAudioInfo WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    RefPtr<AudioSampleBufferConverter> m_audioConverter WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    bool m_formatChangedOccurred WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    std::optional<AudioStreamBasicDescription> m_originalOutputDescription WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Deque<UniqueRef<MediaSamplesBlock>> m_encodedAudioFrames WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    std::optional<std::pair<const MediaTime, GenericPromise::Producer>> m_pendingAudioFramePromise WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Deque<std::pair<std::unique_ptr<InProcessCARingBuffer>, size_t>, 4> m_ringBuffers;
    size_t m_lastRingBufferId { 0 }; // accessed on the audio thread only.
    InProcessCARingBuffer* m_currentRingBuffer WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { nullptr };
    std::atomic<size_t> m_currentRingBufferId { 0 };
    std::atomic<int64_t> m_lastEnqueuedAudioTimeUs { 0 };
    std::atomic<int64_t> m_currentAudioTimeUs { 0 };
    MediaTime m_lastRawAudioSample { MediaTime::invalidTime() };
    PlatformTimeRanges::Range m_lastEncodedAudioSampleRange WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { MediaTime::negativeInfiniteTime(), MediaTime::negativeInfiniteTime() };

    // Audio thread variables.
    std::optional<CAAudioStreamDescription> m_currentStreamDescription;
    MediaTime m_currentAudioTime { MediaTime::zeroTime() };
    uint64_t m_currentAudioSampleCount { 0 };

    FourCharCode m_videoCodec { 0 }; // set on creation. const after
    String m_videoCodecMimeType WTF_GUARDED_BY_CAPABILITY(mainThread);
    std::optional<uint8_t> m_videoTrackIndex WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    RefPtr<VideoInfo> m_videoTrackInfo WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    RetainPtr<CMFormatDescriptionRef> m_videoFormatDescription WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    RefPtr<VideoEncoder> m_videoEncoder WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Deque<std::pair<Ref<VideoFrame>, MediaTime>> m_pendingVideoFrames WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Deque<UniqueRef<MediaSamplesBlock>> m_encodedVideoFrames WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Deque<UniqueRef<MediaSamplesBlock>> m_interleavedFrames WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    bool m_firstVideoFrameProcessed WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    std::optional<MonotonicTime> m_currentVideoSegmentStartTime WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    uint64_t m_previousSegmentVideoDurationUs WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { 0 };
    MediaTime m_lastRawVideoFrameReceived WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { MediaTime::negativeInfiniteTime() };
    MediaTime m_lastEnqueuedRawVideoFrame WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { MediaTime::negativeInfiniteTime() };
    MediaTime m_lastVideoKeyframeTime WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    std::optional<CGAffineTransform> m_videoTransform;
    RefPtr<GenericNonExclusivePromise> m_videoEncoderCreationPromise;

    Lock m_lock;
    SharedBufferBuilder m_data WTF_GUARDED_BY_LOCK(m_lock);
    static constexpr size_t s_dataBufferSize { 1024 };
    Vector<uint8_t> m_dataBuffer WTF_GUARDED_BY_LOCK(m_lock);
    double m_timeCode WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { 0 };

    uint64_t m_audioBitsPerSecond { 0 }; // set on creation. const after
    uint64_t m_videoBitsPerSecond { 0 }; // set on creation. const after

    bool m_hadError WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    bool m_isPaused WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    bool m_hasStartedAudibleAudioFrame WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    bool m_needKeyFrame WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { true };
    MediaTime m_startLastSegmentTime WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { MediaTime::zeroTime() };

    const MediaTime m_minimumSegmentDuration { MediaTime::createWithDouble(1) };
    const MediaTime m_maxGOPDuration { MediaTime::createWithDouble(2) };
    const bool m_hasAudio { false };
    const bool m_hasVideo { false };
    const Ref<Listener> m_listener;
    const std::unique_ptr<MediaRecorderPrivateWriter> m_writer; // Always set and immutable once initialize() has been called
};

} // namespace WebCore

#endif // ENABLE(MEDIA_RECORDER)
