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
#include "WebGPUColor.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUCppAPI.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class StreamServerConnection;
}

namespace WebKit {

namespace WebGPU {
class ObjectHeap;
}

class RemoteRenderPassEncoder final : public IPC::StreamMessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteRenderPassEncoder);
public:
    static Ref<RemoteRenderPassEncoder> create(::WebGPU::RenderPassEncoder& renderPassEncoder, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, RemoteGPU& gpu, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteRenderPassEncoder(renderPassEncoder, objectHeap, WTF::move(streamConnection), gpu, identifier));
    }

    ~RemoteRenderPassEncoder();

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const { return m_gpu->sharedPreferencesForWebProcess(); }

    void stopListeningForIPC();

private:
    friend class WebGPU::ObjectHeap;

    RemoteRenderPassEncoder(::WebGPU::RenderPassEncoder&, WebGPU::ObjectHeap&, Ref<IPC::StreamServerConnection>&&, RemoteGPU&, WebGPUIdentifier);

    RemoteRenderPassEncoder(const RemoteRenderPassEncoder&) = delete;
    RemoteRenderPassEncoder(RemoteRenderPassEncoder&&) = delete;
    RemoteRenderPassEncoder& operator=(const RemoteRenderPassEncoder&) = delete;
    RemoteRenderPassEncoder& operator=(RemoteRenderPassEncoder&&) = delete;

    ::WebGPU::RenderPassEncoder& backing() { return m_backing; }

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void setPipeline(WebGPUIdentifier);

    void setIndexBuffer(WebGPUIdentifier, ::WebGPU::IndexFormat, uint64_t offset, std::optional<uint64_t>);
    void setVertexBuffer(uint32_t slot, WebGPUIdentifier, uint64_t offset, std::optional<uint64_t>);
    void unsetVertexBuffer(uint32_t slot, uint64_t offset, std::optional<uint64_t>);

    void draw(uint32_t vertexCount, uint32_t instanceCount,
        uint32_t firstVertex, uint32_t firstInstance);
    void drawIndexed(uint32_t indexCount, uint32_t instanceCount,
        uint32_t firstIndex,
        int32_t baseVertex,
        uint32_t firstInstance);

    void drawIndirect(WebGPUIdentifier indirectBuffer, uint64_t indirectOffset);
    void drawIndexedIndirect(WebGPUIdentifier indirectBuffer, uint64_t indirectOffset);

    void setBindGroup(uint32_t, std::optional<WebGPUIdentifier>,
        std::optional<Vector<uint32_t>>&& dynamicOffsets);

    void pushDebugGroup(String&& groupLabel);
    void popDebugGroup();
    void insertDebugMarker(String&& markerLabel);

    void setViewport(float x, float y,
        float width, float height,
        float minDepth, float maxDepth);

    void setScissorRect(uint32_t x, uint32_t y,
        uint32_t width, uint32_t height);

    void setBlendConstant(WebGPU::Color);
    void setStencilReference(uint32_t);

    void beginOcclusionQuery(uint32_t queryIndex);
    void endOcclusionQuery();

    void executeBundles(Vector<WebGPUIdentifier>&&);
    void end();

    void setLabel(String&&);
    void destruct();

    const Ref<::WebGPU::RenderPassEncoder> m_backing;
    WeakRef<WebGPU::ObjectHeap> m_objectHeap;
    const Ref<IPC::StreamServerConnection> m_streamConnection;
    WeakRef<RemoteGPU> m_gpu;
    WebGPUIdentifier m_identifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
