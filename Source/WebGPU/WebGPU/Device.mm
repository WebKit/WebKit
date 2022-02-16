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
#import "WebGPUExt.h"
#import <wtf/StdLibExtras.h>

namespace WebGPU {

Device::Device() = default;

Device::~Device() = default;

Ref<BindGroup> Device::createBindGroup(const WGPUBindGroupDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return BindGroup::create();
}

Ref<BindGroupLayout> Device::createBindGroupLayout(const WGPUBindGroupLayoutDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return BindGroupLayout::create();
}

Ref<Buffer> Device::createBuffer(const WGPUBufferDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return Buffer::create();
}

Ref<CommandEncoder> Device::createCommandEncoder(const WGPUCommandEncoderDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return CommandEncoder::create();
}

Ref<ComputePipeline> Device::createComputePipeline(const WGPUComputePipelineDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return ComputePipeline::create();
}

void Device::createComputePipelineAsync(const WGPUComputePipelineDescriptor* descriptor, WTF::Function<void(WGPUCreatePipelineAsyncStatus, Ref<ComputePipeline>&&, const char* message)>&& callback)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(callback);
}

Ref<PipelineLayout> Device::createPipelineLayout(const WGPUPipelineLayoutDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return PipelineLayout::create();
}

Ref<QuerySet> Device::createQuerySet(const WGPUQuerySetDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return QuerySet::create();
}

Ref<RenderBundleEncoder> Device::createRenderBundleEncoder(const WGPURenderBundleEncoderDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return RenderBundleEncoder::create();
}

Ref<RenderPipeline> Device::createRenderPipeline(const WGPURenderPipelineDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return RenderPipeline::create();
}

void Device::createRenderPipelineAsync(const WGPURenderPipelineDescriptor* descriptor, WTF::Function<void(WGPUCreatePipelineAsyncStatus, Ref<RenderPipeline>&&, const char* message)>&& callback)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(callback);
}

Ref<Sampler> Device::createSampler(const WGPUSamplerDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return Sampler::create();
}

Ref<ShaderModule> Device::createShaderModule(const WGPUShaderModuleDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return ShaderModule::create();
}

Ref<SwapChain> Device::createSwapChain(const Surface& surface, const WGPUSwapChainDescriptor* descriptor)
{
    UNUSED_PARAM(surface);
    UNUSED_PARAM(descriptor);
    return SwapChain::create();
}

Ref<Texture> Device::createTexture(const WGPUTextureDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return Texture::create();
}

void Device::destroy()
{
}

size_t Device::enumerateFeatures(WGPUFeatureName* features)
{
    UNUSED_PARAM(features);
    return 0;
}

bool Device::getLimits(WGPUSupportedLimits* limits)
{
    UNUSED_PARAM(limits);
    return false;
}

Ref<Queue> Device::getQueue()
{
    return Queue::create();
}

bool Device::hasFeature(WGPUFeatureName feature)
{
    UNUSED_PARAM(feature);
    return false;
}

bool Device::popErrorScope(WTF::Function<void(WGPUErrorType, const char*)>&& callback)
{
    UNUSED_PARAM(callback);
    return false;
}

void Device::pushErrorScope(WGPUErrorFilter filter)
{
    UNUSED_PARAM(filter);
}

void Device::setDeviceLostCallback(WTF::Function<void(WGPUDeviceLostReason, const char*)>&& callback)
{
    UNUSED_PARAM(callback);
}

void Device::setUncapturedErrorCallback(WTF::Function<void(WGPUErrorType, const char*)>&& callback)
{
    UNUSED_PARAM(callback);
}

void Device::setLabel(const char* label)
{
    UNUSED_PARAM(label);
}

} // namespace WebGPU

void wgpuDeviceRelease(WGPUDevice device)
{
    delete device;
}

WGPUBindGroup wgpuDeviceCreateBindGroup(WGPUDevice device, const WGPUBindGroupDescriptor* descriptor)
{
    return new WGPUBindGroupImpl { device->device->createBindGroup(descriptor) };
}

WGPUBindGroupLayout wgpuDeviceCreateBindGroupLayout(WGPUDevice device, const WGPUBindGroupLayoutDescriptor* descriptor)
{
    return new WGPUBindGroupLayoutImpl { device->device->createBindGroupLayout(descriptor) };
}

WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice device, const WGPUBufferDescriptor* descriptor)
{
    return new WGPUBufferImpl { device->device->createBuffer(descriptor) };
}

WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice device, const WGPUCommandEncoderDescriptor* descriptor)
{
    return new WGPUCommandEncoderImpl { device->device->createCommandEncoder(descriptor) };
}

WGPUComputePipeline wgpuDeviceCreateComputePipeline(WGPUDevice device, const WGPUComputePipelineDescriptor* descriptor)
{
    return new WGPUComputePipelineImpl { device->device->createComputePipeline(descriptor) };
}

void wgpuDeviceCreateComputePipelineAsync(WGPUDevice device, const WGPUComputePipelineDescriptor* descriptor, WGPUCreateComputePipelineAsyncCallback callback, void* userdata)
{
    device->device->createComputePipelineAsync(descriptor, [callback, userdata] (WGPUCreatePipelineAsyncStatus status, Ref<WebGPU::ComputePipeline>&& pipeline, const char* message) {
        callback(status, new WGPUComputePipelineImpl { WTFMove(pipeline) }, message, userdata);
    });
}

WGPUPipelineLayout wgpuDeviceCreatePipelineLayout(WGPUDevice device, const WGPUPipelineLayoutDescriptor* descriptor)
{
    return new WGPUPipelineLayoutImpl { device->device->createPipelineLayout(descriptor) };
}

WGPUQuerySet wgpuDeviceCreateQuerySet(WGPUDevice device, const WGPUQuerySetDescriptor* descriptor)
{
    return new WGPUQuerySetImpl { device->device->createQuerySet(descriptor) };
}

WGPURenderBundleEncoder wgpuDeviceCreateRenderBundleEncoder(WGPUDevice device, const WGPURenderBundleEncoderDescriptor* descriptor)
{
    return new WGPURenderBundleEncoderImpl { device->device->createRenderBundleEncoder(descriptor) };
}

WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice device, const WGPURenderPipelineDescriptor* descriptor)
{
    return new WGPURenderPipelineImpl { device->device->createRenderPipeline(descriptor) };
}

void wgpuDeviceCreateRenderPipelineAsync(WGPUDevice device, const WGPURenderPipelineDescriptor* descriptor, WGPUCreateRenderPipelineAsyncCallback callback, void* userdata)
{
    device->device->createRenderPipelineAsync(descriptor, [callback, userdata] (WGPUCreatePipelineAsyncStatus status, Ref<WebGPU::RenderPipeline>&& pipeline, const char* message) {
        callback(status, new WGPURenderPipelineImpl { WTFMove(pipeline) }, message, userdata);
    });
}

WGPUSampler wgpuDeviceCreateSampler(WGPUDevice device, const WGPUSamplerDescriptor* descriptor)
{
    return new WGPUSamplerImpl { device->device->createSampler(descriptor) };
}

WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice device, const WGPUShaderModuleDescriptor* descriptor)
{
    return new WGPUShaderModuleImpl { device->device->createShaderModule(descriptor) };
}

WGPUSwapChain wgpuDeviceCreateSwapChain(WGPUDevice device, WGPUSurface surface, const WGPUSwapChainDescriptor* descriptor)
{
    return new WGPUSwapChainImpl { device->device->createSwapChain(surface->surface, descriptor) };
}

WGPUTexture wgpuDeviceCreateTexture(WGPUDevice device, const WGPUTextureDescriptor* descriptor)
{
    return new WGPUTextureImpl { device->device->createTexture(descriptor) };
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
    return new WGPUQueueImpl { device->device->getQueue() };
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

void wgpuDeviceSetUncapturedErrorCallback(WGPUDevice device, WGPUErrorCallback callback, void* userdata)
{
    return device->device->setUncapturedErrorCallback([callback, userdata] (WGPUErrorType type, const char* message) {
        if (callback)
            callback(type, message, userdata);
    });
}

void wgpuDeviceSetLabel(WGPUDevice device, const char* label)
{
    device->device->setLabel(label);
}
