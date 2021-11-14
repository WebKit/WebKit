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

typedef enum WGPUSTypeExtended {
    WGPUSType_DeviceDescriptorLabel = 0x242A99E0, // Random
} WGPUSTypeExtended;

typedef enum WGPUTextureFormatExtended {
    WGPUTextureFormat_ETC2RGB8Unorm = 0x28C25C67, // Random
    WGPUTextureFormat_ETC2RGB8unormSrgb = 0x566AAFEE, // Random
    WGPUTextureFormat_ETC2RGB8a1Unorm = 0x2F794348, // Random
    WGPUTextureFormat_ETC2RGB8a1unormSrgb = 0x5EF38ABA, // Random
    WGPUTextureFormat_ETC2RGBA8Unorm = 0x5A4E6480, // Random
    WGPUTextureFormat_ETC2RGBA8UnormSrgb = 0x780968EB, // Random
    WGPUTextureFormat_EACR11Unorm = 0x69863090, // Random
    WGPUTextureFormat_EACR11Snorm = 0x25E5428A, // Random
    WGPUTextureFormat_EACRG11Unorm = 0x1B0969B3, // Random
    WGPUTextureFormat_EACRG11Snorm = 0x16F4BB6D, // Random

    WGPUTextureFormat_ASTC4x4Unorm = 0x39F60F6E, // Random
    WGPUTextureFormat_ASTC4x4UnormSrgb = 0x24472700, // Random
    WGPUTextureFormat_ASTC5x4Unorm = 0x67D32B0E, // Random
    WGPUTextureFormat_ASTC5x4UnormSrgb = 0x313DA1BE, // Random
    WGPUTextureFormat_ASTC5x5Unorm = 0x15AB8DFD, // Random
    WGPUTextureFormat_ASTC5x5UnormSrgb = 0x7B00EB57, // Random
    WGPUTextureFormat_ASTC6x5Unorm = 0x653E80C3, // Random
    WGPUTextureFormat_ASTC6x5UnormSrgb = 0x50AE869A, // Random
    WGPUTextureFormat_ASTC6x6Unorm = 0x579AF598, // Random
    WGPUTextureFormat_ASTC6x6UnormSrgb = 0x73B69732, // Random
    WGPUTextureFormat_ASTC8x5Unorm = 0x06F70308, // Random
    WGPUTextureFormat_ASTC8x5UnormSrgb = 0x420EA946, // Random
    WGPUTextureFormat_ASTC8x6Unorm = 0x61086AC8, // Random
    WGPUTextureFormat_ASTC8x6UnormSrgb = 0x0E17D39A, // Random
    WGPUTextureFormat_ASTC8x8Unorm = 0x569BF2E8, // Random
    WGPUTextureFormat_ASTC8x8UnormSrgb = 0x572A4849, // Random
    WGPUTextureFormat_ASTC10x5Unorm = 0x63ABE432, // Random
    WGPUTextureFormat_ASTC10x5UnormSrgb = 0x3BC3AA4C, // Random
    WGPUTextureFormat_ASTC10x6Unorm = 0x6FE19499, // Random
    WGPUTextureFormat_ASTC10x6UnormSrgb = 0x7FF0B5C0, // Random
    WGPUTextureFormat_ASTC10x8Unorm = 0x2015F4C3, // Random
    WGPUTextureFormat_ASTC10x8UnormSrgb = 0x7D51DC6F, // Random
    WGPUTextureFormat_ASTC10x10Unorm = 0x67F10173, // Random
    WGPUTextureFormat_ASTC10x10UnormSrgb = 0x35D74D21, // Random
    WGPUTextureFormat_ASTC12x10Unorm = 0x09DAD0A7, // Random
    WGPUTextureFormat_ASTC12x10UnormSrgb = 0x3560C93A, // Random
    WGPUTextureFormat_ASTC12x12Unorm = 0x26CC3050, // Random
    WGPUTextureFormat_ASTC12x12UnormSrgb = 0x3EF578A0, // Random

    WGPUTextureFormat_Depth32FloatStencil8 = 0x53DC2307, // Random
} WGPUTextureFormatExtended;

typedef enum WGPUFeatureNameExtended {
    WGPUFeatureName_DepthClipControl = 0x55ABC13D, // Random
    WGPUFeatureName_IndirectFirstInstance = 0x2A7084F5, // Random
    WGPUFeatureName_TextureCompressionETC2 = 0x7BF66F69, // Random
    WGPUFeatureName_TextureCompressionASTC = 0x26173399, // Random
} WGPUFeatureNameExtended;

typedef enum WGPUPowerPreferenceExtended {
    WGPUPowerPreference_NoPreference = 0x4748336F,
} WGPUPowerPreferenceExtended;

typedef struct WGPUDeviceDescriptorLabel {
    WGPUChainedStruct header;
    WGPUChainedStruct const * nextInChain;
    char const * label;
} WGPUDeviceDescriptorLabel;

#ifdef __cplusplus
extern "C" {
#endif

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

typedef WGPUFeatureName (*WGPUProcAdapterGetFeatureAtIndex)(WGPUAdapter adapter, size_t index);

typedef void (*WGPUProcCommandEncoderFillBuffer)(WGPUCommandEncoder commandEncoder, WGPUBuffer destination, uint64_t destinationOffset, uint64_t size);

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

WGPU_EXPORT WGPUFeatureName wgpuAdapterGetFeatureAtIndex(WGPUAdapter adapter, size_t index);

WGPU_EXPORT void wgpuCommandEncoderFillBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer destination, uint64_t destinationOffset, uint64_t size);

#endif  // !defined(WGPU_SKIP_DECLARATIONS)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // WEBGPUEXT_H_
