/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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

#include <IOSurface/IOSurfaceRef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (^WGPUBufferMapBlockCallback)(WGPUBufferMapAsyncStatus status);
typedef void (^WGPUCompilationInfoBlockCallback)(WGPUCompilationInfoRequestStatus status, WGPUCompilationInfo const * compilationInfo);
typedef void (^WGPUCreateComputePipelineAsyncBlockCallback)(WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline pipeline, char const * message);
typedef void (^WGPUCreateRenderPipelineAsyncBlockCallback)(WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline pipeline, char const * message);
typedef void (^WGPUDeviceLostBlockCallback)(WGPUDeviceLostReason reason, char const * message);
typedef void (^WGPUErrorBlockCallback)(WGPUErrorType type, char const * message);
typedef void (^WGPUQueueWorkDoneBlockCallback)(WGPUQueueWorkDoneStatus status);
typedef void (^WGPURequestAdapterBlockCallback)(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const * message);
typedef void (^WGPURequestDeviceBlockCallback)(WGPURequestDeviceStatus status, WGPUDevice device, char const * message);
typedef void (^WGPURequestInvalidDeviceBlockCallback)(WGPUDevice device);
typedef void (^WGPUWorkItem)(void);
typedef void (^WGPUScheduleWorkBlock)(WGPUWorkItem workItem);

typedef enum WGPUSTypeExtended {
    WGPUSTypeExtended_TextureDescriptorViewFormats = 0x1D5BC57, // Random
    WGPUSTypeExtended_InstanceCocoaDescriptor = 0x151BBC00, // Random
    WGPUSTypeExtended_SurfaceDescriptorCocoaSurfaceBacking = 0x017E9710, // Random
    WGPUSTypeExtended_TextureDescriptorCocoaSurfaceBacking = 0xCCE4ED61, // Random
    WGPUSTypeExtended_Force32 = 0x7FFFFFFF
} WGPUSTypeExtended;

typedef struct WGPUInstanceCocoaDescriptor {
    WGPUChainedStruct chain;
    // The API contract is: callers must call WebGPU's functions in a non-racey way with respect
    // to each other. This scheduleWorkBlock will execute on a background thread, and it must
    // schedule the block it's passed to be run in a non-racey way with regards to all the other
    // WebGPU calls. If calls to scheduleWorkBlock are ordered (e.g. multiple calls on the same
    // thread), then the work that is scheduled must also be ordered in the same order.
    // It's fine to pass NULL here, but if you do, you must periodically call
    // wgpuInstanceProcessEvents() to synchronously run the queued callbacks.
    __unsafe_unretained WGPUScheduleWorkBlock scheduleWorkBlock;
} WGPUInstanceCocoaDescriptor;

typedef struct WGPUTextureDescriptorViewFormats {
    WGPUChainedStruct chain;
    uint32_t viewFormatsCount;
    WGPUTextureFormat const * viewFormats;
} WGPUTextureDescriptorViewFormats;

typedef void (^WGPURenderBuffersWereRecreatedBlockCallback)(CFArrayRef ioSurfaces);
typedef void (^WGPUCompositorIntegrationRegisterBlockCallback)(WGPURenderBuffersWereRecreatedBlockCallback renderBuffersWereRecreated);
typedef struct WGPUSurfaceDescriptorCocoaCustomSurface {
    WGPUChainedStruct chain;
    WGPUCompositorIntegrationRegisterBlockCallback compositorIntegrationRegister;
} WGPUSurfaceDescriptorCocoaCustomSurface;

typedef struct WGPUTextureDescriptorCocoaCustomSurface {
    WGPUChainedStruct chain;
    IOSurfaceRef surface;
} WGPUTextureDescriptorCocoaCustomSurface;

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

