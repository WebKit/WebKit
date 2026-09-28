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

// The WebGPU_Private module of WebGPU.framework includes every private header, so this header
// must also compile as C and Objective-C.
#ifdef __cplusplus

#import <WebGPU/WebGPU.h>
#import <WebGPU/WebGPUCpp.h>
#import <WebGPU/WebGPUExt.h>

// Transition bridge between the handles of the WebGPU C API and the objects of the WebGPU C++ API.
// It lets code that has been converted to the C++ API exchange objects with code that still uses
// the C API while WebCore and WebKit move to the C++ API. Remove it when nothing uses the C API
// handles anymore.

namespace WebGPU {

WGPU_EXPORT Adapter& fromAPI(WGPUAdapter);
WGPU_EXPORT BindGroup& fromAPI(WGPUBindGroup);
WGPU_EXPORT BindGroupLayout& fromAPI(WGPUBindGroupLayout);
WGPU_EXPORT Buffer& fromAPI(WGPUBuffer);
WGPU_EXPORT CommandBuffer& fromAPI(WGPUCommandBuffer);
WGPU_EXPORT CommandEncoder& fromAPI(WGPUCommandEncoder);
WGPU_EXPORT ComputePassEncoder& fromAPI(WGPUComputePassEncoder);
WGPU_EXPORT ComputePipeline& fromAPI(WGPUComputePipeline);
WGPU_EXPORT Device& fromAPI(WGPUDevice);
WGPU_EXPORT ExternalTexture& fromAPI(WGPUExternalTexture);
WGPU_EXPORT Instance& fromAPI(WGPUInstance);
WGPU_EXPORT PipelineLayout& fromAPI(WGPUPipelineLayout);
WGPU_EXPORT PresentationContext& fromAPI(WGPUSurface);
WGPU_EXPORT QuerySet& fromAPI(WGPUQuerySet);
WGPU_EXPORT Queue& fromAPI(WGPUQueue);
WGPU_EXPORT RenderBundle& fromAPI(WGPURenderBundle);
WGPU_EXPORT RenderBundleEncoder& fromAPI(WGPURenderBundleEncoder);
WGPU_EXPORT RenderPassEncoder& fromAPI(WGPURenderPassEncoder);
WGPU_EXPORT RenderPipeline& fromAPI(WGPURenderPipeline);
WGPU_EXPORT Sampler& fromAPI(WGPUSampler);
WGPU_EXPORT ShaderModule& fromAPI(WGPUShaderModule);
WGPU_EXPORT Texture& fromAPI(WGPUTexture);
WGPU_EXPORT TextureView& fromAPI(WGPUTextureView);
WGPU_EXPORT XRBinding& fromAPI(WGPUXRBinding);
WGPU_EXPORT XRProjectionLayer& fromAPI(WGPUXRProjectionLayer);
WGPU_EXPORT XRSubImage& fromAPI(WGPUXRSubImage);
WGPU_EXPORT XRView& fromAPI(WGPUXRView);
WGPU_EXPORT PresentationContext& fromAPI(WGPUSwapChain);

WGPU_EXPORT WGPUAdapter toAPI(Adapter&);
WGPU_EXPORT WGPUBindGroup toAPI(BindGroup&);
WGPU_EXPORT WGPUBindGroupLayout toAPI(BindGroupLayout&);
WGPU_EXPORT WGPUBuffer toAPI(Buffer&);
WGPU_EXPORT WGPUCommandBuffer toAPI(CommandBuffer&);
WGPU_EXPORT WGPUCommandEncoder toAPI(CommandEncoder&);
WGPU_EXPORT WGPUComputePassEncoder toAPI(ComputePassEncoder&);
WGPU_EXPORT WGPUComputePipeline toAPI(ComputePipeline&);
WGPU_EXPORT WGPUDevice toAPI(Device&);
WGPU_EXPORT WGPUExternalTexture toAPI(ExternalTexture&);
WGPU_EXPORT WGPUInstance toAPI(Instance&);
WGPU_EXPORT WGPUPipelineLayout toAPI(PipelineLayout&);
WGPU_EXPORT WGPUSurface toAPI(PresentationContext&);
WGPU_EXPORT WGPUQuerySet toAPI(QuerySet&);
WGPU_EXPORT WGPUQueue toAPI(Queue&);
WGPU_EXPORT WGPURenderBundle toAPI(RenderBundle&);
WGPU_EXPORT WGPURenderBundleEncoder toAPI(RenderBundleEncoder&);
WGPU_EXPORT WGPURenderPassEncoder toAPI(RenderPassEncoder&);
WGPU_EXPORT WGPURenderPipeline toAPI(RenderPipeline&);
WGPU_EXPORT WGPUSampler toAPI(Sampler&);
WGPU_EXPORT WGPUShaderModule toAPI(ShaderModule&);
WGPU_EXPORT WGPUTexture toAPI(Texture&);
WGPU_EXPORT WGPUTextureView toAPI(TextureView&);
WGPU_EXPORT WGPUXRBinding toAPI(XRBinding&);
WGPU_EXPORT WGPUXRProjectionLayer toAPI(XRProjectionLayer&);
WGPU_EXPORT WGPUXRSubImage toAPI(XRSubImage&);
WGPU_EXPORT WGPUXRView toAPI(XRView&);
WGPU_EXPORT WGPUSwapChain toAPISwapChain(PresentationContext&);

} // namespace WebGPU

#endif // __cplusplus
