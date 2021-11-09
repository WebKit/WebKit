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

WGPU_EXPORT void wgpuAdapterRelease(WGPUAdapter adapter);
WGPU_EXPORT void wgpuBindGroupRelease(WGPUBindGroup bindGroup);
WGPU_EXPORT void wgpuBindGroupLayoutRelease(WGPUBindGroupLayout bindGroupLayout);
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
WGPU_EXPORT void wgpuRenderBundleRelease(WGPURenderBundle renderBundle);
WGPU_EXPORT void wgpuRenderBundleEncoderRelease(WGPURenderBundleEncoder renderBundleEncoder);
WGPU_EXPORT void wgpuRenderPassEncoderRelease(WGPURenderPassEncoder renderPassEncoder);
WGPU_EXPORT void wgpuRenderPipelineRelease(WGPURenderPipeline renderPipeline);
WGPU_EXPORT void wgpuSamplerRelease(WGPUSampler sampler);
WGPU_EXPORT void wgpuShaderModuleRelease(WGPUShaderModule shaderModule);
WGPU_EXPORT void wgpuSurfaceRelease(WGPUSurface surface);
WGPU_EXPORT void wgpuSwapChainRelease(WGPUSwapChain swapChain);
WGPU_EXPORT void wgpuTextureRelease(WGPUTexture texture);
WGPU_EXPORT void wgpuTextureViewRelease(WGPUTextureView textureView);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
