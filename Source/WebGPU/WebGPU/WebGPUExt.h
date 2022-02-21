/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#ifndef WEBGPUEXT_H_
#define WEBGPUEXT_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum WGPUSTypeExtended {
    WGPUSTypeExtended_ShaderModuleDescriptorHints = 0x348970F3, // Random
    WGPUSTypeExtended_TextureDescriptorViewFormats = 0x1D5BC57, // Random
    WGPUSTypeExtended_Force32 = 0x7FFFFFFF
} WGPUSTypeExtended;

typedef struct WGPUShaderModuleCompilationHint {
    WGPUPipelineLayout layout;
} WGPUShaderModuleCompilationHint;

typedef struct WGPUShaderModuleCompilationHintEntry {
    WGPUChainedStruct const * nextInChain;
    char const * key;
    WGPUShaderModuleCompilationHint hint;
} WGPUShaderModuleCompilationHintEntry;

typedef struct WGPUShaderModuleDescriptorHints {
    WGPUChainedStruct chain;
    uint32_t hintsCount;
    WGPUShaderModuleCompilationHintEntry const * hints;
} WGPUShaderModuleDescriptorHints;

typedef struct WGPUTextureDescriptorViewFormats {
    WGPUChainedStruct chain;
    uint32_t viewFormatsCount;
    WGPUTextureFormat const * viewFormats;
} WGPUTextureDescriptorViewFormats;

#if !defined(WGPU_SKIP_PROCS)

typedef void (*WGPUProcAdapterRelease)(WGPUAdapter adapter);
typedef void (*WGPUProcBindGroupLayoutRelease)(WGPUBindGroupLayout bindGroupLayout);
typedef void (*WGPUProcBindGroupRelease)(WGPUBindGroup bindGroup);
typedef void (*WGPUProcBufferRelease)(WGPUBuffer buffer);
typedef void (*WGPUProcCommandBufferRelease)(WGPUCommandBuffer commandBuffer);
typedef void (*WGPUProcCommandEncoderRelease)(WGPUCommandEncoder commandEncoder);
typedef void (*WGPUProcComputePassEncoderRelease)(WGPUComputePassEncoder computePassEncoder);
typedef void (*WGPUProcComputePipelineRelease)(WGPUComputePipeline computePipeline);
typedef void (*WGPUProcDeviceRelease)(WGPUDevice device);
typedef void (*WGPUProcInstanceRelease)(WGPUInstance instance);
typedef void (*WGPUProcPipelineLayoutRelease)(WGPUPipelineLayout pipelineLayout);
typedef void (*WGPUProcQuerySetRelease)(WGPUQuerySet querySet);
typedef void (*WGPUProcQueueRelease)(WGPUQueue queue);
typedef void (*WGPUProcRenderBundleEncoderRelease)(WGPURenderBundleEncoder renderBundleEncoder);
typedef void (*WGPUProcRenderBundleRelease)(WGPURenderBundle renderBundle);
typedef void (*WGPUProcRenderPassEncoderRelease)(WGPURenderPassEncoder renderPassEncoder);
typedef void (*WGPUProcRenderPipelineRelease)(WGPURenderPipeline renderPipeline);
typedef void (*WGPUProcSamplerRelease)(WGPUSampler sampler);
typedef void (*WGPUProcShaderModuleRelease)(WGPUShaderModule shaderModule);
typedef void (*WGPUProcSurfaceRelease)(WGPUSurface surface);
typedef void (*WGPUProcSwapChainRelease)(WGPUSwapChain swapChain);
typedef void (*WGPUProcTextureRelease)(WGPUTexture texture);
typedef void (*WGPUProcTextureViewRelease)(WGPUTextureView textureView);

typedef void (*WGPUProcBindGroupLayoutSetLabel)(WGPUBindGroupLayout bindGroupLayout, char const * label);
typedef void (*WGPUProcBindGroupSetLabel)(WGPUBindGroup bindGroup, char const * label);
typedef void (*WGPUProcBufferSetLabel)(WGPUBuffer buffer, char const * label);
typedef void (*WGPUProcCommandBufferSetLabel)(WGPUCommandBuffer commandBuffer, char const * label);
typedef void (*WGPUProcCommandEncoderSetLabel)(WGPUCommandEncoder commandEncoder, char const * label);
typedef void (*WGPUProcComputePassEncoderSetLabel)(WGPUComputePassEncoder computePassEncoder, char const * label);
typedef void (*WGPUProcDeviceSetLabel)(WGPUDevice queue, char const * label);
typedef void (*WGPUProcPipelineLayoutSetLabel)(WGPUPipelineLayout pipelineLayout, char const * label);
typedef void (*WGPUProcQuerySetSetLabel)(WGPUQuerySet querySet, char const * label);
typedef void (*WGPUProcQueueSetLabel)(WGPUQueue queue, char const * label);
typedef void (*WGPUProcRenderBundleEncoderSetLabel)(WGPURenderBundleEncoder renderBundleEncoder, char const * label);
typedef void (*WGPUProcRenderBundleSetLabel)(WGPURenderBundle renderBundle, char const * label);
typedef void (*WGPUProcRenderPassEncoderSetLabel)(WGPURenderPassEncoder renderBundleEncoder, char const * label);
typedef void (*WGPUProcSamplerSetLabel)(WGPUSampler sampler, char const * label);
typedef void (*WGPUProcTextureSetLabel)(WGPUTexture sampler, char const * label);
typedef void (*WGPUProcTextureViewSetLabel)(WGPUTextureView sampler, char const * label);

