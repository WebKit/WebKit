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

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)

#include "AudioMediaStreamTrackRendererIdentifier.h"
#include "Connection.h"
#include "GPUConnectionToWebProcess.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebKit {

class RemoteAudioMediaStreamTrackRenderer;

class RemoteAudioMediaStreamTrackRendererManager final : public IPC::Connection::ThreadMessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteAudioMediaStreamTrackRendererManager> create(GPUConnectionToWebProcess& process) { return adoptRef(*new RemoteAudioMediaStreamTrackRendererManager(process)); }
    ~RemoteAudioMediaStreamTrackRendererManager();

    void didReceiveRendererMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder) { didReceiveMessage(connection, decoder); }

    IPC::Connection& connection() const { return m_connectionToWebProcess.connection(); }

    void close();

private:
    explicit RemoteAudioMediaStreamTrackRendererManager(GPUConnectionToWebProcess&);

    // IPC::Connection::ThreadMessageReceiver
    void dispatchToThread(Function<void()>&&) final;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void createRenderer(AudioMediaStreamTrackRendererIdentifier);
    void releaseRenderer(AudioMediaStreamTrackRendererIdentifier);

    GPUConnectionToWebProcess& m_connectionToWebProcess;
    Ref<WorkQueue> m_queue;
    HashMap<AudioMediaStreamTrackRendererIdentifier, std::unique_ptr<RemoteAudioMediaStreamTrackRenderer>> m_renderers;
};

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)
