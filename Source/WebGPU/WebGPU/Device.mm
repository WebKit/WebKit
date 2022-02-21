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

RefPtr<Device> Device::create(id <MTLDevice> device)
{
    id <MTLCommandQueue> commandQueue = [device newCommandQueue];
    if (!commandQueue)
        return nullptr;
    auto queue = Queue::create(commandQueue);

    return adoptRef(*new Device(device, WTFMove(queue)));
}

Device::Device(id <MTLDevice> device, Ref<Queue>&& queue)
    : m_device(device)
    , m_defaultQueue(WTFMove(queue))
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

bool Device::getLimits(WGPUSupportedLimits* limits)
{
    if (limits->nextInChain != nullptr)
        return false;

    // FIXME: Implement this.
    limits->limits = { };
    return true;
}

RefPtr<Queue> Device::getQueue()
{
    return m_defaultQueue.copyRef();
}

bool Device::hasFeature(WGPUFeatureName)
{
    // We support no optional features right now.
    return false;
}

bool Device::popErrorScope(WTF::Function<void(WGPUErrorType, const char*)>&& callback)
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

void Device::setDeviceLostCallback(WTF::Function<void(WGPUDeviceLostReason, const char*)>&& callback)
{
    // FIXME: Implement this.
    UNUSED_PARAM(callback);
}

void Device::setUncapturedErrorCallback(WTF::Function<void(WGPUErrorType, const char*)>&& callback)
{
    // FIXME: Implement this.
    UNUSED_PARAM(callback);
}

void Device::setLabel(const char*)
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
    auto result = device->device->createBindGroup(descriptor);
    return result ? new WGPUBindGroupImpl { result.releaseNonNull() } : nullptr;
}

WGPUBindGroupLayout wgpuDeviceCreateBindGroupLayout(WGPUDevice device, const WGPUBindGroupLayoutDescriptor* descriptor)
{
    auto result = device->device->createBindGroupLayout(descriptor);
    return result ? new WGPUBindGroupLayoutImpl { result.releaseNonNull() } : nullptr;
}

WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice device, const WGPUBufferDescriptor* descriptor)
{
    auto result = device->device->createBuffer(descriptor);
    return result ? new WGPUBufferImpl { result.releaseNonNull() } : nullptr;
}

WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice device, const WGPUCommandEncoderDescriptor* descriptor)
{
    auto result = device->device->createCommandEncoder(descriptor);
    return result ? new WGPUCommandEncoderImpl { result.releaseNonNull() } : nullptr;
}

WGPUComputePipeline wgpuDeviceCreateComputePipeline(WGPUDevice device, const WGPUComputePipelineDescriptor* descriptor)
{
    auto result = device->device->createComputePipeline(descriptor);
    return result ? new WGPUComputePipelineImpl { result.releaseNonNull() } : nullptr;
}

void wgpuDeviceCreateComputePipelineAsync(WGPUDevice device, const WGPUComputePipelineDescriptor* descriptor, WGPUCreateComputePipelineAsyncCallback callback, void* userdata)
{
    device->device->createComputePipelineAsync(descriptor, [callback, userdata] (WGPUCreatePipelineAsyncStatus status, RefPtr<WebGPU::ComputePipeline>&& pipeline, const char* message) {
        callback(status, pipeline ? new WGPUComputePipelineImpl { pipeline.releaseNonNull() } : nullptr, message, userdata);
    });
}

void wgpuDeviceCreateComputePipelineAsyncWithBlock(WGPUDevice device, WGPUComputePipelineDescriptor const * descriptor, WGPUCreateComputePipelineAsyncBlockCallback callback)
{
    device->device->createComputePipelineAsync(descriptor, [callback] (WGPUCreatePipelineAsyncStatus status, RefPtr<WebGPU::ComputePipeline>&& pipeline, const char* message) {
        callback(status, pipeline ? new WGPUComputePipelineImpl { pipeline.releaseNonNull() } : nullptr, message);
    });
}

WGPUPipelineLayout wgpuDeviceCreatePipelineLayout(WGPUDevice device, const WGPUPipelineLayoutDescriptor* descriptor)
{
    auto result = device->device->createPipelineLayout(descriptor);
    return result ? new WGPUPipelineLayoutImpl { result.releaseNonNull() } : nullptr;
}

WGPUQuerySet wgpuDeviceCreateQuerySet(WGPUDevice device, const WGPUQuerySetDescriptor* descriptor)
{
    auto result = device->device->createQuerySet(descriptor);
    return result ? new WGPUQuerySetImpl { result.releaseNonNull() } : nullptr;
}

WGPURenderBundleEncoder wgpuDeviceCreateRenderBundleEncoder(WGPUDevice device, const WGPURenderBundleEncoderDescriptor* descriptor)
{
    auto result = device->device->createRenderBundleEncoder(descriptor);
    return result ? new WGPURenderBundleEncoderImpl { result.releaseNonNull() } : nullptr;
}

WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice device, const WGPURenderPipelineDescriptor* descriptor)
{
    auto result = device->device->createRenderPipeline(descriptor);
    return result ? new WGPURenderPipelineImpl { result.releaseNonNull() } : nullptr;
}

