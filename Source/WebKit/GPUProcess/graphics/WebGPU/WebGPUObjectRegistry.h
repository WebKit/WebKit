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
#include <functional>
#include <variant>
#include <wtf/HashMap.h>

namespace PAL::WebGPU {
class Adapter;
class BindGroup;
class BindGroupLayout;
class Buffer;
class CommandBuffer;
class CommandEncoder;
class ComputePassEncoder;
class ComputePipeline;
class Device;
class ExternalTexture;
class GPU;
class PipelineLayout;
class QuerySet;
class Queue;
class RenderBundleEncoder;
class RenderBundle;
class RenderPassEncoder;
class RenderPipeline;
class Sampler;
class ShaderModule;
class Texture;
class TextureView;
}

namespace WebKit::WebGPU {

class ObjectRegistry final : public WebGPU::ConvertFromBackingContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ObjectRegistry();
    ~ObjectRegistry();

    void addObject(WebGPUIdentifier, PAL::WebGPU::Adapter&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::BindGroup&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::BindGroupLayout&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::Buffer&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::CommandBuffer&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::CommandEncoder&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::ComputePassEncoder&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::ComputePipeline&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::Device&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::ExternalTexture&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::GPU&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::PipelineLayout&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::QuerySet&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::Queue&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::RenderBundleEncoder&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::RenderBundle&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::RenderPassEncoder&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::RenderPipeline&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::Sampler&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::ShaderModule&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::Texture&);
    void addObject(WebGPUIdentifier, PAL::WebGPU::TextureView&);

    void removeObject(WebGPUIdentifier);

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

private:
    using Object = std::variant<
        std::monostate,
        std::reference_wrapper<PAL::WebGPU::Adapter>,
        std::reference_wrapper<PAL::WebGPU::BindGroup>,
        std::reference_wrapper<PAL::WebGPU::BindGroupLayout>,
        std::reference_wrapper<PAL::WebGPU::Buffer>,
        std::reference_wrapper<PAL::WebGPU::CommandBuffer>,
        std::reference_wrapper<PAL::WebGPU::CommandEncoder>,
        std::reference_wrapper<PAL::WebGPU::ComputePassEncoder>,
        std::reference_wrapper<PAL::WebGPU::ComputePipeline>,
        std::reference_wrapper<PAL::WebGPU::Device>,
        std::reference_wrapper<PAL::WebGPU::ExternalTexture>,
        std::reference_wrapper<PAL::WebGPU::GPU>,
        std::reference_wrapper<PAL::WebGPU::PipelineLayout>,
        std::reference_wrapper<PAL::WebGPU::QuerySet>,
        std::reference_wrapper<PAL::WebGPU::Queue>,
        std::reference_wrapper<PAL::WebGPU::RenderBundleEncoder>,
        std::reference_wrapper<PAL::WebGPU::RenderBundle>,
        std::reference_wrapper<PAL::WebGPU::RenderPassEncoder>,
        std::reference_wrapper<PAL::WebGPU::RenderPipeline>,
        std::reference_wrapper<PAL::WebGPU::Sampler>,
        std::reference_wrapper<PAL::WebGPU::ShaderModule>,
        std::reference_wrapper<PAL::WebGPU::Texture>,
        std::reference_wrapper<PAL::WebGPU::TextureView>
    >;
    HashMap<WebGPUIdentifier, Object> m_objects;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
