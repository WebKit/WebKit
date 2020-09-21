/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)

#include "AudioStreamDescription.h"

#include "SharedBuffer.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WeakPtr.h>
#include <wtf/threads/BinarySemaphore.h>

#include <CoreAudio/CoreAudioTypes.h>
#include <CoreMedia/CMTime.h>

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;
typedef struct opaqueCMBufferQueueTriggerToken *CMBufferQueueTriggerToken;

OBJC_CLASS AVAssetWriter;
OBJC_CLASS AVAssetWriterInput;
OBJC_CLASS WebAVAssetWriterDelegate;

namespace WTF {
class MediaTime;
}

namespace WebCore {

class AudioSampleBufferCompressor;
class AudioStreamDescription;
class MediaSample;
class MediaStreamTrackPrivate;
class PlatformAudioData;
class VideoSampleBufferCompressor;
struct MediaRecorderPrivateOptions;

class WEBCORE_EXPORT MediaRecorderPrivateWriter : public ThreadSafeRefCounted<MediaRecorderPrivateWriter, WTF::DestructionThread::Main>, public CanMakeWeakPtr<MediaRecorderPrivateWriter, WeakPtrFactoryInitialization::Eager> {
public:
    static RefPtr<MediaRecorderPrivateWriter> create(bool hasAudio, bool hasVideo, const MediaRecorderPrivateOptions&);
    ~MediaRecorderPrivateWriter();

    void appendVideoSampleBuffer(MediaSample&);
    void appendAudioSampleBuffer(const PlatformAudioData&, const AudioStreamDescription&, const WTF::MediaTime&, size_t);
    void stopRecording();
    void fetchData(CompletionHandler<void(RefPtr<SharedBuffer>&&)>&&);

    void appendData(const char*, size_t);
    void appendData(Ref<SharedBuffer>&&);

private:
    MediaRecorderPrivateWriter(bool hasAudio, bool hasVideo);
    void clear();

    bool initialize();
    void setOptions(const MediaRecorderPrivateOptions&);

    static void compressedVideoOutputBufferCallback(void*, CMBufferQueueTriggerToken);
    static void compressedAudioOutputBufferCallback(void*, CMBufferQueueTriggerToken);

    void startAssetWriter();

    void appendCompressedSampleBuffers();
    bool appendCompressedAudioSampleBufferIfPossible();
    bool appendCompressedVideoSampleBufferIfPossible();
    void appendCompressedVideoSampleBuffer(CMSampleBufferRef);

    void processNewCompressedAudioSampleBuffers();
    void processNewCompressedVideoSampleBuffers();

    void finishAppendingCompressedAudioSampleBuffers(CompletionHandler<void()>&&);
    void finishAppendingCompressedVideoSampleBuffers(CompletionHandler<void()>&&);
    void flushCompressedSampleBuffers(CompletionHandler<void()>&&);

    void finishedFlushingSamples();

    bool m_hasAudio { false };
    bool m_hasVideo { false };

    bool m_hasStartedWriting { false };
    bool m_isStopped { false };
    bool m_isStopping { false };

    RetainPtr<AVAssetWriter> m_writer;

    RefPtr<SharedBuffer> m_data;
    CompletionHandler<void(RefPtr<SharedBuffer>&&)> m_fetchDataCompletionHandler;

    RetainPtr<CMFormatDescriptionRef> m_audioFormatDescription;
    std::unique_ptr<AudioSampleBufferCompressor> m_audioCompressor;
    RetainPtr<AVAssetWriterInput> m_audioAssetWriterInput;

    RetainPtr<CMFormatDescriptionRef> m_videoFormatDescription;
    std::unique_ptr<VideoSampleBufferCompressor> m_videoCompressor;
    RetainPtr<AVAssetWriterInput> m_videoAssetWriterInput;
    CMTime m_lastVideoPresentationTime { kCMTimeInvalid };
    CMTime m_lastVideoDecodingTime { kCMTimeInvalid };
    bool m_hasEncodedVideoSamples { false };

    RetainPtr<WebAVAssetWriterDelegate> m_writerDelegate;
    Deque<RetainPtr<CMSampleBufferRef>> m_pendingVideoSampleQueue;
    Deque<RetainPtr<CMSampleBufferRef>> m_pendingAudioSampleQueue;

    bool m_isFlushingSamples { false };
    bool m_shouldStopAfterFlushingSamples { false };
    bool m_firstVideoFrame { false };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)
