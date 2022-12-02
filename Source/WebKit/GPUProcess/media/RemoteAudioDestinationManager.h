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

#if ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)

#include "Connection.h"
#include "IPCSemaphore.h"
#include "RemoteAudioDestinationIdentifier.h"
#include <memory>
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>

#if PLATFORM(COCOA)
#include "SharedCARingBuffer.h"

namespace WebCore {
class CAAudioStreamDescription;
}
#endif

namespace WebKit {

class GPUConnectionToWebProcess;
class RemoteAudioDestination;

class RemoteAudioDestinationManager : private IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(RemoteAudioDestinationManager);
public:
    RemoteAudioDestinationManager(GPUConnectionToWebProcess&);
    ~RemoteAudioDestinationManager();

    bool didReceiveSyncMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder) { return didReceiveSyncMessage(connection, decoder, encoder); }
    void didReceiveMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder) { didReceiveMessage(connection, decoder); }

    bool allowsExitUnderMemoryPressure() const;

private:
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    void createAudioDestination(const String& inputDeviceId, uint32_t numberOfInputChannels, uint32_t numberOfOutputChannels, float sampleRate, float hardwareSampleRate, IPC::Semaphore&& renderSemaphore, CompletionHandler<void(const WebKit::RemoteAudioDestinationIdentifier)>&&);

    void deleteAudioDestination(RemoteAudioDestinationIdentifier, CompletionHandler<void()>&&);
    void startAudioDestination(RemoteAudioDestinationIdentifier, CompletionHandler<void(bool)>&&);
    void stopAudioDestination(RemoteAudioDestinationIdentifier, CompletionHandler<void(bool)>&&);
#if PLATFORM(COCOA)
    void audioSamplesStorageChanged(RemoteAudioDestinationIdentifier, ConsumerSharedCARingBuffer::Handle&&);
#endif

    HashMap<RemoteAudioDestinationIdentifier, UniqueRef<RemoteAudioDestination>> m_audioDestinations;
    GPUConnectionToWebProcess& m_gpuConnectionToWebProcess;
};

} // namespace WebKit;

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)
