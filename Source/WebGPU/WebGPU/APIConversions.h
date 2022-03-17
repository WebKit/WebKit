/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#import "Adapter.h"
#import "BindGroup.h"
#import "BindGroupLayout.h"
#import "Buffer.h"
#import "CommandBuffer.h"
#import "CommandEncoder.h"
#import "ComputePassEncoder.h"
#import "ComputePipeline.h"
#import "Device.h"
#import "Instance.h"
#import "PipelineLayout.h"
#import "QuerySet.h"
#import "Queue.h"
#import "RenderBundle.h"
#import "RenderBundleEncoder.h"
#import "RenderPassEncoder.h"
#import "RenderPipeline.h"
#import "Sampler.h"
#import "ShaderModule.h"
#import "Surface.h"
#import "SwapChain.h"
#import "Texture.h"
#import "TextureView.h"
#import <wtf/text/WTFString.h>

namespace WebGPU {

inline Adapter& fromAPI(WGPUAdapter adapter)
{
    return adapter->adapter;
}

inline BindGroup& fromAPI(WGPUBindGroup bindGroup)
{
    return bindGroup->bindGroup;
}

inline BindGroupLayout& fromAPI(WGPUBindGroupLayout bindGroupLayout)
{
    return bindGroupLayout->bindGroupLayout;
}

inline Buffer& fromAPI(WGPUBuffer buffer)
{
    return buffer->buffer;
}

inline CommandBuffer& fromAPI(WGPUCommandBuffer commandBuffer)
{
    return commandBuffer->commandBuffer;
}

inline CommandEncoder& fromAPI(WGPUCommandEncoder commandEncoder)
{
    return commandEncoder->commandEncoder;
}

inline ComputePassEncoder& fromAPI(WGPUComputePassEncoder computePassEncoder)
{
    return computePassEncoder->computePassEncoder;
}

inline ComputePipeline& fromAPI(WGPUComputePipeline computePipeline)
{
    return computePipeline->computePipeline;
}

inline Device& fromAPI(WGPUDevice device)
{
    return device->device;
}

inline Instance& fromAPI(WGPUInstance instance)
{
    return instance->instance;
}

inline PipelineLayout& fromAPI(WGPUPipelineLayout pipelineLayout)
{
    return pipelineLayout->pipelineLayout;
}

inline QuerySet& fromAPI(WGPUQuerySet querySet)
{
    return querySet->querySet;
}

inline Queue& fromAPI(WGPUQueue queue)
{
    return queue->queue;
}

inline RenderBundle& fromAPI(WGPURenderBundle renderBundle)
{
    return renderBundle->renderBundle;
}

inline RenderBundleEncoder& fromAPI(WGPURenderBundleEncoder renderBundleEncoder)
{
    return renderBundleEncoder->renderBundleEncoder;
}

inline RenderPassEncoder& fromAPI(WGPURenderPassEncoder renderPassEncoder)
{
    return renderPassEncoder->renderPassEncoder;
}

inline RenderPipeline& fromAPI(WGPURenderPipeline renderPipeline)
{
    return renderPipeline->renderPipeline;
}

inline Sampler& fromAPI(WGPUSampler sampler)
{
    return sampler->sampler;
}

inline ShaderModule& fromAPI(WGPUShaderModule shaderModule)
{
    return shaderModule->shaderModule;
}

inline Surface& fromAPI(WGPUSurface surface)
{
    return surface->surface;
}

inline SwapChain& fromAPI(WGPUSwapChain swapChain)
{
    return swapChain->swapChain;
}

inline Texture& fromAPI(WGPUTexture texture)
{
    return texture->texture;
}

inline TextureView& fromAPI(WGPUTextureView textureView)
{
    return textureView->textureView;
}

inline String fromAPI(const char* string)
{
    return String(string);
}

} // namespace WebGPU
