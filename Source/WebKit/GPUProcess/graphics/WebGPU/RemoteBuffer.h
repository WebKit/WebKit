/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "RemoteGPU.h"
#include "StreamMessageReceiver.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUBuffer.h>
#include <WebCore/WebGPUIntegralTypes.h>
#include <WebCore/WebGPUMapMode.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakRef.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class SharedMemoryHandle;
}

namespace WebCore::WebGPU {
class Buffer;
}

namespace IPC {
class StreamServerConnection;
}

namespace WebKit {

namespace WebGPU {
class ObjectHeap;
}

class RemoteBuffer final : public IPC::StreamMessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteBuffer);
public:
    static Ref<RemoteBuffer> create(WebCore::WebGPU::Buffer& buffer, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, RemoteGPU& gpu, bool mappedAtCreation, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteBuffer(buffer, objectHeap, WTFMove(streamConnection), gpu, mappedAtCreation, identifier));
    }

    virtual ~RemoteBuffer();

    const SharedPreferencesForWebProcess& sharedPreferencesForWebProcess() const { return m_gpu->sharedPreferencesForWebProcess(); }

    void stopListeningForIPC();

private:
    friend class WebGPU::ObjectHeap;

    RemoteBuffer(WebCore::WebGPU::Buffer&, WebGPU::ObjectHeap&, Ref<IPC::StreamServerConnection>&&, RemoteGPU&, bool mappedAtCreation, WebGPUIdentifier);

    RemoteBuffer(const RemoteBuffer&) = delete;
    RemoteBuffer(RemoteBuffer&&) = delete;
    RemoteBuffer& operator=(const RemoteBuffer&) = delete;
    RemoteBuffer& operator=(RemoteBuffer&&) = delete;

    WebCore::WebGPU::Buffer& backing() { return m_backing; }

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void getMappedRange(WebCore::WebGPU::Size64 offset, std::optional<WebCore::WebGPU::Size64> sizeForMap, CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&&);
    void mapAsync(WebCore::WebGPU::MapModeFlags, WebCore::WebGPU::Size64 offset, std::optional<WebCore::WebGPU::Size64> sizeForMap, CompletionHandler<void(bool)>&&);
    void copy(std::optional<WebCore::SharedMemoryHandle>&&, size_t offset, CompletionHandler<void(bool)>&&);
    void unmap();

    void destroy();
    void destruct();

    void setLabel(String&&);

    Ref<WebCore::WebGPU::Buffer> m_backing;
    WeakRef<WebGPU::ObjectHeap> m_objectHeap;
    Ref<IPC::StreamServerConnection> m_streamConnection;
    WeakRef<RemoteGPU> m_gpu;
    WebGPUIdentifier m_identifier;
    bool m_isMapped { false };
    WebCore::WebGPU::MapModeFlags m_mapModeFlags;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
