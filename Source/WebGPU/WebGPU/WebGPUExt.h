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

#include <CoreVideo/CoreVideo.h>
#include <IOSurface/IOSurfaceRef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WGPUExternalTextureImpl* WGPUExternalTexture;

typedef void (^WGPUBufferMapBlockCallback)(WGPUBufferMapAsyncStatus status) WGPU_FUNCTION_ATTRIBUTE;
typedef void (^WGPUCompilationInfoBlockCallback)(WGPUCompilationInfoRequestStatus status, WGPUCompilationInfo const * compilationInfo) WGPU_FUNCTION_ATTRIBUTE;
typedef void (^WGPUCreateComputePipelineAsyncBlockCallback)(WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline pipeline, char const * message) WGPU_FUNCTION_ATTRIBUTE;
typedef void (^WGPUCreateRenderPipelineAsyncBlockCallback)(WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline pipeline, char const * message) WGPU_FUNCTION_ATTRIBUTE;
typedef void (^WGPUErrorBlockCallback)(WGPUErrorType type, char const * message); // FIXME: Why doesn't this have WGPU_FUNCTION_ATTRIBUTE?
typedef void (^WGPUQueueWorkDoneBlockCallback)(WGPUQueueWorkDoneStatus status) WGPU_FUNCTION_ATTRIBUTE;
typedef void (^WGPURequestAdapterBlockCallback)(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const * message) WGPU_FUNCTION_ATTRIBUTE;
typedef void (^WGPURequestDeviceBlockCallback)(WGPURequestDeviceStatus status, WGPUDevice device, char const * message) WGPU_FUNCTION_ATTRIBUTE;
typedef void (^WGPURequestInvalidDeviceBlockCallback)(WGPUDevice device) WGPU_FUNCTION_ATTRIBUTE;
typedef void (^WGPUWorkItem)(void);
typedef void (^WGPUScheduleWorkBlock)(WGPUWorkItem workItem);

typedef enum WGPUBufferBindingTypeExtended {
    WGPUBufferBindingType_Float3x2 = WGPUBufferBindingType_Force32 - 1,
    WGPUBufferBindingType_Float4x3 = WGPUBufferBindingType_Force32 - 2,
} WGPUBufferBindingTypeExtended WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUColorSpace {
    SRGB,
    DisplayP3,
} WGPUColorSpace WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUSTypeExtended {
    WGPUSTypeExtended_InstanceCocoaDescriptor = 0x151BBC00, // Random
    WGPUSTypeExtended_SurfaceDescriptorCocoaSurfaceBacking = 0x017E9710, // Random
    WGPUSTypeExtended_BindGroupEntryExternalTexture = 0xF7A6EBF9, // Random
    WGPUSTypeExtended_BindGroupLayoutEntryExternalTexture = 0x645C3DAA, // Random
    WGPUSTypeExtended_Force32 = 0x7FFFFFFF
} WGPUSTypeExtended WGPU_ENUM_ATTRIBUTE;

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
} WGPUInstanceCocoaDescriptor WGPU_STRUCTURE_ATTRIBUTE;

