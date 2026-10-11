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

#include "RemoteCommandEncoderProxy.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUCppAPI.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteRenderPassEncoderProxy final : public ::WebGPU::RenderPassEncoder {
    WTF_MAKE_TZONE_ALLOCATED(RemoteRenderPassEncoderProxy);
public:
    static Ref<RemoteRenderPassEncoderProxy> create(RemoteCommandEncoderProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteRenderPassEncoderProxy(parent, convertToBackingContext, identifier));
    }

    virtual ~RemoteRenderPassEncoderProxy();

    RemoteGPUProxy& root() const { return m_root; }

    void setPipeline(const ::WebGPU::RenderPipeline&) final;
    void setIndexBuffer(const ::WebGPU::Buffer&, ::WebGPU::IndexFormat, uint64_t offset, std::optional<uint64_t>) final;
    void setVertexBuffer(uint32_t slot, const ::WebGPU::Buffer*, uint64_t offset, std::optional<uint64_t>) final;
    void draw(uint32_t vertexCount, uint32_t instanceCount,
        uint32_t firstVertex, uint32_t firstInstance) final;
    void drawIndexed(uint32_t indexCount, uint32_t instanceCount,
        uint32_t firstIndex,
        int32_t baseVertex,
        uint32_t firstInstance) final;
    void drawIndirect(const ::WebGPU::Buffer& indirectBuffer, uint64_t indirectOffset) final;
    void drawIndexedIndirect(const ::WebGPU::Buffer& indirectBuffer, uint64_t indirectOffset) final;
    void setBindGroup(uint32_t, const ::WebGPU::BindGroup*, std::optional<std::span<const uint32_t>> dynamicOffsets) final;
    void pushDebugGroup(String&& groupLabel) final;
    void popDebugGroup() final;
    void insertDebugMarker(String&& markerLabel) final;
    void setViewport(float x, float y,
        float width, float height,
        float minDepth, float maxDepth) final;
    void setScissorRect(uint32_t x, uint32_t y,
        uint32_t width, uint32_t height) final;
    void setBlendConstant(const ::WebGPU::Color&) final;
    void setStencilReference(uint32_t) final;
    void beginOcclusionQuery(uint32_t queryIndex) final;
    void endOcclusionQuery() final;
    void executeBundles(std::span<const Ref<::WebGPU::RenderBundle>>) final;
    void end() final;

    void setLabel(String&&) final;
    bool isValid() const final;

private:
    friend class DowncastConvertToBackingContext;

    RemoteRenderPassEncoderProxy(RemoteCommandEncoderProxy&, ConvertToBackingContext&, WebGPUIdentifier);

    RemoteRenderPassEncoderProxy(const RemoteRenderPassEncoderProxy&) = delete;
    RemoteRenderPassEncoderProxy(RemoteRenderPassEncoderProxy&&) = delete;
    RemoteRenderPassEncoderProxy& operator=(const RemoteRenderPassEncoderProxy&) = delete;
    RemoteRenderPassEncoderProxy& operator=(RemoteRenderPassEncoderProxy&&) = delete;

    WebGPUIdentifier backing() const { return m_backing; }
    
    template<typename T>
    [[nodiscard]] IPC::Error send(T&& message)
    {
        return protect(root().streamClientConnection())->send(std::forward<T>(message), backing());
    }


    WebGPUIdentifier m_backing;
    const Ref<ConvertToBackingContext> m_convertToBackingContext;
    const Ref<RemoteGPUProxy> m_root;
};

} // namespace WebKit::WebGPU

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebGPU::RemoteRenderPassEncoderProxy)
    // In the Web Process, every WebGPU::RenderPassEncoder is a RemoteRenderPassEncoderProxy.
    static bool isType(const ::WebGPU::RenderPassEncoder&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS)
