/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)

#include "AudioMediaStreamTrackRendererInternalUnitIdentifier.h"
#include "Connection.h"
#include "SharedCARingBuffer.h"
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>

namespace IPC {
class Semaphore;
}

namespace WebCore {
class CAAudioStreamDescription;
}

namespace WebKit {

class GPUConnectionToWebProcess;
class RemoteAudioDestination;

class RemoteAudioMediaStreamTrackRendererInternalUnitManager : private IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(RemoteAudioMediaStreamTrackRendererInternalUnitManager);
public:
    explicit RemoteAudioMediaStreamTrackRendererInternalUnitManager(GPUConnectionToWebProcess&);
    ~RemoteAudioMediaStreamTrackRendererInternalUnitManager();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    bool hasUnits() { return !m_units.isEmpty(); }
    class Unit;

    void notifyLastToCaptureAudioChanged();

private:
    // Messages
    void createUnit(AudioMediaStreamTrackRendererInternalUnitIdentifier, CompletionHandler<void(std::optional<WebCore::CAAudioStreamDescription>, size_t)>&& callback);
    void deleteUnit(AudioMediaStreamTrackRendererInternalUnitIdentifier);
    void startUnit(AudioMediaStreamTrackRendererInternalUnitIdentifier, ConsumerSharedCARingBuffer::Handle&&, IPC::Semaphore&&);
    void stopUnit(AudioMediaStreamTrackRendererInternalUnitIdentifier);
    void setAudioOutputDevice(AudioMediaStreamTrackRendererInternalUnitIdentifier, const String&);

    HashMap<AudioMediaStreamTrackRendererInternalUnitIdentifier, UniqueRef<Unit>> m_units;
    GPUConnectionToWebProcess& m_gpuConnectionToWebProcess;
};

} // namespace WebKit;

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
