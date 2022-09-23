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

#include "RemoteDeviceProxy.h"
#include "WebGPUIdentifier.h"
#include <pal/graphics/WebGPU/WebGPUCommandEncoder.h>

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteCommandEncoderProxy final : public PAL::WebGPU::CommandEncoder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteCommandEncoderProxy> create(RemoteDeviceProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteCommandEncoderProxy(parent, convertToBackingContext, identifier));
    }

    virtual ~RemoteCommandEncoderProxy();

    RemoteDeviceProxy& parent() { return m_parent; }
    RemoteGPUProxy& root() { return m_parent->root(); }

private:
    friend class DowncastConvertToBackingContext;

    RemoteCommandEncoderProxy(RemoteDeviceProxy&, ConvertToBackingContext&, WebGPUIdentifier);

    RemoteCommandEncoderProxy(const RemoteCommandEncoderProxy&) = delete;
    RemoteCommandEncoderProxy(RemoteCommandEncoderProxy&&) = delete;
    RemoteCommandEncoderProxy& operator=(const RemoteCommandEncoderProxy&) = delete;
    RemoteCommandEncoderProxy& operator=(RemoteCommandEncoderProxy&&) = delete;

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

    Ref<PAL::WebGPU::RenderPassEncoder> beginRenderPass(const PAL::WebGPU::RenderPassDescriptor&) final;
    Ref<PAL::WebGPU::ComputePassEncoder> beginComputePass(const std::optional<PAL::WebGPU::ComputePassDescriptor>&) final;

    void copyBufferToBuffer(
        const PAL::WebGPU::Buffer& source,
        PAL::WebGPU::Size64 sourceOffset,
        const PAL::WebGPU::Buffer& destination,
        PAL::WebGPU::Size64 destinationOffset,
        PAL::WebGPU::Size64) final;

    void copyBufferToTexture(
        const PAL::WebGPU::ImageCopyBuffer& source,
        const PAL::WebGPU::ImageCopyTexture& destination,
        const PAL::WebGPU::Extent3D& copySize) final;

    void copyTextureToBuffer(
        const PAL::WebGPU::ImageCopyTexture& source,
        const PAL::WebGPU::ImageCopyBuffer& destination,
        const PAL::WebGPU::Extent3D& copySize) final;

    void copyTextureToTexture(
        const PAL::WebGPU::ImageCopyTexture& source,
        const PAL::WebGPU::ImageCopyTexture& destination,
        const PAL::WebGPU::Extent3D& copySize) final;

    void clearBuffer(
        const PAL::WebGPU::Buffer&,
        PAL::WebGPU::Size64 offset = 0,
        std::optional<PAL::WebGPU::Size64> = std::nullopt) final;

    void pushDebugGroup(String&& groupLabel) final;
    void popDebugGroup() final;
    void insertDebugMarker(String&& markerLabel) final;

    void writeTimestamp(const PAL::WebGPU::QuerySet&, PAL::WebGPU::Size32 queryIndex) final;

    void resolveQuerySet(
        const PAL::WebGPU::QuerySet&,
        PAL::WebGPU::Size32 firstQuery,
        PAL::WebGPU::Size32 queryCount,
        const PAL::WebGPU::Buffer& destination,
        PAL::WebGPU::Size64 destinationOffset) final;

    Ref<PAL::WebGPU::CommandBuffer> finish(const PAL::WebGPU::CommandBufferDescriptor&) final;

    void setLabelInternal(const String&) final;

    WebGPUIdentifier m_backing;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
    Ref<RemoteDeviceProxy> m_parent;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
