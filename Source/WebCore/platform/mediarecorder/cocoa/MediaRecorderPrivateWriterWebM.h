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
#include <wtf/CompletionHandler.h>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/MediaTime.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RetainPtr.h>

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

    void encodePendingVideoFrames();
    Seconds nextVideoFrameTime() const;
    Seconds resumeVideoTime() const;

    void flushEncodedQueues();
    void partiallyFlushEncodedQueues();
    bool muxNextFrame();

    Ref<GenericPromise> flushPendingData();
    void completeFetchData();

    void maybeStartWriting();
    void maybeForceNewCluster();

    friend class MediaRecorderPrivateWriterWebMDelegate;
    void appendData(std::span<const uint8_t>);
    RefPtr<FragmentedSharedBuffer> takeData();
    void flushDataBuffer();

    bool m_hasStartedWriting { false };
    bool m_isStopped { false };
    bool m_isStopping { false };

    Lock m_dataLock;
    SharedBufferBuilder m_data WTF_GUARDED_BY_LOCK(m_dataLock);
    static constexpr size_t s_dataBufferSize { 1024 };
    Vector<uint8_t> m_dataBuffer;
    CompletionHandler<void(RefPtr<FragmentedSharedBuffer>&&, double)> m_fetchDataCompletionHandler;

    bool m_hasAudio { false };
    RetainPtr<CMFormatDescriptionRef> m_audioFormatDescription;
    RefPtr<AudioSampleBufferCompressor> m_audioCompressor;
    Deque<RetainPtr<CMSampleBufferRef>> m_encodedAudioFrames;
    uint8_t m_audioTrackIndex { 0 };
    int64_t m_audioSamplesCount { 0 };
    MediaTime m_currentAudioSampleTime;
    MediaTime m_resumedAudioTime;
    FourCharCode m_audioCodec { 0 };

    bool m_hasVideo { false };
    std::unique_ptr<VideoEncoder> m_videoEncoder;
    uint64_t m_videoBitsPerSecond { 0 };
    Deque<std::pair<Ref<VideoFrame>, Seconds>> m_pendingVideoFrames;
    Deque<VideoEncoder::EncodedFrame> m_encodedVideoFrames;
    bool m_firstVideoFrameProcessed { false };
    size_t m_pendingVideoEncode { 0 };
    uint8_t m_videoTrackIndex { 0 };
    MonotonicTime m_resumedVideoTime;
    Seconds m_currentVideoDuration;
    Seconds m_lastVideoKeyframeTime;
    Seconds m_lastEnqueuedRawVideoFrame;
    Seconds m_lastReceivedCompressedVideoFrame;
    std::optional<Seconds> m_firstVideoFrameAudioTime;
    FourCharCode m_videoCodec { 0 };

    double m_timeCode { 0 };
    Seconds m_maxClusterDuration { 2_s };
    MonotonicTime m_lastTimestamp;
    Seconds m_lastMuxedSampleTime;
    UniqueRef<MediaRecorderPrivateWriterWebMDelegate> m_delegate;
    String m_mimeType;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_RECORDER)
