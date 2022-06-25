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

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPURenderBundleEncoder.h"
#include <WebGPU/WebGPU.h>

namespace PAL::WebGPU {

class ConvertToBackingContext;

class RenderBundleEncoderImpl final : public RenderBundleEncoder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RenderBundleEncoderImpl> create(WGPURenderBundleEncoder renderBundleEncoder, ConvertToBackingContext& convertToBackingContext)
    {
        return adoptRef(*new RenderBundleEncoderImpl(renderBundleEncoder, convertToBackingContext));
    }

    virtual ~RenderBundleEncoderImpl();

private:
    friend class DowncastConvertToBackingContext;

    RenderBundleEncoderImpl(WGPURenderBundleEncoder, ConvertToBackingContext&);

    RenderBundleEncoderImpl(const RenderBundleEncoderImpl&) = delete;
    RenderBundleEncoderImpl(RenderBundleEncoderImpl&&) = delete;
    RenderBundleEncoderImpl& operator=(const RenderBundleEncoderImpl&) = delete;
    RenderBundleEncoderImpl& operator=(RenderBundleEncoderImpl&&) = delete;

    WGPURenderBundleEncoder backing() const { return m_backing; }

    void setPipeline(const RenderPipeline&) final;

    void setIndexBuffer(const Buffer&, IndexFormat, Size64 offset, std::optional<Size64>) final;
    void setVertexBuffer(Index32 slot, const Buffer&, Size64 offset, std::optional<Size64>) final;

    void draw(Size32 vertexCount, Size32 instanceCount,
        Size32 firstVertex, Size32 firstInstance) final;
    void drawIndexed(Size32 indexCount, Size32 instanceCount,
        Size32 firstIndex,
        SignedOffset32 baseVertex,
        Size32 firstInstance) final;

    void drawIndirect(const Buffer& indirectBuffer, Size64 indirectOffset) final;
    void drawIndexedIndirect(const Buffer& indirectBuffer, Size64 indirectOffset) final;

    void setBindGroup(Index32, const BindGroup&,
        std::optional<Vector<BufferDynamicOffset>>&& dynamicOffsets) final;

    void setBindGroup(Index32, const BindGroup&,
        const uint32_t* dynamicOffsetsArrayBuffer,
        size_t dynamicOffsetsArrayBufferLength,
        Size64 dynamicOffsetsDataStart,
        Size32 dynamicOffsetsDataLength) final;

    void pushDebugGroup(String&& groupLabel) final;
    void popDebugGroup() final;
    void insertDebugMarker(String&& markerLabel) final;

    Ref<RenderBundle> finish(const RenderBundleDescriptor&) final;

    void setLabelInternal(const String&) final;

    WGPURenderBundleEncoder m_backing { nullptr };
    Ref<ConvertToBackingContext> m_convertToBackingContext;
};

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
