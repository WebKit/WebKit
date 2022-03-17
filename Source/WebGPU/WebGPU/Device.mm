/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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
#import "Device.h"

#import "APIConversions.h"
#import "BindGroup.h"
#import "BindGroupLayout.h"
#import "Buffer.h"
#import "CommandEncoder.h"
#import "ComputePipeline.h"
#import "PipelineLayout.h"
#import "QuerySet.h"
#import "Queue.h"
#import "RenderBundleEncoder.h"
#import "RenderPipeline.h"
#import "Sampler.h"
#import "ShaderModule.h"
#import "Surface.h"
#import "SwapChain.h"
#import "Texture.h"
#import <wtf/StdLibExtras.h>

namespace WebGPU {

RefPtr<Device> Device::create(id<MTLDevice> device, String&& deviceLabel, Instance& instance)
{
    id<MTLCommandQueue> commandQueue = [device newCommandQueue];
    if (!commandQueue)
        return nullptr;

    // See the comment in Device::setLabel() about why we're not setting the label on the MTLDevice here.

    commandQueue.label = @"Default queue";
    if (!deviceLabel.isEmpty())
        commandQueue.label = [NSString stringWithFormat:@"Default queue for device %s", deviceLabel.utf8().data()];

    return adoptRef(*new Device(device, commandQueue, instance));
}

Device::Device(id<MTLDevice> device, id<MTLCommandQueue> defaultQueue, Instance& instance)
    : m_device(device)
    , m_defaultQueue(Queue::create(defaultQueue, *this))
    , m_instance(instance)
{
}

Device::~Device() = default;

void Device::destroy()
{
    // FIXME: Implement this.
}

size_t Device::enumerateFeatures(WGPUFeatureName*)
{
    // We support no optional features right now.
    return 0;
}

bool Device::getLimits(WGPUSupportedLimits& limits)
{
    if (limits.nextInChain != nullptr)
        return false;

    // FIXME: Implement this.
    limits.limits = { };
    return true;
}

Queue& Device::getQueue()
{
    return m_defaultQueue;
}

bool Device::hasFeature(WGPUFeatureName)
{
    // We support no optional features right now.
    return false;
}

bool Device::popErrorScope(CompletionHandler<void(WGPUErrorType, String&&)>&& callback)
{
    // FIXME: Implement this.
    UNUSED_PARAM(callback);
    return false;
}

void Device::pushErrorScope(WGPUErrorFilter filter)
{
    // FIXME: Implement this.
    UNUSED_PARAM(filter);
}

void Device::setDeviceLostCallback(Function<void(WGPUDeviceLostReason, String&&)>&& callback)
{
    // FIXME: Implement this.
    UNUSED_PARAM(callback);
}

void Device::setUncapturedErrorCallback(Function<void(WGPUErrorType, String&&)>&& callback)
{
    // FIXME: Implement this.
    UNUSED_PARAM(callback);
}

void Device::setLabel(String&&)
{
    // Because MTLDevices are process-global, we can't set the label on it, because 2 contexts' labels would fight each other.
}

} // namespace WebGPU

void wgpuDeviceRelease(WGPUDevice device)
{
    delete device;
}

WGPUBindGroup wgpuDeviceCreateBindGroup(WGPUDevice device, const WGPUBindGroupDescriptor* descriptor)
{
    auto result = WebGPU::fromAPI(device).createBindGroup(*descriptor);
    return result ? new WGPUBindGroupImpl { result.releaseNonNull() } : nullptr;
}

WGPUBindGroupLayout wgpuDeviceCreateBindGroupLayout(WGPUDevice device, const WGPUBindGroupLayoutDescriptor* descriptor)
{
    auto result = WebGPU::fromAPI(device).createBindGroupLayout(*descriptor);
    return result ? new WGPUBindGroupLayoutImpl { result.releaseNonNull() } : nullptr;
}

WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice device, const WGPUBufferDescriptor* descriptor)
{
    auto result = WebGPU::fromAPI(device).createBuffer(*descriptor);
    return result ? new WGPUBufferImpl { result.releaseNonNull() } : nullptr;
}

WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice device, const WGPUCommandEncoderDescriptor* descriptor)
{
    auto result = WebGPU::fromAPI(device).createCommandEncoder(*descriptor);
    return result ? new WGPUCommandEncoderImpl { result.releaseNonNull() } : nullptr;
}

