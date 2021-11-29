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
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

namespace PAL::WebGPU {
class ComputePipeline;
}

namespace WebKit {

namespace WebGPU {
class ObjectHeap;
class ObjectRegistry;
}

class RemoteComputePipeline final : public IPC::StreamMessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteComputePipeline> create(PAL::WebGPU::ComputePipeline& computePipeline, WebGPU::ObjectRegistry& objectRegistry, WebGPU::ObjectHeap& objectHeap, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteComputePipeline(computePipeline, objectRegistry, objectHeap, identifier));
    }

    virtual ~RemoteComputePipeline();

private:
    friend class ObjectRegistry;

    RemoteComputePipeline(PAL::WebGPU::ComputePipeline&, WebGPU::ObjectRegistry&, WebGPU::ObjectHeap&, WebGPUIdentifier);

    RemoteComputePipeline(const RemoteComputePipeline&) = delete;
    RemoteComputePipeline(RemoteComputePipeline&&) = delete;
    RemoteComputePipeline& operator=(const RemoteComputePipeline&) = delete;
    RemoteComputePipeline& operator=(RemoteComputePipeline&&) = delete;

    void didReceiveStreamMessage(IPC::StreamServerConnectionBase&, IPC::Decoder&) final;

    void getBindGroupLayout(uint32_t index, WebGPUIdentifier);

    void setLabel(String&&);

    Ref<PAL::WebGPU::ComputePipeline> m_backing;
    WebGPU::ObjectRegistry& m_objectRegistry;
    WebGPU::ObjectHeap& m_objectHeap;
    WebGPUIdentifier m_identifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
