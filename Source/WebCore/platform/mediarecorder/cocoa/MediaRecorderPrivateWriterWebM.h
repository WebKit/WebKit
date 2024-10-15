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

#if ENABLE(MEDIA_RECORDER_WEBM)

#include "MediaRecorderPrivateWriter.h"
#include "SharedBuffer.h"
#include "VideoEncoder.h"
#include <CoreAudio/CoreAudioTypes.h>
#include <atomic>
#include <wtf/CompletionHandler.h>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/MediaTime.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RetainPtr.h>
#include <wtf/WorkQueue.h>

using CMBufferQueueTriggerToken = struct opaqueCMBufferQueueTriggerToken*;
using CMFormatDescriptionRef = const struct opaqueCMFormatDescription*;
using CMSampleBufferRef = struct opaqueCMSampleBuffer*;

namespace WTF {
class MediaTime;
}

namespace WebCore {

class AudioSampleBufferCompressor;
class AudioStreamDescription;
class MediaStreamTrackPrivate;
class PlatformAudioData;
class VideoFrame;
class VideoSampleBufferCompressor;
struct MediaRecorderPrivateOptions;

class MediaRecorderPrivateWriterWebMDelegate;

class MediaRecorderPrivateWriterWebM final : public MediaRecorderPrivateWriter {
public:
    static Ref<MediaRecorderPrivateWriter> create(bool hasAudio, bool hasVideo);
    ~MediaRecorderPrivateWriterWebM();

private:
    MediaRecorderPrivateWriterWebM(bool hasAudio, bool hasVideo);
    void appendVideoFrame(VideoFrame&) final;
    void appendAudioSampleBuffer(const PlatformAudioData&, const AudioStreamDescription&, const WTF::MediaTime&, size_t) final;
    void stopRecording() final;
    void fetchData(CompletionHandler<void(RefPtr<FragmentedSharedBuffer>&&, double)>&&) final;

    void pause() final;
    void resume() final;

    const String& mimeType() const final;
    unsigned audioBitRate() const final;
    unsigned videoBitRate() const final;

    void close() final;

    bool initialize(const MediaRecorderPrivateOptions&) final;

    static void compressedAudioOutputBufferCallback(void*, CMBufferQueueTriggerToken);
    void enqueueCompressedAudioSampleBuffers();
    Seconds sampleTime(const RetainPtr<CMSampleBufferRef>&) const;

    void appendVideoFrame(Ref<VideoFrame>&&);
    Ref<GenericPromise> encodePendingVideoFrames();
    Seconds nextVideoFrameTime() const;
    Seconds resumeVideoTime() const;

    void flushEncodedQueues();
    void partiallyFlushEncodedQueues();
    bool muxNextFrame();

    Ref<GenericPromise> flushPendingData();

    void maybeStartWriting();
    void maybeForceNewCluster();

    friend class MediaRecorderPrivateWriterWebMDelegate;
    void appendData(std::span<const uint8_t>);
    RefPtr<FragmentedSharedBuffer> takeData();
    void flushDataBuffer();

    WorkQueue& workQueue() const { return m_workQueue.get(); }
    Ref<WorkQueue> protectedWorkQueue() const { return m_workQueue; }

    bool m_hasStartedWriting WTF_GUARDED_BY_CAPABILITY(workQueue()) { false };
    std::atomic<bool> m_isStopped { false };

    SharedBufferBuilder m_data WTF_GUARDED_BY_CAPABILITY(workQueue());
    static constexpr size_t s_dataBufferSize { 1024 };
    Vector<uint8_t> m_dataBuffer WTF_GUARDED_BY_CAPABILITY(workQueue());

    const bool m_hasAudio { false };
    uint64_t m_audioBitsPerSecond { 0 }; // set on creation. const after
    FourCharCode m_audioCodec { 0 }; // set on creation. const after
    RetainPtr<CMFormatDescriptionRef> m_audioFormatDescription WTF_GUARDED_BY_CAPABILITY(workQueue());
    RefPtr<AudioSampleBufferCompressor> m_audioCompressor;
    Deque<RetainPtr<CMSampleBufferRef>> m_encodedAudioFrames WTF_GUARDED_BY_CAPABILITY(workQueue());
    uint8_t m_audioTrackIndex WTF_GUARDED_BY_CAPABILITY(workQueue()) { 0 };
    int64_t m_audioSamplesCountAudioThread { 0 };
    int64_t m_audioSamplesCount WTF_GUARDED_BY_CAPABILITY(workQueue()) { 0 };
    MediaTime m_currentAudioSampleTime WTF_GUARDED_BY_CAPABILITY(workQueue());
    MediaTime m_resumedAudioTime WTF_GUARDED_BY_CAPABILITY(workQueue());

    const bool m_hasVideo { false };
    uint64_t m_videoBitsPerSecond { 0 }; // set on creation. const after
    FourCharCode m_videoCodec { 0 }; // set on creation. const after
    std::unique_ptr<VideoEncoder> m_videoEncoder;
    Deque<std::pair<Ref<VideoFrame>, Seconds>> m_pendingVideoFrames WTF_GUARDED_BY_CAPABILITY(workQueue());
    Deque<VideoEncoder::EncodedFrame> m_encodedVideoFrames WTF_GUARDED_BY_CAPABILITY(workQueue());
    bool m_firstVideoFrameProcessed WTF_GUARDED_BY_CAPABILITY(workQueue()) { false };
    size_t m_pendingVideoEncode WTF_GUARDED_BY_CAPABILITY(workQueue()) { 0 };
    uint8_t m_videoTrackIndex WTF_GUARDED_BY_CAPABILITY(workQueue()) { 0 };
    MonotonicTime m_resumedVideoTime WTF_GUARDED_BY_CAPABILITY(workQueue());
    Seconds m_currentVideoDuration WTF_GUARDED_BY_CAPABILITY(workQueue());
    Seconds m_lastVideoKeyframeTime WTF_GUARDED_BY_CAPABILITY(workQueue());
    Seconds m_lastEnqueuedRawVideoFrame WTF_GUARDED_BY_CAPABILITY(workQueue());
    Seconds m_lastReceivedCompressedVideoFrame WTF_GUARDED_BY_CAPABILITY(workQueue());
    std::optional<Seconds> m_firstVideoFrameAudioTime WTF_GUARDED_BY_CAPABILITY(workQueue());

    RefPtr<GenericPromise> m_finalOperations WTF_GUARDED_BY_CAPABILITY(mainThread);
    double m_timeCode WTF_GUARDED_BY_CAPABILITY(workQueue()) { 0 };
    Seconds m_maxClusterDuration { 2_s };
    MonotonicTime m_lastTimestamp WTF_GUARDED_BY_CAPABILITY(workQueue());
    Seconds m_lastMuxedSampleTime WTF_GUARDED_BY_CAPABILITY(workQueue());
    UniqueRef<MediaRecorderPrivateWriterWebMDelegate> m_delegate;
    String m_mimeType WTF_GUARDED_BY_CAPABILITY(mainThread);
    Ref<WorkQueue> m_workQueue;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_RECORDER)
