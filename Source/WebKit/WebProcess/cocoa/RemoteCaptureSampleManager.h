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
#include "MessageReceiver.h"
#include "RemoteRealtimeMediaSource.h"
#include "SharedMemory.h"
#include <WebCore/CAAudioStreamDescription.h>
#include <WebCore/CARingBuffer.h>
#include <WebCore/WebAudioBufferList.h>
#include <wtf/HashMap.h>
#include <wtf/WorkQueue.h>

namespace WebKit {

class RemoteCaptureSampleManager : public IPC::Connection::ThreadMessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteCaptureSampleManager();
    ~RemoteCaptureSampleManager();

    void addSource(Ref<RemoteRealtimeMediaSource>&&);
    void removeSource(WebCore::RealtimeMediaSourceIdentifier);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

private:
    // IPC::Connection::ThreadMessageReceiver
    void dispatchToThread(Function<void()>&&) final;

    // Messages
    void audioStorageChanged(WebCore::RealtimeMediaSourceIdentifier, const SharedMemory::IPCHandle&, const WebCore::CAAudioStreamDescription&, uint64_t numberOfFrames);
    void audioSamplesAvailable(WebCore::RealtimeMediaSourceIdentifier, MediaTime, uint64_t numberOfFrames, uint64_t startFrame, uint64_t endFrame);

    void setConnection(IPC::Connection*);

    class RemoteAudio {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        explicit RemoteAudio(Ref<RemoteRealtimeMediaSource>&&);

        void setStorage(const SharedMemory::Handle&, const WebCore::CAAudioStreamDescription&, uint64_t numberOfFrames);
        void audioSamplesAvailable(MediaTime, uint64_t numberOfFrames, uint64_t startFrame, uint64_t endFrame);

    private:
        Ref<RemoteRealtimeMediaSource> m_source;
        WebCore::CAAudioStreamDescription m_description;
        std::unique_ptr<WebCore::CARingBuffer> m_ringBuffer;
        std::unique_ptr<WebCore::WebAudioBufferList> m_buffer;
    };

    Ref<WorkQueue> m_queue;
    RefPtr<IPC::Connection> m_connection;

    // background thread member
    HashMap<WebCore::RealtimeMediaSourceIdentifier, std::unique_ptr<RemoteAudio>> m_sources;
};

} // namespace WebKit

#endif
