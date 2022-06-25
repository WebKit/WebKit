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

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUComputePassEncoder.h"
#include <WebGPU/WebGPU.h>

namespace PAL::WebGPU {

class ConvertToBackingContext;

class ComputePassEncoderImpl final : public ComputePassEncoder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<ComputePassEncoderImpl> create(WGPUComputePassEncoder computePassEncoder, ConvertToBackingContext& convertToBackingContext)
    {
        return adoptRef(*new ComputePassEncoderImpl(computePassEncoder, convertToBackingContext));
    }

    virtual ~ComputePassEncoderImpl();

private:
    friend class DowncastConvertToBackingContext;

    ComputePassEncoderImpl(WGPUComputePassEncoder, ConvertToBackingContext&);

    ComputePassEncoderImpl(const ComputePassEncoderImpl&) = delete;
    ComputePassEncoderImpl(ComputePassEncoderImpl&&) = delete;
    ComputePassEncoderImpl& operator=(const ComputePassEncoderImpl&) = delete;
    ComputePassEncoderImpl& operator=(ComputePassEncoderImpl&&) = delete;

    WGPUComputePassEncoder backing() const { return m_backing; }

    void setPipeline(const ComputePipeline&) final;
    void dispatch(Size32 workgroupCountX, Size32 workgroupCountY, Size32 workgroupCountZ) final;
    void dispatchIndirect(const Buffer& indirectBuffer, Size64 indirectOffset) final;

    void end() final;

    void setBindGroup(Index32, const BindGroup&,
        std::optional<Vector<BufferDynamicOffset>>&&) final;

    void setBindGroup(Index32, const BindGroup&,
        const uint32_t* dynamicOffsetsArrayBuffer,
        size_t dynamicOffsetsArrayBufferLength,
        Size64 dynamicOffsetsDataStart,
        Size32 dynamicOffsetsDataLength) final;

    void pushDebugGroup(String&& groupLabel) final;
    void popDebugGroup() final;
    void insertDebugMarker(String&& markerLabel) final;

    void setLabelInternal(const String&) final;

    WGPUComputePassEncoder m_backing { nullptr };
    Ref<ConvertToBackingContext> m_convertToBackingContext;
};

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
