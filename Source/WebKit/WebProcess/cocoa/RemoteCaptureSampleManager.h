/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#include "Connection.h"
#include "IPCSemaphore.h"
#include "MessageReceiver.h"
#include "RemoteRealtimeAudioSource.h"
#include "RemoteRealtimeVideoSource.h"
#include "RemoteVideoFrameIdentifier.h"
#include "RemoteVideoFrameProxy.h"
#include "SharedCARingBuffer.h"
#include "WorkQueueMessageReceiver.h"
#include <WebCore/CAAudioStreamDescription.h>
#include <WebCore/CARingBuffer.h>
#include <WebCore/WebAudioBufferList.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/WorkQueue.h>

namespace WebCore {
class ImageTransferSessionVT;
}

namespace WebKit {
class RemoteVideoFrameObjectHeapProxy;

class RemoteCaptureSampleManager : public IPC::WorkQueueMessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteCaptureSampleManager();
    ~RemoteCaptureSampleManager();
    void stopListeningForIPC();

    void addSource(Ref<RemoteRealtimeAudioSource>&&);
    void addSource(Ref<RemoteRealtimeVideoSource>&&);
    void removeSource(WebCore::RealtimeMediaSourceIdentifier);

    void didUpdateSourceConnection(IPC::Connection&);
    void setVideoFrameObjectHeapProxy(RemoteVideoFrameObjectHeapProxy*);

    // IPC::WorkQueueMessageReceiver overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

private:
    // Messages
    void audioStorageChanged(WebCore::RealtimeMediaSourceIdentifier, ConsumerSharedCARingBuffer::Handle&&, const WebCore::CAAudioStreamDescription&, IPC::Semaphore&&, const MediaTime&, size_t frameSampleSize);
    void audioSamplesAvailable(WebCore::RealtimeMediaSourceIdentifier, MediaTime, uint64_t numberOfFrames);
    void videoFrameAvailable(WebCore::RealtimeMediaSourceIdentifier, RemoteVideoFrameProxy::Properties&&, WebCore::VideoFrameTimeMetadata);
    // FIXME: Will be removed once RemoteVideoFrameProxy providers are the only ones sending data.
    void videoFrameAvailableCV(WebCore::RealtimeMediaSourceIdentifier, RetainPtr<CVPixelBufferRef>&&, WebCore::VideoFrame::Rotation, bool mirrored, MediaTime, WebCore::VideoFrameTimeMetadata);

    void setConnection(IPC::Connection*);

    class RemoteAudio {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        explicit RemoteAudio(Ref<RemoteRealtimeAudioSource>&&);
        ~RemoteAudio();

        void setStorage(ConsumerSharedCARingBuffer::Handle&&, const WebCore::CAAudioStreamDescription&, IPC::Semaphore&&, const MediaTime&, size_t frameChunkSize);

    private:
        void stopThread();
        void startThread();

        Ref<RemoteRealtimeAudioSource> m_source;
        std::optional<WebCore::CAAudioStreamDescription> m_description;
        std::unique_ptr<WebCore::WebAudioBufferList> m_buffer;
        std::unique_ptr<ConsumerSharedCARingBuffer> m_ringBuffer;
        int64_t m_readOffset { 0 };
        MediaTime m_startTime;
        size_t m_frameChunkSize { 0 };

        IPC::Semaphore m_semaphore;
        RefPtr<Thread> m_thread;
        std::atomic<bool> m_shouldStopThread { false };
    };

    bool m_isRegisteredToParentProcessConnection { false };
    Ref<WorkQueue> m_queue;
    RefPtr<IPC::Connection> m_connection;
    // background thread member
    HashMap<WebCore::RealtimeMediaSourceIdentifier, std::unique_ptr<RemoteAudio>> m_audioSources;
    HashMap<WebCore::RealtimeMediaSourceIdentifier, Ref<RemoteRealtimeVideoSource>> m_videoSources;

    Lock m_videoFrameObjectHeapProxyLock;
    RefPtr<RemoteVideoFrameObjectHeapProxy> m_videoFrameObjectHeapProxy WTF_GUARDED_BY_LOCK(m_videoFrameObjectHeapProxyLock);
};

} // namespace WebKit

#endif
