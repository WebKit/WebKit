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
#include <pal/graphics/WebGPU/WebGPUIntegralTypes.h>
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

namespace PAL::WebGPU {
class ComputePassEncoder;
}

namespace IPC {
class StreamServerConnection;
}

namespace WebKit {

namespace WebGPU {
class ObjectHeap;
}

class RemoteComputePassEncoder final : public IPC::StreamMessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteComputePassEncoder> create(PAL::WebGPU::ComputePassEncoder& computePassEncoder, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteComputePassEncoder(computePassEncoder, objectHeap, WTFMove(streamConnection), identifier));
    }

    virtual ~RemoteComputePassEncoder();

    void stopListeningForIPC();

private:
    friend class WebGPU::ObjectHeap;

    RemoteComputePassEncoder(PAL::WebGPU::ComputePassEncoder&, WebGPU::ObjectHeap&, Ref<IPC::StreamServerConnection>&&, WebGPUIdentifier);

    RemoteComputePassEncoder(const RemoteComputePassEncoder&) = delete;
    RemoteComputePassEncoder(RemoteComputePassEncoder&&) = delete;
    RemoteComputePassEncoder& operator=(const RemoteComputePassEncoder&) = delete;
    RemoteComputePassEncoder& operator=(RemoteComputePassEncoder&&) = delete;

    PAL::WebGPU::ComputePassEncoder& backing() { return m_backing; }

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void setPipeline(WebGPUIdentifier);
    void dispatch(PAL::WebGPU::Size32 workgroupCountX, PAL::WebGPU::Size32 workgroupCountY = 1, PAL::WebGPU::Size32 workgroupCountZ = 1);
    void dispatchIndirect(WebGPUIdentifier indirectBuffer, PAL::WebGPU::Size64 indirectOffset);

    void end();

    void setBindGroup(PAL::WebGPU::Index32, WebGPUIdentifier,
        std::optional<Vector<PAL::WebGPU::BufferDynamicOffset>>&&);

    void pushDebugGroup(String&& groupLabel);
    void popDebugGroup();
    void insertDebugMarker(String&& markerLabel);

    void setLabel(String&&);

    Ref<PAL::WebGPU::ComputePassEncoder> m_backing;
    WebGPU::ObjectHeap& m_objectHeap;
    Ref<IPC::StreamServerConnection> m_streamConnection;
    WebGPUIdentifier m_identifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
