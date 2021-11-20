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

#if ENABLE(GPU_PROCESS)

#include "StreamMessageReceiver.h"
#include "WebGPUIdentifier.h"
#include <pal/graphics/WebGPU/WebGPUIntegralTypes.h>
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

namespace PAL::WebGPU {
class ComputePassEncoder;
}

namespace WebKit {

namespace WebGPU {
class ObjectHeap;
class ObjectRegistry;
}

class RemoteComputePassEncoder final : public IPC::StreamMessageReceiver {
public:
    static Ref<RemoteComputePassEncoder> create(PAL::WebGPU::ComputePassEncoder& computePassEncoder, WebGPU::ObjectRegistry& objectRegistry, WebGPU::ObjectHeap& objectHeap, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteComputePassEncoder(computePassEncoder, objectRegistry, objectHeap, identifier));
    }

    virtual ~RemoteComputePassEncoder();

private:
    friend class ObjectRegistry;

    RemoteComputePassEncoder(PAL::WebGPU::ComputePassEncoder&, WebGPU::ObjectRegistry&, WebGPU::ObjectHeap&, WebGPUIdentifier);

    RemoteComputePassEncoder(const RemoteComputePassEncoder&) = delete;
    RemoteComputePassEncoder(RemoteComputePassEncoder&&) = delete;
    RemoteComputePassEncoder& operator=(const RemoteComputePassEncoder&) = delete;
    RemoteComputePassEncoder& operator=(RemoteComputePassEncoder&&) = delete;

    void didReceiveStreamMessage(IPC::StreamServerConnectionBase&, IPC::Decoder&) final;

    void setPipeline(WebGPUIdentifier);
    void dispatch(PAL::WebGPU::Size32 x, std::optional<PAL::WebGPU::Size32> y, std::optional<PAL::WebGPU::Size32>);
    void dispatchIndirect(WebGPUIdentifier indirectBuffer, PAL::WebGPU::Size64 indirectOffset);

    void beginPipelineStatisticsQuery(WebGPUIdentifier, PAL::WebGPU::Size32 queryIndex);
    void endPipelineStatisticsQuery();

    void endPass();

    void setBindGroup(PAL::WebGPU::Index32, WebGPUIdentifier,
        std::optional<Vector<PAL::WebGPU::BufferDynamicOffset>>&&);

    void pushDebugGroup(String&& groupLabel);
    void popDebugGroup();
    void insertDebugMarker(String&& markerLabel);

    void setLabel(String&&);

    Ref<PAL::WebGPU::ComputePassEncoder> m_backing;
    WebGPU::ObjectRegistry& m_objectRegistry;
    WebGPU::ObjectHeap& m_objectHeap;
    WebGPUIdentifier m_identifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