#endif  // !defined(WGPU_SKIP_PROCS)

#if !defined(WGPU_SKIP_DECLARATIONS)

WGPU_EXPORT void wgpuAdapterRelease(WGPUAdapter adapter);
WGPU_EXPORT void wgpuBindGroupLayoutRelease(WGPUBindGroupLayout bindGroupLayout);
WGPU_EXPORT void wgpuBindGroupRelease(WGPUBindGroup bindGroup);
WGPU_EXPORT void wgpuBufferRelease(WGPUBuffer buffer);
WGPU_EXPORT void wgpuCommandBufferRelease(WGPUCommandBuffer commandBuffer);
WGPU_EXPORT void wgpuCommandEncoderRelease(WGPUCommandEncoder commandEncoder);
WGPU_EXPORT void wgpuComputePassEncoderRelease(WGPUComputePassEncoder computePassEncoder);
WGPU_EXPORT void wgpuComputePipelineRelease(WGPUComputePipeline computePipeline);
WGPU_EXPORT void wgpuDeviceRelease(WGPUDevice device);
WGPU_EXPORT void wgpuInstanceRelease(WGPUInstance instance);
WGPU_EXPORT void wgpuPipelineLayoutRelease(WGPUPipelineLayout pipelineLayout);
WGPU_EXPORT void wgpuQuerySetRelease(WGPUQuerySet querySet);
WGPU_EXPORT void wgpuQueueRelease(WGPUQueue queue);
WGPU_EXPORT void wgpuRenderBundleEncoderRelease(WGPURenderBundleEncoder renderBundleEncoder);
WGPU_EXPORT void wgpuRenderBundleRelease(WGPURenderBundle renderBundle);
WGPU_EXPORT void wgpuRenderPassEncoderRelease(WGPURenderPassEncoder renderPassEncoder);
WGPU_EXPORT void wgpuRenderPipelineRelease(WGPURenderPipeline renderPipeline);
WGPU_EXPORT void wgpuSamplerRelease(WGPUSampler sampler);
WGPU_EXPORT void wgpuShaderModuleRelease(WGPUShaderModule shaderModule);
WGPU_EXPORT void wgpuSurfaceRelease(WGPUSurface surface);
WGPU_EXPORT void wgpuSwapChainRelease(WGPUSwapChain swapChain);
WGPU_EXPORT void wgpuTextureRelease(WGPUTexture texture);
WGPU_EXPORT void wgpuTextureViewRelease(WGPUTextureView textureView);

WGPU_EXPORT void wgpuBindGroupLayoutSetLabel(WGPUBindGroupLayout bindGroupLayout, char const * label);
WGPU_EXPORT void wgpuBindGroupSetLabel(WGPUBindGroup bindGroup, char const * label);
WGPU_EXPORT void wgpuBufferSetLabel(WGPUBuffer buffer, char const * label);
WGPU_EXPORT void wgpuCommandBufferSetLabel(WGPUCommandBuffer commandBuffer, char const * label);
WGPU_EXPORT void wgpuCommandEncoderSetLabel(WGPUCommandEncoder commandEncoder, char const * label);
WGPU_EXPORT void wgpuComputePassEncoderSetLabel(WGPUComputePassEncoder computePassEncoder, char const * label);
WGPU_EXPORT void wgpuDeviceSetLabel(WGPUDevice queue, char const * label);
WGPU_EXPORT void wgpuPipelineLayoutSetLabel(WGPUPipelineLayout pipelineLayout, char const * label);
WGPU_EXPORT void wgpuQuerySetSetLabel(WGPUQuerySet querySet, char const * label);
WGPU_EXPORT void wgpuQueueSetLabel(WGPUQueue queue, char const * label);
WGPU_EXPORT void wgpuRenderBundleEncoderSetLabel(WGPURenderBundleEncoder renderBundleEncoder, char const * label);
WGPU_EXPORT void wgpuRenderBundleSetLabel(WGPURenderBundle renderBundle, char const * label);
WGPU_EXPORT void wgpuRenderPassEncoderSetLabel(WGPURenderPassEncoder renderBundleEncoder, char const * label);
WGPU_EXPORT void wgpuSamplerSetLabel(WGPUSampler sampler, char const * label);
WGPU_EXPORT void wgpuTextureSetLabel(WGPUTexture sampler, char const * label);
WGPU_EXPORT void wgpuTextureViewSetLabel(WGPUTextureView sampler, char const * label);

#endif  // !defined(WGPU_SKIP_DECLARATIONS)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // WEBGPUEXT_H_