WGPUComputePipeline wgpuDeviceCreateComputePipeline(WGPUDevice device, const WGPUComputePipelineDescriptor* descriptor)
{
    auto result = WebGPU::fromAPI(device).createComputePipeline(*descriptor);
    return result ? new WGPUComputePipelineImpl { result.releaseNonNull() } : nullptr;
}

void wgpuDeviceCreateComputePipelineAsync(WGPUDevice device, const WGPUComputePipelineDescriptor* descriptor, WGPUCreateComputePipelineAsyncCallback callback, void* userdata)
{
    WebGPU::fromAPI(device).createComputePipelineAsync(*descriptor, [callback, userdata] (WGPUCreatePipelineAsyncStatus status, RefPtr<WebGPU::ComputePipeline>&& pipeline, String&& message) {
        callback(status, pipeline ? new WGPUComputePipelineImpl { pipeline.releaseNonNull() } : nullptr, message.utf8().data(), userdata);
    });
}

void wgpuDeviceCreateComputePipelineAsyncWithBlock(WGPUDevice device, WGPUComputePipelineDescriptor const * descriptor, WGPUCreateComputePipelineAsyncBlockCallback callback)
{
    WebGPU::fromAPI(device).createComputePipelineAsync(*descriptor, [callback] (WGPUCreatePipelineAsyncStatus status, RefPtr<WebGPU::ComputePipeline>&& pipeline, String&& message) {
        callback(status, pipeline ? new WGPUComputePipelineImpl { pipeline.releaseNonNull() } : nullptr, message.utf8().data());
    });
}

WGPUPipelineLayout wgpuDeviceCreatePipelineLayout(WGPUDevice device, const WGPUPipelineLayoutDescriptor* descriptor)
{
    auto result = WebGPU::fromAPI(device).createPipelineLayout(*descriptor);
    return result ? new WGPUPipelineLayoutImpl { result.releaseNonNull() } : nullptr;
}

WGPUQuerySet wgpuDeviceCreateQuerySet(WGPUDevice device, const WGPUQuerySetDescriptor* descriptor)
{
    auto result = WebGPU::fromAPI(device).createQuerySet(*descriptor);
    return result ? new WGPUQuerySetImpl { result.releaseNonNull() } : nullptr;
}

WGPURenderBundleEncoder wgpuDeviceCreateRenderBundleEncoder(WGPUDevice device, const WGPURenderBundleEncoderDescriptor* descriptor)
{
    auto result = WebGPU::fromAPI(device).createRenderBundleEncoder(*descriptor);
    return result ? new WGPURenderBundleEncoderImpl { result.releaseNonNull() } : nullptr;
}

WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice device, const WGPURenderPipelineDescriptor* descriptor)
{
    auto result = WebGPU::fromAPI(device).createRenderPipeline(*descriptor);
    return result ? new WGPURenderPipelineImpl { result.releaseNonNull() } : nullptr;
}

void wgpuDeviceCreateRenderPipelineAsync(WGPUDevice device, const WGPURenderPipelineDescriptor* descriptor, WGPUCreateRenderPipelineAsyncCallback callback, void* userdata)
{
    WebGPU::fromAPI(device).createRenderPipelineAsync(*descriptor, [callback, userdata] (WGPUCreatePipelineAsyncStatus status, RefPtr<WebGPU::RenderPipeline>&& pipeline, String&& message) {
        callback(status, pipeline ? new WGPURenderPipelineImpl { pipeline.releaseNonNull() } : nullptr, message.utf8().data(), userdata);
    });
}

void wgpuDeviceCreateRenderPipelineAsyncWithBlock(WGPUDevice device, WGPURenderPipelineDescriptor const * descriptor, WGPUCreateRenderPipelineAsyncBlockCallback callback)
{
    WebGPU::fromAPI(device).createRenderPipelineAsync(*descriptor, [callback] (WGPUCreatePipelineAsyncStatus status, RefPtr<WebGPU::RenderPipeline>&& pipeline, String&& message) {
        callback(status, pipeline ? new WGPURenderPipelineImpl { pipeline.releaseNonNull() } : nullptr, message.utf8().data());
    });
}

