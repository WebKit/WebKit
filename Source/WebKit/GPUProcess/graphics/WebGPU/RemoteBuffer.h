/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#include "StreamMessageReceiver.h"
#include "WebGPUIdentifier.h"
#include <pal/graphics/WebGPU/WebGPUBuffer.h>
#include <pal/graphics/WebGPU/WebGPUIntegralTypes.h>
#include <pal/graphics/WebGPU/WebGPUMapMode.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace PAL::WebGPU {
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
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteBuffer> create(PAL::WebGPU::Buffer& buffer, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteBuffer(buffer, objectHeap, WTFMove(streamConnection), identifier));
    }

    virtual ~RemoteBuffer();

    void stopListeningForIPC();

private:
    friend class WebGPU::ObjectHeap;

    RemoteBuffer(PAL::WebGPU::Buffer&, WebGPU::ObjectHeap&, Ref<IPC::StreamServerConnection>&&, WebGPUIdentifier);

    RemoteBuffer(const RemoteBuffer&) = delete;
    RemoteBuffer(RemoteBuffer&&) = delete;
    RemoteBuffer& operator=(const RemoteBuffer&) = delete;
    RemoteBuffer& operator=(RemoteBuffer&&) = delete;

    PAL::WebGPU::Buffer& backing() { return m_backing; }

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void getMappedRange(PAL::WebGPU::Size64 offset, std::optional<PAL::WebGPU::Size64> sizeForMap, CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&&);
    void mapAsync(PAL::WebGPU::MapModeFlags, PAL::WebGPU::Size64 offset, std::optional<PAL::WebGPU::Size64> sizeForMap, CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&&);
    void unmap(Vector<uint8_t>&&);

    void destroy();

    void setLabel(String&&);

    Ref<PAL::WebGPU::Buffer> m_backing;
    WebGPU::ObjectHeap& m_objectHeap;
    Ref<IPC::StreamServerConnection> m_streamConnection;
    WebGPUIdentifier m_identifier;
    bool m_isMapped { false };
    std::optional<PAL::WebGPU::Buffer::MappedRange> m_mappedRange;
    PAL::WebGPU::MapModeFlags m_mapModeFlags;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
