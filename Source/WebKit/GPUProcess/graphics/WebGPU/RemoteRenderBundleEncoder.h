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

#include "StreamMessageReceiver.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUIndexFormat.h>
#include <WebCore/WebGPUIntegralTypes.h>
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

namespace WebCore::WebGPU {
class RenderBundleEncoder;
}

namespace IPC {
class StreamServerConnection;
}

namespace WebKit {

namespace WebGPU {
class ObjectHeap;
struct RenderBundleDescriptor;
}

class RemoteRenderBundleEncoder final : public IPC::StreamMessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteRenderBundleEncoder> create(WebCore::WebGPU::RenderBundleEncoder& renderBundleEncoder, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteRenderBundleEncoder(renderBundleEncoder, objectHeap, WTFMove(streamConnection), identifier));
    }

    ~RemoteRenderBundleEncoder();

    void stopListeningForIPC();

private:
    friend class WebGPU::ObjectHeap;

    RemoteRenderBundleEncoder(WebCore::WebGPU::RenderBundleEncoder&, WebGPU::ObjectHeap&, Ref<IPC::StreamServerConnection>&&, WebGPUIdentifier);

    RemoteRenderBundleEncoder(const RemoteRenderBundleEncoder&) = delete;
    RemoteRenderBundleEncoder(RemoteRenderBundleEncoder&&) = delete;
    RemoteRenderBundleEncoder& operator=(const RemoteRenderBundleEncoder&) = delete;
    RemoteRenderBundleEncoder& operator=(RemoteRenderBundleEncoder&&) = delete;

    WebCore::WebGPU::RenderBundleEncoder& backing() { return m_backing; }

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void setPipeline(WebGPUIdentifier);

    void setIndexBuffer(WebGPUIdentifier, WebCore::WebGPU::IndexFormat, std::optional<WebCore::WebGPU::Size64> offset, std::optional<WebCore::WebGPU::Size64>);
    void setVertexBuffer(WebCore::WebGPU::Index32 slot, WebGPUIdentifier, std::optional<WebCore::WebGPU::Size64> offset, std::optional<WebCore::WebGPU::Size64>);
    void unsetVertexBuffer(WebCore::WebGPU::Index32 slot, std::optional<WebCore::WebGPU::Size64> offset, std::optional<WebCore::WebGPU::Size64>);

    void draw(WebCore::WebGPU::Size32 vertexCount, std::optional<WebCore::WebGPU::Size32> instanceCount,
        std::optional<WebCore::WebGPU::Size32> firstVertex, std::optional<WebCore::WebGPU::Size32> firstInstance);
    void drawIndexed(WebCore::WebGPU::Size32 indexCount, std::optional<WebCore::WebGPU::Size32> instanceCount,
        std::optional<WebCore::WebGPU::Size32> firstIndex,
        std::optional<WebCore::WebGPU::SignedOffset32> baseVertex,
        std::optional<WebCore::WebGPU::Size32> firstInstance);

    void drawIndirect(WebGPUIdentifier indirectBuffer, WebCore::WebGPU::Size64 indirectOffset);
    void drawIndexedIndirect(WebGPUIdentifier indirectBuffer, WebCore::WebGPU::Size64 indirectOffset);

    void setBindGroup(WebCore::WebGPU::Index32, WebGPUIdentifier,
        std::optional<Vector<WebCore::WebGPU::BufferDynamicOffset>>&& dynamicOffsets);

    void pushDebugGroup(String&& groupLabel);
    void popDebugGroup();
    void insertDebugMarker(String&& markerLabel);

    void finish(const WebGPU::RenderBundleDescriptor&, WebGPUIdentifier);

    void setLabel(String&&);
    void destruct();

    Ref<WebCore::WebGPU::RenderBundleEncoder> m_backing;
    WebGPU::ObjectHeap& m_objectHeap;
    Ref<IPC::StreamServerConnection> m_streamConnection;
    WebGPUIdentifier m_identifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