WGPUSampler wgpuDeviceCreateSampler(WGPUDevice device, const WGPUSamplerDescriptor* descriptor)
{
    auto result = WebGPU::fromAPI(device).createSampler(*descriptor);
    return result ? new WGPUSamplerImpl { result.releaseNonNull() } : nullptr;
}

WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice device, const WGPUShaderModuleDescriptor* descriptor)
{
    auto result = WebGPU::fromAPI(device).createShaderModule(*descriptor);
    return result ? new WGPUShaderModuleImpl { result.releaseNonNull() } : nullptr;
}

WGPUSwapChain wgpuDeviceCreateSwapChain(WGPUDevice device, WGPUSurface surface, const WGPUSwapChainDescriptor* descriptor)
{
    auto result = WebGPU::fromAPI(device).createSwapChain(WebGPU::fromAPI(surface), *descriptor);
    return result ? new WGPUSwapChainImpl { result.releaseNonNull() } : nullptr;
}

WGPUTexture wgpuDeviceCreateTexture(WGPUDevice device, const WGPUTextureDescriptor* descriptor)
{
    auto result = WebGPU::fromAPI(device).createTexture(*descriptor);
    return result ? new WGPUTextureImpl { result.releaseNonNull() } : nullptr;
}

void wgpuDeviceDestroy(WGPUDevice device)
{
    WebGPU::fromAPI(device).destroy();
}

size_t wgpuDeviceEnumerateFeatures(WGPUDevice device, WGPUFeatureName* features)
{
    return WebGPU::fromAPI(device).enumerateFeatures(features);
}

bool wgpuDeviceGetLimits(WGPUDevice device, WGPUSupportedLimits* limits)
{
    return WebGPU::fromAPI(device).getLimits(*limits);
}

WGPUQueue wgpuDeviceGetQueue(WGPUDevice device)
{
    return &device->defaultQueue;
}

bool wgpuDeviceHasFeature(WGPUDevice device, WGPUFeatureName feature)
{
    return WebGPU::fromAPI(device).hasFeature(feature);
}

bool wgpuDevicePopErrorScope(WGPUDevice device, WGPUErrorCallback callback, void* userdata)
{
    return WebGPU::fromAPI(device).popErrorScope([callback, userdata] (WGPUErrorType type, String&& message) {
        callback(type, message.utf8().data(), userdata);
    });
}

bool wgpuDevicePopErrorScopeWithBlock(WGPUDevice device, WGPUErrorBlockCallback callback)
{
    return WebGPU::fromAPI(device).popErrorScope([callback] (WGPUErrorType type, String&& message) {
        callback(type, message.utf8().data());
    });
}

void wgpuDevicePushErrorScope(WGPUDevice device, WGPUErrorFilter filter)
{
    WebGPU::fromAPI(device).pushErrorScope(filter);
}

void wgpuDeviceSetDeviceLostCallback(WGPUDevice device, WGPUDeviceLostCallback callback, void* userdata)
{
    return WebGPU::fromAPI(device).setDeviceLostCallback([callback, userdata] (WGPUDeviceLostReason reason, String&& message) {
        if (callback)
            callback(reason, message.utf8().data(), userdata);
    });
}

void wgpuDeviceSetDeviceLostCallbackWithBlock(WGPUDevice device, WGPUDeviceLostBlockCallback callback)
{
    return WebGPU::fromAPI(device).setDeviceLostCallback([callback] (WGPUDeviceLostReason reason, String&& message) {
        if (callback)
            callback(reason, message.utf8().data());
    });
}

void wgpuDeviceSetUncapturedErrorCallback(WGPUDevice device, WGPUErrorCallback callback, void* userdata)
{
    return WebGPU::fromAPI(device).setUncapturedErrorCallback([callback, userdata] (WGPUErrorType type, String&& message) {
        if (callback)
            callback(type, message.utf8().data(), userdata);
    });
}

void wgpuDeviceSetUncapturedErrorCallbackWithBlock(WGPUDevice device, WGPUErrorBlockCallback callback)
{
    return WebGPU::fromAPI(device).setUncapturedErrorCallback([callback] (WGPUErrorType type, String&& message) {
        if (callback)
            callback(type, message.utf8().data());
    });
}

void wgpuDeviceSetLabel(WGPUDevice device, const char* label)
{
    WebGPU::fromAPI(device).setLabel(WebGPU::fromAPI(label));
}
