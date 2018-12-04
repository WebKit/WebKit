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

#if ENABLE(MEDIA_STREAM)

#include "SharedBuffer.h"
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/threads/BinarySemaphore.h>

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

OBJC_CLASS AVAssetWriter;
OBJC_CLASS AVAssetWriterInput;

namespace WTF {
class MediaTime;
}

namespace WebCore {

class AudioStreamDescription;
class PlatformAudioData;

class MediaRecorderPrivateWriter : public ThreadSafeRefCounted<MediaRecorderPrivateWriter, WTF::DestructionThread::Main> {
public:
    MediaRecorderPrivateWriter() = default;
    
    bool setupWriter();
    bool setVideoInput(int width, int height);
    bool setAudioInput();
    void appendVideoSampleBuffer(CMSampleBufferRef);
    void appendAudioSampleBuffer(const PlatformAudioData&, const AudioStreamDescription&, const WTF::MediaTime&, size_t);
    void stopRecording();
    RefPtr<SharedBuffer> fetchData();

private:
    RetainPtr<AVAssetWriter> m_writer;
    RetainPtr<AVAssetWriterInput> m_videoInput;
    RetainPtr<AVAssetWriterInput> m_audioInput;

    String m_path;
    Lock m_videoLock;
    Lock m_audioLock;
    BinarySemaphore m_finishWritingSemaphore;
    BinarySemaphore m_finishWritingAudioSemaphore;
    BinarySemaphore m_finishWritingVideoSemaphore;
    bool m_hasStartedWriting { false };
    bool m_isStopped { false };
    bool m_isFirstAudioSample { true };
    dispatch_queue_t m_audioPullQueue;
    dispatch_queue_t m_videoPullQueue;
    Deque<RetainPtr<CMSampleBufferRef>> m_videoBufferPool;
    Deque<RetainPtr<CMSampleBufferRef>> m_audioBufferPool;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
