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

#include "WebGPUConvertToBackingContext.h"

namespace WebKit::WebGPU {

class DowncastConvertToBackingContext final : public ConvertToBackingContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<DowncastConvertToBackingContext> create()
    {
        return adoptRef(*new DowncastConvertToBackingContext());
    }

    virtual ~DowncastConvertToBackingContext() = default;

    WebGPUIdentifier convertToBacking(const PAL::WebGPU::Adapter&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::BindGroup&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::BindGroupLayout&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::Buffer&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::CommandBuffer&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::CommandEncoder&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::ComputePassEncoder&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::ComputePipeline&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::Device&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::ExternalTexture&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::GPU&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::PipelineLayout&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::QuerySet&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::Queue&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::RenderBundleEncoder&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::RenderBundle&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::RenderPassEncoder&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::RenderPipeline&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::Sampler&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::ShaderModule&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::Surface&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::SwapChain&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::Texture&) final;
    WebGPUIdentifier convertToBacking(const PAL::WebGPU::TextureView&) final;

private:
    DowncastConvertToBackingContext() = default;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
