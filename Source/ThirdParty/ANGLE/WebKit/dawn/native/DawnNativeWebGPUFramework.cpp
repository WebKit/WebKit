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

#include <dawn/native/DawnNative.h>

namespace dawn::native
{
// The procs of WebGPU.framework, whose WebGPU.h is this webgpu.h. The procs that it does not
// implement are null.
const DawnProcTable &GetProcs()
{
    static const DawnProcTable kProcs = [] {
        DawnProcTable procs { };
        procs.createInstance = wgpuCreateInstance;
        procs.getProcAddress = wgpuGetProcAddress;
        procs.adapterGetInfo = wgpuAdapterGetInfo;
        procs.adapterGetInstance = wgpuAdapterGetInstance;
        procs.adapterGetLimits = wgpuAdapterGetLimits;
        procs.adapterHasFeature = wgpuAdapterHasFeature;
        procs.adapterRequestDevice = wgpuAdapterRequestDevice;
        procs.adapterAddRef = wgpuAdapterAddRef;
        procs.adapterRelease = wgpuAdapterRelease;
        procs.adapterInfoFreeMembers = wgpuAdapterInfoFreeMembers;
        procs.bindGroupSetLabel = wgpuBindGroupSetLabel;
        procs.bindGroupAddRef = wgpuBindGroupAddRef;
        procs.bindGroupRelease = wgpuBindGroupRelease;
        procs.bindGroupLayoutSetLabel = wgpuBindGroupLayoutSetLabel;
        procs.bindGroupLayoutAddRef = wgpuBindGroupLayoutAddRef;
        procs.bindGroupLayoutRelease = wgpuBindGroupLayoutRelease;
        procs.bufferDestroy = wgpuBufferDestroy;
        procs.bufferGetConstMappedRange = wgpuBufferGetConstMappedRange;
        procs.bufferGetMappedRange = wgpuBufferGetMappedRange;
        procs.bufferGetMapState = wgpuBufferGetMapState;
        procs.bufferGetSize = wgpuBufferGetSize;
        procs.bufferGetUsage = wgpuBufferGetUsage;
        procs.bufferMapAsync = wgpuBufferMapAsync;
        procs.bufferSetLabel = wgpuBufferSetLabel;
        procs.bufferUnmap = wgpuBufferUnmap;
        procs.bufferAddRef = wgpuBufferAddRef;
        procs.bufferRelease = wgpuBufferRelease;
        procs.commandBufferSetLabel = wgpuCommandBufferSetLabel;
        procs.commandBufferAddRef = wgpuCommandBufferAddRef;
        procs.commandBufferRelease = wgpuCommandBufferRelease;
        procs.commandEncoderBeginComputePass = wgpuCommandEncoderBeginComputePass;
        procs.commandEncoderBeginRenderPass = wgpuCommandEncoderBeginRenderPass;
        procs.commandEncoderClearBuffer = wgpuCommandEncoderClearBuffer;
        procs.commandEncoderCopyBufferToBuffer = wgpuCommandEncoderCopyBufferToBuffer;
        procs.commandEncoderCopyBufferToTexture = wgpuCommandEncoderCopyBufferToTexture;
        procs.commandEncoderCopyTextureToBuffer = wgpuCommandEncoderCopyTextureToBuffer;
        procs.commandEncoderCopyTextureToTexture = wgpuCommandEncoderCopyTextureToTexture;
        procs.commandEncoderFinish = wgpuCommandEncoderFinish;
        procs.commandEncoderInsertDebugMarker = wgpuCommandEncoderInsertDebugMarker;
        procs.commandEncoderPopDebugGroup = wgpuCommandEncoderPopDebugGroup;
        procs.commandEncoderPushDebugGroup = wgpuCommandEncoderPushDebugGroup;
        procs.commandEncoderResolveQuerySet = wgpuCommandEncoderResolveQuerySet;
        procs.commandEncoderSetLabel = wgpuCommandEncoderSetLabel;
        procs.commandEncoderWriteTimestamp = wgpuCommandEncoderWriteTimestamp;
        procs.commandEncoderAddRef = wgpuCommandEncoderAddRef;
        procs.commandEncoderRelease = wgpuCommandEncoderRelease;
        procs.computePassEncoderDispatchWorkgroups = wgpuComputePassEncoderDispatchWorkgroups;
        procs.computePassEncoderDispatchWorkgroupsIndirect = wgpuComputePassEncoderDispatchWorkgroupsIndirect;
        procs.computePassEncoderEnd = wgpuComputePassEncoderEnd;
        procs.computePassEncoderInsertDebugMarker = wgpuComputePassEncoderInsertDebugMarker;
        procs.computePassEncoderPopDebugGroup = wgpuComputePassEncoderPopDebugGroup;
        procs.computePassEncoderPushDebugGroup = wgpuComputePassEncoderPushDebugGroup;
        procs.computePassEncoderSetBindGroup = wgpuComputePassEncoderSetBindGroup;
        procs.computePassEncoderSetLabel = wgpuComputePassEncoderSetLabel;
        procs.computePassEncoderSetPipeline = wgpuComputePassEncoderSetPipeline;
        procs.computePassEncoderAddRef = wgpuComputePassEncoderAddRef;
        procs.computePassEncoderRelease = wgpuComputePassEncoderRelease;
        procs.computePipelineGetBindGroupLayout = wgpuComputePipelineGetBindGroupLayout;
        procs.computePipelineSetLabel = wgpuComputePipelineSetLabel;
        procs.computePipelineAddRef = wgpuComputePipelineAddRef;
        procs.computePipelineRelease = wgpuComputePipelineRelease;
        procs.deviceCreateBindGroup = wgpuDeviceCreateBindGroup;
        procs.deviceCreateBindGroupLayout = wgpuDeviceCreateBindGroupLayout;
        procs.deviceCreateBuffer = wgpuDeviceCreateBuffer;
        procs.deviceCreateCommandEncoder = wgpuDeviceCreateCommandEncoder;
        procs.deviceCreateComputePipeline = wgpuDeviceCreateComputePipeline;
        procs.deviceCreateComputePipelineAsync = wgpuDeviceCreateComputePipelineAsync;
        procs.deviceCreatePipelineLayout = wgpuDeviceCreatePipelineLayout;
        procs.deviceCreateQuerySet = wgpuDeviceCreateQuerySet;
        procs.deviceCreateRenderBundleEncoder = wgpuDeviceCreateRenderBundleEncoder;
        procs.deviceCreateRenderPipeline = wgpuDeviceCreateRenderPipeline;
        procs.deviceCreateRenderPipelineAsync = wgpuDeviceCreateRenderPipelineAsync;
        procs.deviceCreateSampler = wgpuDeviceCreateSampler;
        procs.deviceCreateShaderModule = wgpuDeviceCreateShaderModule;
        procs.deviceCreateTexture = wgpuDeviceCreateTexture;
        procs.deviceDestroy = wgpuDeviceDestroy;
        procs.deviceGetAdapter = wgpuDeviceGetAdapter;
        procs.deviceGetLimits = wgpuDeviceGetLimits;
        procs.deviceGetQueue = wgpuDeviceGetQueue;
        procs.deviceHasFeature = wgpuDeviceHasFeature;
        procs.devicePopErrorScope = wgpuDevicePopErrorScope;
        procs.devicePushErrorScope = wgpuDevicePushErrorScope;
        procs.deviceSetLabel = wgpuDeviceSetLabel;
        procs.deviceAddRef = wgpuDeviceAddRef;
        procs.deviceRelease = wgpuDeviceRelease;
        procs.externalTextureDestroy = wgpuExternalTextureDestroy;
        procs.externalTextureAddRef = wgpuExternalTextureAddRef;
        procs.externalTextureRelease = wgpuExternalTextureRelease;
        procs.instanceCreateSurface = wgpuInstanceCreateSurface;
        procs.instanceProcessEvents = wgpuInstanceProcessEvents;
        procs.instanceRequestAdapter = wgpuInstanceRequestAdapter;
        procs.instanceWaitAny = wgpuInstanceWaitAny;
        procs.instanceAddRef = wgpuInstanceAddRef;
        procs.instanceRelease = wgpuInstanceRelease;
        procs.pipelineLayoutSetLabel = wgpuPipelineLayoutSetLabel;
        procs.pipelineLayoutAddRef = wgpuPipelineLayoutAddRef;
        procs.pipelineLayoutRelease = wgpuPipelineLayoutRelease;
        procs.querySetDestroy = wgpuQuerySetDestroy;
        procs.querySetGetCount = wgpuQuerySetGetCount;
        procs.querySetGetType = wgpuQuerySetGetType;
        procs.querySetSetLabel = wgpuQuerySetSetLabel;
        procs.querySetAddRef = wgpuQuerySetAddRef;
        procs.querySetRelease = wgpuQuerySetRelease;
        procs.queueOnSubmittedWorkDone = wgpuQueueOnSubmittedWorkDone;
        procs.queueSetLabel = wgpuQueueSetLabel;
        procs.queueSubmit = wgpuQueueSubmit;
        procs.queueWriteBuffer = wgpuQueueWriteBuffer;
        procs.queueWriteTexture = wgpuQueueWriteTexture;
        procs.queueAddRef = wgpuQueueAddRef;
        procs.queueRelease = wgpuQueueRelease;
        procs.renderBundleSetLabel = wgpuRenderBundleSetLabel;
        procs.renderBundleAddRef = wgpuRenderBundleAddRef;
        procs.renderBundleRelease = wgpuRenderBundleRelease;
        procs.renderBundleEncoderDraw = wgpuRenderBundleEncoderDraw;
        procs.renderBundleEncoderDrawIndexed = wgpuRenderBundleEncoderDrawIndexed;
        procs.renderBundleEncoderDrawIndexedIndirect = wgpuRenderBundleEncoderDrawIndexedIndirect;
        procs.renderBundleEncoderDrawIndirect = wgpuRenderBundleEncoderDrawIndirect;
        procs.renderBundleEncoderFinish = wgpuRenderBundleEncoderFinish;
        procs.renderBundleEncoderInsertDebugMarker = wgpuRenderBundleEncoderInsertDebugMarker;
        procs.renderBundleEncoderPopDebugGroup = wgpuRenderBundleEncoderPopDebugGroup;
        procs.renderBundleEncoderPushDebugGroup = wgpuRenderBundleEncoderPushDebugGroup;
        procs.renderBundleEncoderSetBindGroup = wgpuRenderBundleEncoderSetBindGroup;
        procs.renderBundleEncoderSetIndexBuffer = wgpuRenderBundleEncoderSetIndexBuffer;
        procs.renderBundleEncoderSetLabel = wgpuRenderBundleEncoderSetLabel;
        procs.renderBundleEncoderSetPipeline = wgpuRenderBundleEncoderSetPipeline;
        procs.renderBundleEncoderSetVertexBuffer = wgpuRenderBundleEncoderSetVertexBuffer;
        procs.renderBundleEncoderAddRef = wgpuRenderBundleEncoderAddRef;
        procs.renderBundleEncoderRelease = wgpuRenderBundleEncoderRelease;
        procs.renderPassEncoderBeginOcclusionQuery = wgpuRenderPassEncoderBeginOcclusionQuery;
        procs.renderPassEncoderDraw = wgpuRenderPassEncoderDraw;
        procs.renderPassEncoderDrawIndexed = wgpuRenderPassEncoderDrawIndexed;
        procs.renderPassEncoderDrawIndexedIndirect = wgpuRenderPassEncoderDrawIndexedIndirect;
        procs.renderPassEncoderDrawIndirect = wgpuRenderPassEncoderDrawIndirect;
        procs.renderPassEncoderEnd = wgpuRenderPassEncoderEnd;
        procs.renderPassEncoderEndOcclusionQuery = wgpuRenderPassEncoderEndOcclusionQuery;
        procs.renderPassEncoderExecuteBundles = wgpuRenderPassEncoderExecuteBundles;
        procs.renderPassEncoderInsertDebugMarker = wgpuRenderPassEncoderInsertDebugMarker;
        procs.renderPassEncoderPopDebugGroup = wgpuRenderPassEncoderPopDebugGroup;
        procs.renderPassEncoderPushDebugGroup = wgpuRenderPassEncoderPushDebugGroup;
        procs.renderPassEncoderSetBindGroup = wgpuRenderPassEncoderSetBindGroup;
        procs.renderPassEncoderSetBlendConstant = wgpuRenderPassEncoderSetBlendConstant;
        procs.renderPassEncoderSetIndexBuffer = wgpuRenderPassEncoderSetIndexBuffer;
        procs.renderPassEncoderSetLabel = wgpuRenderPassEncoderSetLabel;
        procs.renderPassEncoderSetPipeline = wgpuRenderPassEncoderSetPipeline;
        procs.renderPassEncoderSetScissorRect = wgpuRenderPassEncoderSetScissorRect;
        procs.renderPassEncoderSetStencilReference = wgpuRenderPassEncoderSetStencilReference;
        procs.renderPassEncoderSetVertexBuffer = wgpuRenderPassEncoderSetVertexBuffer;
        procs.renderPassEncoderSetViewport = wgpuRenderPassEncoderSetViewport;
        procs.renderPassEncoderAddRef = wgpuRenderPassEncoderAddRef;
        procs.renderPassEncoderRelease = wgpuRenderPassEncoderRelease;
        procs.renderPipelineGetBindGroupLayout = wgpuRenderPipelineGetBindGroupLayout;
        procs.renderPipelineSetLabel = wgpuRenderPipelineSetLabel;
        procs.renderPipelineAddRef = wgpuRenderPipelineAddRef;
        procs.renderPipelineRelease = wgpuRenderPipelineRelease;
        procs.samplerSetLabel = wgpuSamplerSetLabel;
        procs.samplerAddRef = wgpuSamplerAddRef;
        procs.samplerRelease = wgpuSamplerRelease;
        procs.shaderModuleGetCompilationInfo = wgpuShaderModuleGetCompilationInfo;
        procs.shaderModuleSetLabel = wgpuShaderModuleSetLabel;
        procs.shaderModuleAddRef = wgpuShaderModuleAddRef;
        procs.shaderModuleRelease = wgpuShaderModuleRelease;
        procs.surfaceAddRef = wgpuSurfaceAddRef;
        procs.surfaceRelease = wgpuSurfaceRelease;
        procs.textureCreateView = wgpuTextureCreateView;
        procs.textureDestroy = wgpuTextureDestroy;
        procs.textureGetDepthOrArrayLayers = wgpuTextureGetDepthOrArrayLayers;
        procs.textureGetDimension = wgpuTextureGetDimension;
        procs.textureGetFormat = wgpuTextureGetFormat;
        procs.textureGetHeight = wgpuTextureGetHeight;
        procs.textureGetMipLevelCount = wgpuTextureGetMipLevelCount;
        procs.textureGetSampleCount = wgpuTextureGetSampleCount;
        procs.textureGetUsage = wgpuTextureGetUsage;
        procs.textureGetWidth = wgpuTextureGetWidth;
        procs.textureSetLabel = wgpuTextureSetLabel;
        procs.textureAddRef = wgpuTextureAddRef;
        procs.textureRelease = wgpuTextureRelease;
        procs.textureViewSetLabel = wgpuTextureViewSetLabel;
        procs.textureViewAddRef = wgpuTextureViewAddRef;
        procs.textureViewRelease = wgpuTextureViewRelease;
        return procs;
    }();
    return kProcs;
}
}  // namespace dawn::native
