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

#if ENABLE(MEDIA_STREAM)

#include "MessageReceiver.h"
#include "MessageSender.h"
#include <WebCore/RealtimeMediaSourceIdentifier.h>

#if PLATFORM(COCOA)
#include "SharedCARingBuffer.h"
#endif

namespace WTF {
class MediaTime;
}

namespace WebCore {
class CaptureDevice;
}

namespace WebKit {

class SpeechRecognitionRemoteRealtimeMediaSource;

class SpeechRecognitionRemoteRealtimeMediaSourceManager final : public IPC::MessageReceiver, public IPC::MessageSender {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit SpeechRecognitionRemoteRealtimeMediaSourceManager(Ref<IPC::Connection>&&);
    void addSource(SpeechRecognitionRemoteRealtimeMediaSource&, const WebCore::CaptureDevice&);
    void removeSource(SpeechRecognitionRemoteRealtimeMediaSource&);

private:
    // Messages::SpeechRecognitionRemoteRealtimeMediaSourceManager
    void remoteAudioSamplesAvailable(WebCore::RealtimeMediaSourceIdentifier, const WTF::MediaTime&, uint64_t numberOfFrames);
    void remoteCaptureFailed(WebCore::RealtimeMediaSourceIdentifier);
    void remoteSourceStopped(WebCore::RealtimeMediaSourceIdentifier);
#if PLATFORM(COCOA)
    void setStorage(WebCore::RealtimeMediaSourceIdentifier, ConsumerSharedCARingBuffer::Handle&&, const WebCore::CAAudioStreamDescription&);
#endif

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    Ref<IPC::Connection> m_connection;
    HashMap<WebCore::RealtimeMediaSourceIdentifier, WeakPtr<SpeechRecognitionRemoteRealtimeMediaSource>> m_sources;
    HashSet<WebCore::RealtimeMediaSourceIdentifier> m_sourcesNeedingSandboxExtension;
};

} // namespace WebKit

#endif
