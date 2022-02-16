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

#import "config.h"
#import "Instance.h"

#import "Adapter.h"
#import "Surface.h"
#import "WebGPUExt.h"
#import <cstring>
#import <wtf/StdLibExtras.h>

namespace WebGPU {

Instance::Instance() = default;

Instance::~Instance() = default;

Ref<Surface> Instance::createSurface(const WGPUSurfaceDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return Surface::create();
}

void Instance::processEvents()
{
}

void Instance::requestAdapter(const WGPURequestAdapterOptions* options, WTF::Function<void(WGPURequestAdapterStatus, Ref<Adapter>&&, const char*)>&& callback)
{
    UNUSED_PARAM(options);
    UNUSED_PARAM(callback);
    callback(WGPURequestAdapterStatus_Unavailable, Adapter::create(), "Adapter");
}

} // namespace WebGPU

void wgpuInstanceRelease(WGPUInstance instance)
{
    delete instance;
}

WGPUInstance wgpuCreateInstance(const WGPUInstanceDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return new WGPUInstanceImpl { WebGPU::Instance::create() };
}

WGPUProc wgpuGetProcAddress(WGPUDevice device, const char* procName)
{
    UNUSED_PARAM(device);
    // FIXME: Use gperf to make this faster.
    // FIXME: Generate this at build time
    if (!strcmp(procName, "wgpuAdapterEnumerateFeatures"))
        return reinterpret_cast<WGPUProc>(&wgpuAdapterEnumerateFeatures);
    if (!strcmp(procName, "wgpuAdapterGetLimits"))
        return reinterpret_cast<WGPUProc>(&wgpuAdapterGetLimits);
    if (!strcmp(procName, "wgpuAdapterGetProperties"))
        return reinterpret_cast<WGPUProc>(&wgpuAdapterGetProperties);
    if (!strcmp(procName, "wgpuAdapterHasFeature"))
        return reinterpret_cast<WGPUProc>(&wgpuAdapterHasFeature);
    if (!strcmp(procName, "wgpuAdapterRequestDevice"))
        return reinterpret_cast<WGPUProc>(&wgpuAdapterRequestDevice);
    if (!strcmp(procName, "wgpuBufferDestroy"))
        return reinterpret_cast<WGPUProc>(&wgpuBufferDestroy);
    if (!strcmp(procName, "wgpuBufferGetConstMappedRange"))
        return reinterpret_cast<WGPUProc>(&wgpuBufferGetConstMappedRange);
    if (!strcmp(procName, "wgpuBufferGetMappedRange"))
        return reinterpret_cast<WGPUProc>(&wgpuBufferGetMappedRange);
    if (!strcmp(procName, "wgpuBufferMapAsync"))
        return reinterpret_cast<WGPUProc>(&wgpuBufferMapAsync);
    if (!strcmp(procName, "wgpuBufferUnmap"))
        return reinterpret_cast<WGPUProc>(&wgpuBufferUnmap);
    if (!strcmp(procName, "wgpuCommandEncoderBeginComputePass"))
        return reinterpret_cast<WGPUProc>(&wgpuCommandEncoderBeginComputePass);
    if (!strcmp(procName, "wgpuCommandEncoderBeginRenderPass"))
        return reinterpret_cast<WGPUProc>(&wgpuCommandEncoderBeginRenderPass);
    if (!strcmp(procName, "wgpuCommandEncoderClearBuffer"))
        return reinterpret_cast<WGPUProc>(&wgpuCommandEncoderClearBuffer);
    if (!strcmp(procName, "wgpuCommandEncoderCopyBufferToBuffer"))
        return reinterpret_cast<WGPUProc>(&wgpuCommandEncoderCopyBufferToBuffer);
    if (!strcmp(procName, "wgpuCommandEncoderCopyBufferToTexture"))
        return reinterpret_cast<WGPUProc>(&wgpuCommandEncoderCopyBufferToTexture);
    if (!strcmp(procName, "wgpuCommandEncoderCopyTextureToBuffer"))
        return reinterpret_cast<WGPUProc>(&wgpuCommandEncoderCopyTextureToBuffer);
    if (!strcmp(procName, "wgpuCommandEncoderCopyTextureToTexture"))
        return reinterpret_cast<WGPUProc>(&wgpuCommandEncoderCopyTextureToTexture);
    if (!strcmp(procName, "wgpuCommandEncoderFinish"))
        return reinterpret_cast<WGPUProc>(&wgpuCommandEncoderFinish);
    if (!strcmp(procName, "wgpuCommandEncoderInsertDebugMarker"))
        return reinterpret_cast<WGPUProc>(&wgpuCommandEncoderInsertDebugMarker);
    if (!strcmp(procName, "wgpuCommandEncoderPopDebugGroup"))
        return reinterpret_cast<WGPUProc>(&wgpuCommandEncoderPopDebugGroup);
    if (!strcmp(procName, "wgpuCommandEncoderPushDebugGroup"))
        return reinterpret_cast<WGPUProc>(&wgpuCommandEncoderPushDebugGroup);
    if (!strcmp(procName, "wgpuCommandEncoderResolveQuerySet"))
        return reinterpret_cast<WGPUProc>(&wgpuCommandEncoderResolveQuerySet);
    if (!strcmp(procName, "wgpuCommandEncoderWriteTimestamp"))
        return reinterpret_cast<WGPUProc>(&wgpuCommandEncoderWriteTimestamp);
    if (!strcmp(procName, "wgpuComputePassEncoderBeginPipelineStatisticsQuery"))
        return reinterpret_cast<WGPUProc>(&wgpuComputePassEncoderBeginPipelineStatisticsQuery);
    if (!strcmp(procName, "wgpuComputePassEncoderDispatch"))
        return reinterpret_cast<WGPUProc>(&wgpuComputePassEncoderDispatch);
    if (!strcmp(procName, "wgpuComputePassEncoderDispatchIndirect"))
        return reinterpret_cast<WGPUProc>(&wgpuComputePassEncoderDispatchIndirect);
    if (!strcmp(procName, "wgpuComputePassEncoderEndPass"))
        return reinterpret_cast<WGPUProc>(&wgpuComputePassEncoderEndPass);
    if (!strcmp(procName, "wgpuComputePassEncoderEndPipelineStatisticsQuery"))
        return reinterpret_cast<WGPUProc>(&wgpuComputePassEncoderEndPipelineStatisticsQuery);
    if (!strcmp(procName, "wgpuComputePassEncoderInsertDebugMarker"))
        return reinterpret_cast<WGPUProc>(&wgpuComputePassEncoderInsertDebugMarker);
    if (!strcmp(procName, "wgpuComputePassEncoderPopDebugGroup"))
        return reinterpret_cast<WGPUProc>(&wgpuComputePassEncoderPopDebugGroup);
    if (!strcmp(procName, "wgpuComputePassEncoderPushDebugGroup"))
        return reinterpret_cast<WGPUProc>(&wgpuComputePassEncoderPushDebugGroup);
    if (!strcmp(procName, "wgpuComputePassEncoderSetBindGroup"))
        return reinterpret_cast<WGPUProc>(&wgpuComputePassEncoderSetBindGroup);
    if (!strcmp(procName, "wgpuComputePassEncoderSetPipeline"))
        return reinterpret_cast<WGPUProc>(&wgpuComputePassEncoderSetPipeline);
    if (!strcmp(procName, "wgpuComputePipelineGetBindGroupLayout"))
        return reinterpret_cast<WGPUProc>(&wgpuComputePipelineGetBindGroupLayout);
    if (!strcmp(procName, "wgpuComputePipelineSetLabel"))
        return reinterpret_cast<WGPUProc>(&wgpuComputePipelineSetLabel);
    if (!strcmp(procName, "wgpuCreateInstance"))
        return reinterpret_cast<WGPUProc>(&wgpuCreateInstance);
    if (!strcmp(procName, "wgpuDeviceCreateBindGroup"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceCreateBindGroup);
    if (!strcmp(procName, "wgpuDeviceCreateBindGroupLayout"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceCreateBindGroupLayout);
    if (!strcmp(procName, "wgpuDeviceCreateBuffer"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceCreateBuffer);
    if (!strcmp(procName, "wgpuDeviceCreateCommandEncoder"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceCreateCommandEncoder);
    if (!strcmp(procName, "wgpuDeviceCreateComputePipeline"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceCreateComputePipeline);
    if (!strcmp(procName, "wgpuDeviceCreateComputePipelineAsync"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceCreateComputePipelineAsync);
    if (!strcmp(procName, "wgpuDeviceCreatePipelineLayout"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceCreatePipelineLayout);
    if (!strcmp(procName, "wgpuDeviceCreateQuerySet"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceCreateQuerySet);
    if (!strcmp(procName, "wgpuDeviceCreateRenderBundleEncoder"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceCreateRenderBundleEncoder);
    if (!strcmp(procName, "wgpuDeviceCreateRenderPipeline"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceCreateRenderPipeline);
    if (!strcmp(procName, "wgpuDeviceCreateRenderPipelineAsync"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceCreateRenderPipelineAsync);
    if (!strcmp(procName, "wgpuDeviceCreateSampler"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceCreateSampler);
    if (!strcmp(procName, "wgpuDeviceCreateShaderModule"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceCreateShaderModule);
    if (!strcmp(procName, "wgpuDeviceCreateSwapChain"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceCreateSwapChain);
    if (!strcmp(procName, "wgpuDeviceCreateTexture"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceCreateTexture);
    if (!strcmp(procName, "wgpuDeviceDestroy"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceDestroy);
    if (!strcmp(procName, "wgpuDeviceEnumerateFeatures"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceEnumerateFeatures);
    if (!strcmp(procName, "wgpuDeviceGetLimits"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceGetLimits);
    if (!strcmp(procName, "wgpuDeviceGetQueue"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceGetQueue);
    if (!strcmp(procName, "wgpuDeviceHasFeature"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceHasFeature);
    if (!strcmp(procName, "wgpuDevicePopErrorScope"))
        return reinterpret_cast<WGPUProc>(&wgpuDevicePopErrorScope);
    if (!strcmp(procName, "wgpuDevicePushErrorScope"))
        return reinterpret_cast<WGPUProc>(&wgpuDevicePushErrorScope);
    if (!strcmp(procName, "wgpuDeviceSetDeviceLostCallback"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceSetDeviceLostCallback);
    if (!strcmp(procName, "wgpuDeviceSetUncapturedErrorCallback"))
        return reinterpret_cast<WGPUProc>(&wgpuDeviceSetUncapturedErrorCallback);
    if (!strcmp(procName, "wgpuGetProcAddress"))
        return reinterpret_cast<WGPUProc>(&wgpuGetProcAddress);
    if (!strcmp(procName, "wgpuInstanceCreateSurface"))
        return reinterpret_cast<WGPUProc>(&wgpuInstanceCreateSurface);
    if (!strcmp(procName, "wgpuInstanceProcessEvents"))
        return reinterpret_cast<WGPUProc>(&wgpuInstanceProcessEvents);
    if (!strcmp(procName, "wgpuInstanceRequestAdapter"))
        return reinterpret_cast<WGPUProc>(&wgpuInstanceRequestAdapter);
    if (!strcmp(procName, "wgpuQuerySetDestroy"))
        return reinterpret_cast<WGPUProc>(&wgpuQuerySetDestroy);
    if (!strcmp(procName, "wgpuQueueOnSubmittedWorkDone"))
        return reinterpret_cast<WGPUProc>(&wgpuQueueOnSubmittedWorkDone);
    if (!strcmp(procName, "wgpuQueueSubmit"))
        return reinterpret_cast<WGPUProc>(&wgpuQueueSubmit);
    if (!strcmp(procName, "wgpuQueueWriteBuffer"))
        return reinterpret_cast<WGPUProc>(&wgpuQueueWriteBuffer);
    if (!strcmp(procName, "wgpuQueueWriteTexture"))
        return reinterpret_cast<WGPUProc>(&wgpuQueueWriteTexture);
    if (!strcmp(procName, "wgpuRenderBundleEncoderDraw"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderBundleEncoderDraw);
    if (!strcmp(procName, "wgpuRenderBundleEncoderDrawIndexed"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderBundleEncoderDrawIndexed);
    if (!strcmp(procName, "wgpuRenderBundleEncoderDrawIndexedIndirect"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderBundleEncoderDrawIndexedIndirect);
    if (!strcmp(procName, "wgpuRenderBundleEncoderDrawIndirect"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderBundleEncoderDrawIndirect);
    if (!strcmp(procName, "wgpuRenderBundleEncoderFinish"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderBundleEncoderFinish);
    if (!strcmp(procName, "wgpuRenderBundleEncoderInsertDebugMarker"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderBundleEncoderInsertDebugMarker);
    if (!strcmp(procName, "wgpuRenderBundleEncoderPopDebugGroup"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderBundleEncoderPopDebugGroup);
    if (!strcmp(procName, "wgpuRenderBundleEncoderPushDebugGroup"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderBundleEncoderPushDebugGroup);
    if (!strcmp(procName, "wgpuRenderBundleEncoderSetBindGroup"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderBundleEncoderSetBindGroup);
    if (!strcmp(procName, "wgpuRenderBundleEncoderSetIndexBuffer"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderBundleEncoderSetIndexBuffer);
    if (!strcmp(procName, "wgpuRenderBundleEncoderSetPipeline"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderBundleEncoderSetPipeline);
    if (!strcmp(procName, "wgpuRenderBundleEncoderSetVertexBuffer"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderBundleEncoderSetVertexBuffer);
    if (!strcmp(procName, "wgpuRenderPassEncoderBeginOcclusionQuery"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderBeginOcclusionQuery);
    if (!strcmp(procName, "wgpuRenderPassEncoderBeginPipelineStatisticsQuery"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderBeginPipelineStatisticsQuery);
    if (!strcmp(procName, "wgpuRenderPassEncoderDraw"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderDraw);
    if (!strcmp(procName, "wgpuRenderPassEncoderDrawIndexed"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderDrawIndexed);
    if (!strcmp(procName, "wgpuRenderPassEncoderDrawIndexedIndirect"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderDrawIndexedIndirect);
    if (!strcmp(procName, "wgpuRenderPassEncoderDrawIndirect"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderDrawIndirect);
    if (!strcmp(procName, "wgpuRenderPassEncoderEndOcclusionQuery"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderEndOcclusionQuery);
    if (!strcmp(procName, "wgpuRenderPassEncoderEndPass"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderEndPass);
    if (!strcmp(procName, "wgpuRenderPassEncoderEndPipelineStatisticsQuery"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderEndPipelineStatisticsQuery);
    if (!strcmp(procName, "wgpuRenderPassEncoderExecuteBundles"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderExecuteBundles);
    if (!strcmp(procName, "wgpuRenderPassEncoderInsertDebugMarker"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderInsertDebugMarker);
    if (!strcmp(procName, "wgpuRenderPassEncoderPopDebugGroup"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderPopDebugGroup);
    if (!strcmp(procName, "wgpuRenderPassEncoderPushDebugGroup"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderPushDebugGroup);
    if (!strcmp(procName, "wgpuRenderPassEncoderSetBindGroup"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderSetBindGroup);
    if (!strcmp(procName, "wgpuRenderPassEncoderSetBlendConstant"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderSetBlendConstant);
    if (!strcmp(procName, "wgpuRenderPassEncoderSetIndexBuffer"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderSetIndexBuffer);
    if (!strcmp(procName, "wgpuRenderPassEncoderSetPipeline"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderSetPipeline);
    if (!strcmp(procName, "wgpuRenderPassEncoderSetScissorRect"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderSetScissorRect);
    if (!strcmp(procName, "wgpuRenderPassEncoderSetStencilReference"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderSetStencilReference);
    if (!strcmp(procName, "wgpuRenderPassEncoderSetVertexBuffer"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderSetVertexBuffer);
    if (!strcmp(procName, "wgpuRenderPassEncoderSetViewport"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPassEncoderSetViewport);
    if (!strcmp(procName, "wgpuRenderPipelineGetBindGroupLayout"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPipelineGetBindGroupLayout);
    if (!strcmp(procName, "wgpuRenderPipelineSetLabel"))
        return reinterpret_cast<WGPUProc>(&wgpuRenderPipelineSetLabel);
    if (!strcmp(procName, "wgpuShaderModuleGetCompilationInfo"))
        return reinterpret_cast<WGPUProc>(&wgpuShaderModuleGetCompilationInfo);
    if (!strcmp(procName, "wgpuShaderModuleSetLabel"))
        return reinterpret_cast<WGPUProc>(&wgpuShaderModuleSetLabel);
    if (!strcmp(procName, "wgpuSurfaceGetPreferredFormat"))
        return reinterpret_cast<WGPUProc>(&wgpuSurfaceGetPreferredFormat);
    if (!strcmp(procName, "wgpuSwapChainGetCurrentTextureView"))
        return reinterpret_cast<WGPUProc>(&wgpuSwapChainGetCurrentTextureView);
    if (!strcmp(procName, "wgpuSwapChainPresent"))
        return reinterpret_cast<WGPUProc>(&wgpuSwapChainPresent);
    if (!strcmp(procName, "wgpuTextureCreateView"))
        return reinterpret_cast<WGPUProc>(&wgpuTextureCreateView);
    if (!strcmp(procName, "wgpuTextureDestroy"))
        return reinterpret_cast<WGPUProc>(&wgpuTextureDestroy);
    return nullptr;
}

WGPUSurface wgpuInstanceCreateSurface(WGPUInstance instance, const WGPUSurfaceDescriptor* descriptor)
{
    return new WGPUSurfaceImpl { instance->instance->createSurface(descriptor) };
}

void wgpuInstanceProcessEvents(WGPUInstance instance)
{
    instance->instance->processEvents();
}

void wgpuInstanceRequestAdapter(WGPUInstance instance, const WGPURequestAdapterOptions* options, WGPURequestAdapterCallback callback, void* userdata)
{
    instance->instance->requestAdapter(options, [callback, userdata] (WGPURequestAdapterStatus status, Ref<WebGPU::Adapter>&& adapter, const char* message) {
        callback(status, new WGPUAdapterImpl { WTFMove(adapter) }, message, userdata);
    });
}