typedef void (^WGPURenderBuffersWereRecreatedBlockCallback)(CFArrayRef ioSurfaces) WGPU_FUNCTION_ATTRIBUTE;
typedef void (^WGPUOnSubmittedWorkScheduledCallback)(WGPUWorkItem) WGPU_FUNCTION_ATTRIBUTE;
typedef void (^WGPUCompositorIntegrationRegisterBlockCallback)(WGPURenderBuffersWereRecreatedBlockCallback renderBuffersWereRecreated, WGPUOnSubmittedWorkScheduledCallback onSubmittedWorkScheduledCallback) WGPU_FUNCTION_ATTRIBUTE;
typedef struct WGPUSurfaceDescriptorCocoaCustomSurface {
    WGPUChainedStruct chain;
    WGPUCompositorIntegrationRegisterBlockCallback compositorIntegrationRegister;
} WGPUSurfaceDescriptorCocoaCustomSurface WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUExternalTextureBindingLayout {
    WGPUChainedStruct const * nextInChain;
} WGPUExternalTextureBindingLayout WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUExternalTextureBindGroupLayoutEntry {
    WGPUChainedStruct chain;
    WGPUExternalTextureBindingLayout externalTexture;
} WGPUExternalTextureBindGroupLayoutEntry WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUBindGroupExternalTextureEntry {
    WGPUChainedStruct chain;
    WGPUExternalTexture externalTexture;
} WGPUBindGroupExternalTextureEntry WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUExternalTextureDescriptor {
    WGPUChainedStruct const * nextInChain;
    char const * label; // nullable
    CVPixelBufferRef pixelBuffer;
    WGPUColorSpace colorSpace;
} WGPUExternalTextureDescriptor WGPU_STRUCTURE_ATTRIBUTE;

#if !defined(WGPU_SKIP_PROCS)

typedef void (*WGPUProcBindGroupLayoutSetLabel)(WGPUBindGroupLayout bindGroupLayout, char const * label) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcBindGroupSetLabel)(WGPUBindGroup bindGroup, char const * label) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcBufferSetLabel)(WGPUBuffer buffer, char const * label) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcCommandBufferSetLabel)(WGPUCommandBuffer commandBuffer, char const * label) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcCommandEncoderSetLabel)(WGPUCommandEncoder commandEncoder, char const * label) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcComputePassEncoderSetLabel)(WGPUComputePassEncoder computePassEncoder, char const * label) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcDeviceSetLabel)(WGPUDevice queue, char const * label) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcPipelineLayoutSetLabel)(WGPUPipelineLayout pipelineLayout, char const * label) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcQuerySetSetLabel)(WGPUQuerySet querySet, char const * label) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcQueueSetLabel)(WGPUQueue queue, char const * label) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcRenderBundleEncoderSetLabel)(WGPURenderBundleEncoder renderBundleEncoder, char const * label) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcRenderBundleSetLabel)(WGPURenderBundle renderBundle, char const * label) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcRenderPassEncoderSetLabel)(WGPURenderPassEncoder renderBundleEncoder, char const * label) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcSamplerSetLabel)(WGPUSampler sampler, char const * label) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcTextureSetLabel)(WGPUTexture sampler, char const * label) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcTextureViewSetLabel)(WGPUTextureView sampler, char const * label) WGPU_FUNCTION_ATTRIBUTE;

