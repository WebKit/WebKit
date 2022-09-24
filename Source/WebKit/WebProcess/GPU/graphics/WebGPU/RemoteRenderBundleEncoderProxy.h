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

#include "RemoteDeviceProxy.h"
#include "WebGPUIdentifier.h"
#include <pal/graphics/WebGPU/WebGPURenderBundleEncoder.h>

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteRenderBundleEncoderProxy final : public PAL::WebGPU::RenderBundleEncoder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteRenderBundleEncoderProxy> create(RemoteDeviceProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteRenderBundleEncoderProxy(parent, convertToBackingContext, identifier));
    }

    virtual ~RemoteRenderBundleEncoderProxy();

    RemoteDeviceProxy& parent() { return m_parent; }
    RemoteGPUProxy& root() { return m_parent->root(); }

private:
    friend class DowncastConvertToBackingContext;

    RemoteRenderBundleEncoderProxy(RemoteDeviceProxy&, ConvertToBackingContext&, WebGPUIdentifier);

    RemoteRenderBundleEncoderProxy(const RemoteRenderBundleEncoderProxy&) = delete;
    RemoteRenderBundleEncoderProxy(RemoteRenderBundleEncoderProxy&&) = delete;
    RemoteRenderBundleEncoderProxy& operator=(const RemoteRenderBundleEncoderProxy&) = delete;
    RemoteRenderBundleEncoderProxy& operator=(RemoteRenderBundleEncoderProxy&&) = delete;

    WebGPUIdentifier backing() const { return m_backing; }
    
    static inline constexpr Seconds defaultSendTimeout = 30_s;
    template<typename T>
    WARN_UNUSED_RETURN bool send(T&& message)
    {
        return root().streamClientConnection().send(WTFMove(message), backing(), defaultSendTimeout);
    }
    template<typename T>
    WARN_UNUSED_RETURN IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return root().streamClientConnection().sendSync(WTFMove(message), backing(), defaultSendTimeout);
    }

    void setPipeline(const PAL::WebGPU::RenderPipeline&) final;

    void setIndexBuffer(const PAL::WebGPU::Buffer&, PAL::WebGPU::IndexFormat, PAL::WebGPU::Size64 offset, std::optional<PAL::WebGPU::Size64>) final;
    void setVertexBuffer(PAL::WebGPU::Index32 slot, const PAL::WebGPU::Buffer&, PAL::WebGPU::Size64 offset, std::optional<PAL::WebGPU::Size64>) final;

    void draw(PAL::WebGPU::Size32 vertexCount, PAL::WebGPU::Size32 instanceCount,
        PAL::WebGPU::Size32 firstVertex, PAL::WebGPU::Size32 firstInstance) final;
    void drawIndexed(PAL::WebGPU::Size32 indexCount, PAL::WebGPU::Size32 instanceCount,
        PAL::WebGPU::Size32 firstIndex,
        PAL::WebGPU::SignedOffset32 baseVertex,
        PAL::WebGPU::Size32 firstInstance) final;

    void drawIndirect(const PAL::WebGPU::Buffer& indirectBuffer, PAL::WebGPU::Size64 indirectOffset) final;
    void drawIndexedIndirect(const PAL::WebGPU::Buffer& indirectBuffer, PAL::WebGPU::Size64 indirectOffset) final;

    void setBindGroup(PAL::WebGPU::Index32, const PAL::WebGPU::BindGroup&,
        std::optional<Vector<PAL::WebGPU::BufferDynamicOffset>>&& dynamicOffsets) final;

    void setBindGroup(PAL::WebGPU::Index32, const PAL::WebGPU::BindGroup&,
        const uint32_t* dynamicOffsetsArrayBuffer,
        size_t dynamicOffsetsArrayBufferLength,
        PAL::WebGPU::Size64 dynamicOffsetsDataStart,
        PAL::WebGPU::Size32 dynamicOffsetsDataLength) final;

    void pushDebugGroup(String&& groupLabel) final;
    void popDebugGroup() final;
    void insertDebugMarker(String&& markerLabel) final;

    Ref<PAL::WebGPU::RenderBundle> finish(const PAL::WebGPU::RenderBundleDescriptor&) final;

    void setLabelInternal(const String&) final;

    WebGPUIdentifier m_backing;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
    Ref<RemoteDeviceProxy> m_parent;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
