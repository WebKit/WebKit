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
#include "WebGPUExtent3D.h"
#include "WebGPUIdentifier.h"
#include <pal/graphics/WebGPU/WebGPUIntegralTypes.h>
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

namespace PAL::WebGPU {
class CommandEncoder;
}

namespace IPC {
class StreamServerConnection;
}

namespace WebKit {

namespace WebGPU {
struct CommandBufferDescriptor;
struct ComputePassDescriptor;
struct ImageCopyBuffer;
struct ImageCopyTexture;
class ObjectHeap;
struct RenderPassDescriptor;
}

class RemoteCommandEncoder final : public IPC::StreamMessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteCommandEncoder> create(PAL::WebGPU::CommandEncoder& commandEncoder, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteCommandEncoder(commandEncoder, objectHeap, WTFMove(streamConnection), identifier));
    }

    virtual ~RemoteCommandEncoder();

    void stopListeningForIPC();

private:
    friend class WebGPU::ObjectHeap;

    RemoteCommandEncoder(PAL::WebGPU::CommandEncoder&, WebGPU::ObjectHeap&, Ref<IPC::StreamServerConnection>&&, WebGPUIdentifier);

    RemoteCommandEncoder(const RemoteCommandEncoder&) = delete;
    RemoteCommandEncoder(RemoteCommandEncoder&&) = delete;
    RemoteCommandEncoder& operator=(const RemoteCommandEncoder&) = delete;
    RemoteCommandEncoder& operator=(RemoteCommandEncoder&&) = delete;

    PAL::WebGPU::CommandEncoder& backing() { return m_backing; }

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void beginRenderPass(const WebGPU::RenderPassDescriptor&, WebGPUIdentifier);
    void beginComputePass(const std::optional<WebGPU::ComputePassDescriptor>&, WebGPUIdentifier);

    void copyBufferToBuffer(
        WebGPUIdentifier source,
        PAL::WebGPU::Size64 sourceOffset,
        WebGPUIdentifier destination,
        PAL::WebGPU::Size64 destinationOffset,
        PAL::WebGPU::Size64);

    void copyBufferToTexture(
        const WebGPU::ImageCopyBuffer& source,
        const WebGPU::ImageCopyTexture& destination,
        const WebGPU::Extent3D& copySize);

    void copyTextureToBuffer(
        const WebGPU::ImageCopyTexture& source,
        const WebGPU::ImageCopyBuffer& destination,
        const WebGPU::Extent3D& copySize);

    void copyTextureToTexture(
        const WebGPU::ImageCopyTexture& source,
        const WebGPU::ImageCopyTexture& destination,
        const WebGPU::Extent3D& copySize);

    void clearBuffer(
        WebGPUIdentifier buffer,
        PAL::WebGPU::Size64 offset = 0,
        std::optional<PAL::WebGPU::Size64> = std::nullopt);

    void pushDebugGroup(String&& groupLabel);
    void popDebugGroup();
    void insertDebugMarker(String&& markerLabel);

    void writeTimestamp(WebGPUIdentifier, PAL::WebGPU::Size32 queryIndex);

    void resolveQuerySet(
        WebGPUIdentifier,
        PAL::WebGPU::Size32 firstQuery,
        PAL::WebGPU::Size32 queryCount,
        WebGPUIdentifier destination,
        PAL::WebGPU::Size64 destinationOffset);

    void finish(const WebGPU::CommandBufferDescriptor&, WebGPUIdentifier);

    void setLabel(String&&);

    Ref<PAL::WebGPU::CommandEncoder> m_backing;
    WebGPU::ObjectHeap& m_objectHeap;
    Ref<IPC::StreamServerConnection> m_streamConnection;
    WebGPUIdentifier m_identifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
