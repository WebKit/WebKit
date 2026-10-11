/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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
#include <WebCore/WebGPUCppAPI.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteCommandEncoderProxy final : public ::WebGPU::CommandEncoder {
    WTF_MAKE_TZONE_ALLOCATED(RemoteCommandEncoderProxy);
public:
    static Ref<RemoteCommandEncoderProxy> create(RemoteGPUProxy& root, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteCommandEncoderProxy(root, convertToBackingContext, identifier));
    }

    virtual ~RemoteCommandEncoderProxy();

    RemoteGPUProxy& root() const { return m_root; }

    RefPtr<::WebGPU::RenderPassEncoder> beginRenderPass(const ::WebGPU::RenderPassDescriptor&) final;
    RefPtr<::WebGPU::ComputePassEncoder> beginComputePass(const std::optional<::WebGPU::ComputePassDescriptor>&) final;
    void copyBufferToBuffer(
        const ::WebGPU::Buffer& source,
        uint64_t sourceOffset,
        const ::WebGPU::Buffer& destination,
        uint64_t destinationOffset,
        uint64_t) final;
    void copyBufferToTexture(
        const ::WebGPU::TexelCopyBufferInfo& source,
        const ::WebGPU::TexelCopyTextureInfo& destination,
        const ::WebGPU::Extent3D& copySize) final;
    void copyTextureToBuffer(
        const ::WebGPU::TexelCopyTextureInfo& source,
        const ::WebGPU::TexelCopyBufferInfo& destination,
        const ::WebGPU::Extent3D& copySize) final;
    void copyTextureToTexture(
        const ::WebGPU::TexelCopyTextureInfo& source,
        const ::WebGPU::TexelCopyTextureInfo& destination,
        const ::WebGPU::Extent3D& copySize) final;
    void clearBuffer(
        const ::WebGPU::Buffer&,
        uint64_t offset = 0,
        std::optional<uint64_t> = std::nullopt) final;
    void pushDebugGroup(String&& groupLabel) final;
    void popDebugGroup() final;
    void insertDebugMarker(String&& markerLabel) final;
    void writeTimestamp(const ::WebGPU::QuerySet&, uint32_t queryIndex) final;
    void resolveQuerySet(
        const ::WebGPU::QuerySet&,
        uint32_t firstQuery,
        uint32_t queryCount,
        const ::WebGPU::Buffer& destination,
        uint64_t destinationOffset) final;
    RefPtr<::WebGPU::CommandBuffer> finish(const ::WebGPU::CommandBufferDescriptor&) final;

    void setLabel(String&&) final;
    bool isValid() const final;

private:
    friend class DowncastConvertToBackingContext;

    RemoteCommandEncoderProxy(RemoteGPUProxy&, ConvertToBackingContext&, WebGPUIdentifier);

    RemoteCommandEncoderProxy(const RemoteCommandEncoderProxy&) = delete;
    RemoteCommandEncoderProxy(RemoteCommandEncoderProxy&&) = delete;
    RemoteCommandEncoderProxy& operator=(const RemoteCommandEncoderProxy&) = delete;
    RemoteCommandEncoderProxy& operator=(RemoteCommandEncoderProxy&&) = delete;

    WebGPUIdentifier backing() const { return m_backing; }
    
    template<typename T>
    [[nodiscard]] IPC::Error send(T&& message)
    {
        return protect(root().streamClientConnection())->send(std::forward<T>(message), backing());
    }
    template<typename T>
    [[nodiscard]] IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return protect(root().streamClientConnection())->sendSync(std::forward<T>(message), backing());
    }

    WebGPUIdentifier m_backing;
    const Ref<ConvertToBackingContext> m_convertToBackingContext;
    const Ref<RemoteGPUProxy> m_root;
};

} // namespace WebKit::WebGPU

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebGPU::RemoteCommandEncoderProxy)
    // In the Web Process, every WebGPU::CommandEncoder is a RemoteCommandEncoderProxy.
    static bool isType(const ::WebGPU::CommandEncoder&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS)
