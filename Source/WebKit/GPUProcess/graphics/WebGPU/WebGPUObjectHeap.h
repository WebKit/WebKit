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

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUIdentifier.h"
#include <variant>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>

namespace WebKit::WebGPU {

class ObjectHeap final : public WebGPU::ConvertFromBackingContext {
public:
    static Ref<ObjectHeap> create()
    {
        return adoptRef(*new ObjectHeap());
    }

    ~ObjectHeap();

private:
    ObjectHeap();

    PAL::WebGPU::Adapter* convertAdapterFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::BindGroup* convertBindGroupFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::BindGroupLayout* convertBindGroupLayoutFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::Buffer* convertBufferFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::CommandBuffer* convertCommandBufferFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::CommandEncoder* convertCommandEncoderFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::ComputePassEncoder* convertComputePassEncoderFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::ComputePipeline* convertComputePipelineFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::Device* convertDeviceFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::ExternalTexture* convertExternalTextureFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::GPU* convertGPUFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::PipelineLayout* convertPipelineLayoutFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::QuerySet* convertQuerySetFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::Queue* convertQueueFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::RenderBundleEncoder* convertRenderBundleEncoderFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::RenderBundle* convertRenderBundleFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::RenderPassEncoder* convertRenderPassEncoderFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::RenderPipeline* convertRenderPipelineFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::Sampler* convertSamplerFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::ShaderModule* convertShaderModuleFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::Texture* convertTextureFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::TextureView* convertTextureViewFromBacking(WebGPUIdentifier) final;

    using Object = std::variant<
        Ref<PAL::WebGPU::Adapter>,
        Ref<PAL::WebGPU::BindGroup>,
        Ref<PAL::WebGPU::BindGroupLayout>,
        Ref<PAL::WebGPU::Buffer>,
        Ref<PAL::WebGPU::CommandBuffer>,
        Ref<PAL::WebGPU::CommandEncoder>,
        Ref<PAL::WebGPU::ComputePassEncoder>,
        Ref<PAL::WebGPU::ComputePipeline>,
        Ref<PAL::WebGPU::Device>,
        Ref<PAL::WebGPU::ExternalTexture>,
        Ref<PAL::WebGPU::GPU>,
        Ref<PAL::WebGPU::PipelineLayout>,
        Ref<PAL::WebGPU::QuerySet>,
        Ref<PAL::WebGPU::Queue>,
        Ref<PAL::WebGPU::RenderBundleEncoder>,
        Ref<PAL::WebGPU::RenderBundle>,
        Ref<PAL::WebGPU::RenderPassEncoder>,
        Ref<PAL::WebGPU::RenderPipeline>,
        Ref<PAL::WebGPU::Sampler>,
        Ref<PAL::WebGPU::ShaderModule>,
        Ref<PAL::WebGPU::Texture>,
        Ref<PAL::WebGPU::TextureView>
    >;
    HashMap<WebGPUIdentifier, Object> m_objects;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