typedef void (*WGPUProcAdapterRequestDeviceWithBlock)(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor, WGPURequestDeviceBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcBufferMapAsyncWithBlock)(WGPUBuffer buffer, WGPUMapModeFlags mode, size_t offset, size_t size, WGPUBufferMapBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcDeviceCreateComputePipelineAsyncWithBlock)(WGPUDevice device, WGPUComputePipelineDescriptor const * descriptor, WGPUCreateComputePipelineAsyncBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcDeviceCreateRenderPipelineAsyncWithBlock)(WGPUDevice device, WGPURenderPipelineDescriptor const * descriptor, WGPUCreateRenderPipelineAsyncBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;
typedef bool (*WGPUProcDevicePopErrorScopeWithBlock)(WGPUDevice device, WGPUErrorBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcDeviceSetUncapturedErrorCallbackWithBlock)(WGPUDevice device, WGPUErrorBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcInstanceRequestAdapterWithBlock)(WGPUInstance instance, WGPURequestAdapterOptions const * options, WGPURequestAdapterBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcQueueOnSubmittedWorkDoneWithBlock)(WGPUQueue queue, uint64_t signalValue, WGPUQueueWorkDoneBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUProcShaderModuleGetCompilationInfoWithBlock)(WGPUShaderModule shaderModule, WGPUCompilationInfoBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;

// FIXME: https://github.com/webgpu-native/webgpu-headers/issues/89 is about moving this from WebGPUExt.h to WebGPU.h
typedef WGPUTexture (*WGPUProcSwapChainGetCurrentTexture)(WGPUSwapChain swapChain) WGPU_FUNCTION_ATTRIBUTE;

#endif  // !defined(WGPU_SKIP_PROCS)

#if !defined(WGPU_SKIP_DECLARATIONS)

WGPU_EXPORT void wgpuBindGroupLayoutSetLabel(WGPUBindGroupLayout bindGroupLayout, char const * label) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuBindGroupSetLabel(WGPUBindGroup bindGroup, char const * label) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuBufferSetLabel(WGPUBuffer buffer, char const * label) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuCommandBufferSetLabel(WGPUCommandBuffer commandBuffer, char const * label) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuCommandEncoderSetLabel(WGPUCommandEncoder commandEncoder, char const * label) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuComputePassEncoderSetLabel(WGPUComputePassEncoder computePassEncoder, char const * label) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuDeviceSetLabel(WGPUDevice queue, char const * label) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuPipelineLayoutSetLabel(WGPUPipelineLayout pipelineLayout, char const * label) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuQuerySetSetLabel(WGPUQuerySet querySet, char const * label) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuQueueSetLabel(WGPUQueue queue, char const * label) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuRenderBundleEncoderSetLabel(WGPURenderBundleEncoder renderBundleEncoder, char const * label) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuRenderBundleSetLabel(WGPURenderBundle renderBundle, char const * label) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuRenderPassEncoderSetLabel(WGPURenderPassEncoder renderBundleEncoder, char const * label) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuSamplerSetLabel(WGPUSampler sampler, char const * label) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuTextureSetLabel(WGPUTexture sampler, char const * label) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuTextureViewSetLabel(WGPUTextureView sampler, char const * label) WGPU_FUNCTION_ATTRIBUTE;

WGPU_EXPORT void wgpuAdapterRequestDeviceWithBlock(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor, WGPURequestDeviceBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuBufferMapAsyncWithBlock(WGPUBuffer buffer, WGPUMapModeFlags mode, size_t offset, size_t size, WGPUBufferMapBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuDeviceCreateComputePipelineAsyncWithBlock(WGPUDevice device, WGPUComputePipelineDescriptor const * descriptor, WGPUCreateComputePipelineAsyncBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuDeviceCreateRenderPipelineAsyncWithBlock(WGPUDevice device, WGPURenderPipelineDescriptor const * descriptor, WGPUCreateRenderPipelineAsyncBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT WGPUExternalTexture wgpuDeviceImportExternalTexture(WGPUDevice device, const WGPUExternalTextureDescriptor* descriptor) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuDevicePopErrorScopeWithBlock(WGPUDevice device, WGPUErrorBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuDeviceSetUncapturedErrorCallbackWithBlock(WGPUDevice device, WGPUErrorBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuInstanceRequestAdapterWithBlock(WGPUInstance instance, WGPURequestAdapterOptions const * options, WGPURequestAdapterBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuQueueOnSubmittedWorkDoneWithBlock(WGPUQueue queue, WGPUQueueWorkDoneBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;
WGPU_EXPORT void wgpuShaderModuleGetCompilationInfoWithBlock(WGPUShaderModule shaderModule, WGPUCompilationInfoBlockCallback callback) WGPU_FUNCTION_ATTRIBUTE;

// FIXME: https://github.com/webgpu-native/webgpu-headers/issues/89 is about moving this from WebGPUExt.h to WebGPU.h
WGPU_EXPORT WGPUTexture wgpuSwapChainGetCurrentTexture(WGPUSwapChain swapChain) WGPU_FUNCTION_ATTRIBUTE;

#endif  // !defined(WGPU_SKIP_DECLARATIONS)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // WEBGPUEXT_H_
