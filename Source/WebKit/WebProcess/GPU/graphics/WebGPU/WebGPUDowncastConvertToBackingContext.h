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

#include "WebGPUConvertToBackingContext.h"
#include <wtf/TZoneMalloc.h>

namespace WebKit::WebGPU {

class DowncastConvertToBackingContext final : public ConvertToBackingContext {
    WTF_MAKE_TZONE_ALLOCATED(DowncastConvertToBackingContext);
public:
    static Ref<DowncastConvertToBackingContext> create()
    {
        return adoptRef(*new DowncastConvertToBackingContext());
    }

    virtual ~DowncastConvertToBackingContext() = default;

    WebGPUIdentifier convertToBacking(const ::WebGPU::Adapter&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::BindGroup&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::BindGroupLayout&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::Buffer&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::CommandBuffer&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::CommandEncoder&) final;
    const RemoteCompositorIntegrationProxy& convertToRawBacking(const WebCore::WebGPUCompositorIntegration&) final;
    WebGPUIdentifier convertToBacking(const WebCore::WebGPUCompositorIntegration&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::ComputePassEncoder&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::ComputePipeline&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::Device&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::ExternalTexture&) final;
    WebGPUIdentifier convertToBacking(const WebCore::WebGPUIntegration&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::PipelineLayout&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::PresentationContext&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::QuerySet&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::Queue&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::RenderBundleEncoder&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::RenderBundle&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::RenderPassEncoder&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::RenderPipeline&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::Sampler&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::ShaderModule&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::Texture&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::TextureView&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::XRBinding&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::XRProjectionLayer&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::XRSubImage&) final;
    WebGPUIdentifier convertToBacking(const ::WebGPU::XRView&) final;

private:
    DowncastConvertToBackingContext() = default;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
