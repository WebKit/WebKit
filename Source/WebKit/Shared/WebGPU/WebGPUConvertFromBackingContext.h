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

#include "WebGPUIdentifier.h"
#include <wtf/RefCounted.h>

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

} // namespace PAL::WebGPU

namespace WebKit::WebGPU {

class ConvertFromBackingContext : public RefCounted<ConvertFromBackingContext> {
public:
    virtual ~ConvertFromBackingContext() = default;

    virtual const PAL::WebGPU::Adapter* convertAdapterFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::BindGroup* convertBindGroupFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::BindGroupLayout* convertBindGroupLayoutFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::Buffer* convertBufferFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::CommandBuffer* convertCommandBufferFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::CommandEncoder* convertCommandEncoderFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::ComputePassEncoder* convertComputePassEncoderFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::ComputePipeline* convertComputePipelineFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::Device* convertDeviceFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::GPU* convertGPUFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::PipelineLayout* convertPipelineLayoutFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::QuerySet* convertQuerySetFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::Queue* convertQueueFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::RenderBundleEncoder* convertRenderBundleEncoderFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::RenderBundle* convertRenderBundleFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::RenderPassEncoder* convertRenderPassEncoderFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::RenderPipeline* convertRenderPipelineFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::Sampler* convertSamplerFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::ShaderModule* convertShaderModuleFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::Texture* convertTextureFromBacking(WebGPUIdentifier) = 0;
    virtual const PAL::WebGPU::TextureView* convertTextureViewFromBacking(WebGPUIdentifier) = 0;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