typedef void (*WGPUProcAdapterRequestDeviceWithBlock)(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor, WGPURequestDeviceBlockCallback callback);
typedef void (*WGPUProcBufferMapAsyncWithBlock)(WGPUBuffer buffer, WGPUMapModeFlags mode, size_t offset, size_t size, WGPUBufferMapBlockCallback callback);
typedef void (*WGPUProcDeviceCreateComputePipelineAsyncWithBlock)(WGPUDevice device, WGPUComputePipelineDescriptor const * descriptor, WGPUCreateComputePipelineAsyncBlockCallback callback);
typedef void (*WGPUProcDeviceCreateRenderPipelineAsyncWithBlock)(WGPUDevice device, WGPURenderPipelineDescriptor const * descriptor, WGPUCreateRenderPipelineAsyncBlockCallback callback);
typedef bool (*WGPUProcDevicePopErrorScopeWithBlock)(WGPUDevice device, WGPUErrorBlockCallback callback);
typedef void (*WGPUProcDeviceSetDeviceLostCallbackWithBlock)(WGPUDevice device, WGPUDeviceLostBlockCallback callback);
typedef void (*WGPUProcDeviceSetUncapturedErrorCallbackWithBlock)(WGPUDevice device, WGPUErrorBlockCallback callback);
typedef void (*WGPUProcInstanceRequestAdapterWithBlock)(WGPUInstance instance, WGPURequestAdapterOptions const * options, WGPURequestAdapterBlockCallback callback);
typedef void (*WGPUProcQueueOnSubmittedWorkDoneWithBlock)(WGPUQueue queue, uint64_t signalValue, WGPUQueueWorkDoneBlockCallback callback);
typedef void (*WGPUProcShaderModuleGetCompilationInfoWithBlock)(WGPUShaderModule shaderModule, WGPUCompilationInfoBlockCallback callback);

// FIXME: https://github.com/webgpu-native/webgpu-headers/issues/89 is about moving this from WebGPUExt.h to WebGPU.h
typedef WGPUTexture (*WGPUProcSwapChainGetCurrentTexture)(WGPUSwapChain swapChain);

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

WGPU_EXPORT void wgpuAdapterRequestDeviceWithBlock(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor, WGPURequestDeviceBlockCallback callback);
WGPU_EXPORT void wgpuAdapterRequestInvalidDeviceWithBlock(WGPUAdapter adapter, WGPURequestInvalidDeviceBlockCallback callback);
WGPU_EXPORT void wgpuBufferMapAsyncWithBlock(WGPUBuffer buffer, WGPUMapModeFlags mode, size_t offset, size_t size, WGPUBufferMapBlockCallback callback);
WGPU_EXPORT void wgpuDeviceCreateComputePipelineAsyncWithBlock(WGPUDevice device, WGPUComputePipelineDescriptor const * descriptor, WGPUCreateComputePipelineAsyncBlockCallback callback);
WGPU_EXPORT void wgpuDeviceCreateRenderPipelineAsyncWithBlock(WGPUDevice device, WGPURenderPipelineDescriptor const * descriptor, WGPUCreateRenderPipelineAsyncBlockCallback callback);
WGPU_EXPORT bool wgpuDevicePopErrorScopeWithBlock(WGPUDevice device, WGPUErrorBlockCallback callback);
WGPU_EXPORT void wgpuDeviceSetDeviceLostCallbackWithBlock(WGPUDevice device, WGPUDeviceLostBlockCallback callback);
WGPU_EXPORT void wgpuDeviceSetUncapturedErrorCallbackWithBlock(WGPUDevice device, WGPUErrorBlockCallback callback);
WGPU_EXPORT void wgpuInstanceRequestAdapterWithBlock(WGPUInstance instance, WGPURequestAdapterOptions const * options, WGPURequestAdapterBlockCallback callback);
WGPU_EXPORT void wgpuQueueOnSubmittedWorkDoneWithBlock(WGPUQueue queue, WGPUQueueWorkDoneBlockCallback callback);
WGPU_EXPORT void wgpuShaderModuleGetCompilationInfoWithBlock(WGPUShaderModule shaderModule, WGPUCompilationInfoBlockCallback callback);

WGPU_EXPORT IOSurfaceRef wgpuSurfaceCocoaCustomSurfaceGetDisplayBuffer(WGPUSurface);
WGPU_EXPORT IOSurfaceRef wgpuSurfaceCocoaCustomSurfaceGetDrawingBuffer(WGPUSurface);

// FIXME: https://github.com/webgpu-native/webgpu-headers/issues/89 is about moving this from WebGPUExt.h to WebGPU.h
WGPU_EXPORT WGPUTexture wgpuSwapChainGetCurrentTexture(WGPUSwapChain swapChain);

#endif  // !defined(WGPU_SKIP_DECLARATIONS)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // WEBGPUEXT_H_