void wgpuDeviceCreateRenderPipelineAsync(WGPUDevice device, const WGPURenderPipelineDescriptor* descriptor, WGPUCreateRenderPipelineAsyncCallback callback, void* userdata)
{
    device->device->createRenderPipelineAsync(descriptor, [callback, userdata] (WGPUCreatePipelineAsyncStatus status, RefPtr<WebGPU::RenderPipeline>&& pipeline, const char* message) {
        callback(status, pipeline ? new WGPURenderPipelineImpl { pipeline.releaseNonNull() } : nullptr, message, userdata);
    });
}

void wgpuDeviceCreateRenderPipelineAsyncWithBlock(WGPUDevice device, WGPURenderPipelineDescriptor const * descriptor, WGPUCreateRenderPipelineAsyncBlockCallback callback)
{
    device->device->createRenderPipelineAsync(descriptor, [callback] (WGPUCreatePipelineAsyncStatus status, RefPtr<WebGPU::RenderPipeline>&& pipeline, const char* message) {
        callback(status, pipeline ? new WGPURenderPipelineImpl { pipeline.releaseNonNull() } : nullptr, message);
    });
}

WGPUSampler wgpuDeviceCreateSampler(WGPUDevice device, const WGPUSamplerDescriptor* descriptor)
{
    auto result = device->device->createSampler(descriptor);
    return result ? new WGPUSamplerImpl { result.releaseNonNull() } : nullptr;
}

WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice device, const WGPUShaderModuleDescriptor* descriptor)
{
    auto result = device->device->createShaderModule(descriptor);
    return result ? new WGPUShaderModuleImpl { result.releaseNonNull() } : nullptr;
}

WGPUSwapChain wgpuDeviceCreateSwapChain(WGPUDevice device, WGPUSurface surface, const WGPUSwapChainDescriptor* descriptor)
{
    auto result = device->device->createSwapChain(surface->surface, descriptor);
    return result ? new WGPUSwapChainImpl { result.releaseNonNull() } : nullptr;
}

WGPUTexture wgpuDeviceCreateTexture(WGPUDevice device, const WGPUTextureDescriptor* descriptor)
{
    auto result = device->device->createTexture(descriptor);
    return result ? new WGPUTextureImpl { result.releaseNonNull() } : nullptr;
}

void wgpuDeviceDestroy(WGPUDevice device)
{
    device->device->destroy();
}

size_t wgpuDeviceEnumerateFeatures(WGPUDevice device, WGPUFeatureName* features)
{
    return device->device->enumerateFeatures(features);
}

bool wgpuDeviceGetLimits(WGPUDevice device, WGPUSupportedLimits* limits)
{
    return device->device->getLimits(limits);
}

WGPUQueue wgpuDeviceGetQueue(WGPUDevice device)
{
    auto result = device->device->getQueue();
    return result ? new WGPUQueueImpl { result.releaseNonNull() } : nullptr;
}

bool wgpuDeviceHasFeature(WGPUDevice device, WGPUFeatureName feature)
{
    return device->device->hasFeature(feature);
}

bool wgpuDevicePopErrorScope(WGPUDevice device, WGPUErrorCallback callback, void* userdata)
{
    return device->device->popErrorScope([callback, userdata] (WGPUErrorType type, const char* message) {
        callback(type, message, userdata);
    });
}

bool wgpuDevicePopErrorScopeWithBlock(WGPUDevice device, WGPUErrorBlockCallback callback)
{
    return device->device->popErrorScope([callback] (WGPUErrorType type, const char* message) {
        callback(type, message);
    });
}

void wgpuDevicePushErrorScope(WGPUDevice device, WGPUErrorFilter filter)
{
    device->device->pushErrorScope(filter);
}

void wgpuDeviceSetDeviceLostCallback(WGPUDevice device, WGPUDeviceLostCallback callback, void* userdata)
{
    return device->device->setDeviceLostCallback([callback, userdata] (WGPUDeviceLostReason reason, const char* message) {
        if (callback)
            callback(reason, message, userdata);
    });
}

void wgpuDeviceSetDeviceLostCallbackWithBlock(WGPUDevice device, WGPUDeviceLostBlockCallback callback)
{
    return device->device->setDeviceLostCallback([callback] (WGPUDeviceLostReason reason, const char* message) {
        if (callback)
            callback(reason, message);
    });
}

void wgpuDeviceSetUncapturedErrorCallback(WGPUDevice device, WGPUErrorCallback callback, void* userdata)
{
    return device->device->setUncapturedErrorCallback([callback, userdata] (WGPUErrorType type, const char* message) {
        if (callback)
            callback(type, message, userdata);
    });
}

void wgpuDeviceSetUncapturedErrorCallbackWithBlock(WGPUDevice device, WGPUErrorBlockCallback callback)
{
    return device->device->setUncapturedErrorCallback([callback] (WGPUErrorType type, const char* message) {
        if (callback)
            callback(type, message);
    });
}

void wgpuDeviceSetLabel(WGPUDevice device, const char* label)
{
    device->device->setLabel(label);
}
