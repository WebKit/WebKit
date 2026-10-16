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

#pragma once

#import <WebGPU/WebGPU.h>
#import <WebGPU/WebGPUCpp.h>
#import <WebGPU/WebGPUExt.h>

// Bridge between the handles of the WebGPU C API and the objects of the WebGPU C++ API, for the
// conversions of the C API shim, which cannot include the headers of the Metal objects. Remove it
// when the C API handles are the C++ API objects.

namespace WebGPU {

Adapter& fromAPI(WGPUAdapter);
BindGroup& fromAPI(WGPUBindGroup);
BindGroupLayout& fromAPI(WGPUBindGroupLayout);
Buffer& fromAPI(WGPUBuffer);
CommandBuffer& fromAPI(WGPUCommandBuffer);
CommandEncoder& fromAPI(WGPUCommandEncoder);
ComputePassEncoder& fromAPI(WGPUComputePassEncoder);
ComputePipeline& fromAPI(WGPUComputePipeline);
Device& fromAPI(WGPUDevice);
ExternalTexture& fromAPI(WGPUExternalTexture);
Instance& fromAPI(WGPUInstance);
PipelineLayout& fromAPI(WGPUPipelineLayout);
PresentationContext& fromAPI(WGPUSurface);
QuerySet& fromAPI(WGPUQuerySet);
Queue& fromAPI(WGPUQueue);
RenderBundle& fromAPI(WGPURenderBundle);
RenderBundleEncoder& fromAPI(WGPURenderBundleEncoder);
RenderPassEncoder& fromAPI(WGPURenderPassEncoder);
RenderPipeline& fromAPI(WGPURenderPipeline);
Sampler& fromAPI(WGPUSampler);
ShaderModule& fromAPI(WGPUShaderModule);
Texture& fromAPI(WGPUTexture);
TextureView& fromAPI(WGPUTextureView);
XRBinding& fromAPI(WGPUXRBinding);
XRProjectionLayer& fromAPI(WGPUXRProjectionLayer);
XRSubImage& fromAPI(WGPUXRSubImage);
XRView& fromAPI(WGPUXRView);
PresentationContext& fromAPI(WGPUSwapChain);

WGPUAdapter toAPI(Adapter&);
WGPUBindGroup toAPI(BindGroup&);
WGPUBindGroupLayout toAPI(BindGroupLayout&);
WGPUBuffer toAPI(Buffer&);
WGPUCommandBuffer toAPI(CommandBuffer&);
WGPUCommandEncoder toAPI(CommandEncoder&);
WGPUComputePassEncoder toAPI(ComputePassEncoder&);
WGPUComputePipeline toAPI(ComputePipeline&);
WGPUDevice toAPI(Device&);
WGPUExternalTexture toAPI(ExternalTexture&);
WGPUInstance toAPI(Instance&);
WGPUPipelineLayout toAPI(PipelineLayout&);
WGPUSurface toAPI(PresentationContext&);
WGPUQuerySet toAPI(QuerySet&);
WGPUQueue toAPI(Queue&);
WGPURenderBundle toAPI(RenderBundle&);
WGPURenderBundleEncoder toAPI(RenderBundleEncoder&);
WGPURenderPassEncoder toAPI(RenderPassEncoder&);
WGPURenderPipeline toAPI(RenderPipeline&);
WGPUSampler toAPI(Sampler&);
WGPUShaderModule toAPI(ShaderModule&);
WGPUTexture toAPI(Texture&);
WGPUTextureView toAPI(TextureView&);
WGPUXRBinding toAPI(XRBinding&);
WGPUXRProjectionLayer toAPI(XRProjectionLayer&);
WGPUXRSubImage toAPI(XRSubImage&);
WGPUXRView toAPI(XRView&);
WGPUSwapChain toAPISwapChain(PresentationContext&);

} // namespace WebGPU
