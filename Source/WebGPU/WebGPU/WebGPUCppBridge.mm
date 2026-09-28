/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebGPUCppBridge.h"

#import "APIConversions.h"

namespace WebGPU {

Adapter& fromAPI(WGPUAdapter object)
{
    return Metal::fromAPI(object);
}

BindGroup& fromAPI(WGPUBindGroup object)
{
    return Metal::fromAPI(object);
}

BindGroupLayout& fromAPI(WGPUBindGroupLayout object)
{
    return Metal::fromAPI(object);
}

Buffer& fromAPI(WGPUBuffer object)
{
    return Metal::fromAPI(object);
}

CommandBuffer& fromAPI(WGPUCommandBuffer object)
{
    return Metal::fromAPI(object);
}

CommandEncoder& fromAPI(WGPUCommandEncoder object)
{
    return Metal::fromAPI(object);
}

ComputePassEncoder& fromAPI(WGPUComputePassEncoder object)
{
    return Metal::fromAPI(object);
}

ComputePipeline& fromAPI(WGPUComputePipeline object)
{
    return Metal::fromAPI(object);
}

Device& fromAPI(WGPUDevice object)
{
    return Metal::fromAPI(object);
}

ExternalTexture& fromAPI(WGPUExternalTexture object)
{
    return Metal::fromAPI(object);
}

Instance& fromAPI(WGPUInstance object)
{
    return Metal::fromAPI(object);
}

PipelineLayout& fromAPI(WGPUPipelineLayout object)
{
    return Metal::fromAPI(object);
}

PresentationContext& fromAPI(WGPUSurface object)
{
    return Metal::fromAPI(object);
}

QuerySet& fromAPI(WGPUQuerySet object)
{
    return Metal::fromAPI(object);
}

Queue& fromAPI(WGPUQueue object)
{
    return Metal::fromAPI(object);
}

RenderBundle& fromAPI(WGPURenderBundle object)
{
    return Metal::fromAPI(object);
}

RenderBundleEncoder& fromAPI(WGPURenderBundleEncoder object)
{
    return Metal::fromAPI(object);
}

RenderPassEncoder& fromAPI(WGPURenderPassEncoder object)
{
    return Metal::fromAPI(object);
}

RenderPipeline& fromAPI(WGPURenderPipeline object)
{
    return Metal::fromAPI(object);
}

Sampler& fromAPI(WGPUSampler object)
{
    return Metal::fromAPI(object);
}

ShaderModule& fromAPI(WGPUShaderModule object)
{
    return Metal::fromAPI(object);
}

Texture& fromAPI(WGPUTexture object)
{
    return Metal::fromAPI(object);
}

TextureView& fromAPI(WGPUTextureView object)
{
    return Metal::fromAPI(object);
}

XRBinding& fromAPI(WGPUXRBinding object)
{
    return Metal::fromAPI(object);
}

XRProjectionLayer& fromAPI(WGPUXRProjectionLayer object)
{
    return Metal::fromAPI(object);
}

XRSubImage& fromAPI(WGPUXRSubImage object)
{
    return Metal::fromAPI(object);
}

XRView& fromAPI(WGPUXRView object)
{
    return Metal::fromAPI(object);
}

PresentationContext& fromAPI(WGPUSwapChain swapChain)
{
    return Metal::fromAPI(swapChain);
}

WGPUAdapter toAPI(Adapter& object)
{
    return Metal::toAPI(object);
}

WGPUBindGroup toAPI(BindGroup& object)
{
    return Metal::toAPI(object);
}

WGPUBindGroupLayout toAPI(BindGroupLayout& object)
{
    return Metal::toAPI(object);
}

WGPUBuffer toAPI(Buffer& object)
{
    return Metal::toAPI(object);
}

WGPUCommandBuffer toAPI(CommandBuffer& object)
{
    return Metal::toAPI(object);
}

WGPUCommandEncoder toAPI(CommandEncoder& object)
{
    return Metal::toAPI(object);
}

WGPUComputePassEncoder toAPI(ComputePassEncoder& object)
{
    return Metal::toAPI(object);
}

WGPUComputePipeline toAPI(ComputePipeline& object)
{
    return Metal::toAPI(object);
}

WGPUDevice toAPI(Device& object)
{
    return Metal::toAPI(object);
}

WGPUExternalTexture toAPI(ExternalTexture& object)
{
    return Metal::toAPI(object);
}

WGPUInstance toAPI(Instance& object)
{
    return Metal::toAPI(object);
}

WGPUPipelineLayout toAPI(PipelineLayout& object)
{
    return Metal::toAPI(object);
}

WGPUSurface toAPI(PresentationContext& object)
{
    return Metal::toAPI(object);
}

WGPUQuerySet toAPI(QuerySet& object)
{
    return Metal::toAPI(object);
}

WGPUQueue toAPI(Queue& object)
{
    return Metal::toAPI(object);
}

WGPURenderBundle toAPI(RenderBundle& object)
{
    return Metal::toAPI(object);
}

WGPURenderBundleEncoder toAPI(RenderBundleEncoder& object)
{
    return Metal::toAPI(object);
}

WGPURenderPassEncoder toAPI(RenderPassEncoder& object)
{
    return Metal::toAPI(object);
}

WGPURenderPipeline toAPI(RenderPipeline& object)
{
    return Metal::toAPI(object);
}

WGPUSampler toAPI(Sampler& object)
{
    return Metal::toAPI(object);
}

WGPUShaderModule toAPI(ShaderModule& object)
{
    return Metal::toAPI(object);
}

WGPUTexture toAPI(Texture& object)
{
    return Metal::toAPI(object);
}

WGPUTextureView toAPI(TextureView& object)
{
    return Metal::toAPI(object);
}

WGPUXRBinding toAPI(XRBinding& object)
{
    return Metal::toAPI(object);
}

WGPUXRProjectionLayer toAPI(XRProjectionLayer& object)
{
    return Metal::toAPI(object);
}

WGPUXRSubImage toAPI(XRSubImage& object)
{
    return Metal::toAPI(object);
}

WGPUXRView toAPI(XRView& object)
{
    return Metal::toAPI(object);
}

WGPUSwapChain toAPISwapChain(PresentationContext& presentationContext)
{
    return Metal::toAPISwapChain(presentationContext);
}

} // namespace WebGPU
