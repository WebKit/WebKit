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
#include "WebGPUColor.h"
#include "WebGPUIdentifier.h"
#include <pal/graphics/WebGPU/WebGPUIndexFormat.h>
#include <pal/graphics/WebGPU/WebGPUIntegralTypes.h>
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

namespace PAL::WebGPU {
class RenderPassEncoder;
}

namespace IPC {
class StreamServerConnection;
}

namespace WebKit {

namespace WebGPU {
class ObjectHeap;
}

class RemoteRenderPassEncoder final : public IPC::StreamMessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteRenderPassEncoder> create(PAL::WebGPU::RenderPassEncoder& renderPassEncoder, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteRenderPassEncoder(renderPassEncoder, objectHeap, WTFMove(streamConnection), identifier));
    }

    ~RemoteRenderPassEncoder();

    void stopListeningForIPC();

private:
    friend class WebGPU::ObjectHeap;

    RemoteRenderPassEncoder(PAL::WebGPU::RenderPassEncoder&, WebGPU::ObjectHeap&, Ref<IPC::StreamServerConnection>&&, WebGPUIdentifier);

    RemoteRenderPassEncoder(const RemoteRenderPassEncoder&) = delete;
    RemoteRenderPassEncoder(RemoteRenderPassEncoder&&) = delete;
    RemoteRenderPassEncoder& operator=(const RemoteRenderPassEncoder&) = delete;
    RemoteRenderPassEncoder& operator=(RemoteRenderPassEncoder&&) = delete;

    PAL::WebGPU::RenderPassEncoder& backing() { return m_backing; }

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void setPipeline(WebGPUIdentifier);

    void setIndexBuffer(WebGPUIdentifier, PAL::WebGPU::IndexFormat, std::optional<PAL::WebGPU::Size64> offset, std::optional<PAL::WebGPU::Size64>);
    void setVertexBuffer(PAL::WebGPU::Index32 slot, WebGPUIdentifier, std::optional<PAL::WebGPU::Size64> offset, std::optional<PAL::WebGPU::Size64>);

    void draw(PAL::WebGPU::Size32 vertexCount, std::optional<PAL::WebGPU::Size32> instanceCount,
        std::optional<PAL::WebGPU::Size32> firstVertex, std::optional<PAL::WebGPU::Size32> firstInstance);
    void drawIndexed(PAL::WebGPU::Size32 indexCount, std::optional<PAL::WebGPU::Size32> instanceCount,
        std::optional<PAL::WebGPU::Size32> firstIndex,
        std::optional<PAL::WebGPU::SignedOffset32> baseVertex,
        std::optional<PAL::WebGPU::Size32> firstInstance);

    void drawIndirect(WebGPUIdentifier indirectBuffer, PAL::WebGPU::Size64 indirectOffset);
    void drawIndexedIndirect(WebGPUIdentifier indirectBuffer, PAL::WebGPU::Size64 indirectOffset);

    void setBindGroup(PAL::WebGPU::Index32, WebGPUIdentifier,
        std::optional<Vector<PAL::WebGPU::BufferDynamicOffset>>&& dynamicOffsets);

    void pushDebugGroup(String&& groupLabel);
    void popDebugGroup();
    void insertDebugMarker(String&& markerLabel);

    void setViewport(float x, float y,
        float width, float height,
        float minDepth, float maxDepth);

    void setScissorRect(PAL::WebGPU::IntegerCoordinate x, PAL::WebGPU::IntegerCoordinate y,
        PAL::WebGPU::IntegerCoordinate width, PAL::WebGPU::IntegerCoordinate height);

    void setBlendConstant(WebGPU::Color);
    void setStencilReference(PAL::WebGPU::StencilValue);

    void beginOcclusionQuery(PAL::WebGPU::Size32 queryIndex);
    void endOcclusionQuery();

    void executeBundles(Vector<WebGPUIdentifier>&&);
    void end();

    void setLabel(String&&);

    Ref<PAL::WebGPU::RenderPassEncoder> m_backing;
    WebGPU::ObjectHeap& m_objectHeap;
    Ref<IPC::StreamServerConnection> m_streamConnection;
    WebGPUIdentifier m_identifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
