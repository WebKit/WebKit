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
#import "Adapter.h"

#import "APIConversions.h"
#import "Device.h"
#import "Instance.h"
#import <wtf/StdLibExtras.h>

namespace WebGPU {

Adapter::Adapter(id<MTLDevice> device, Instance& instance)
    : m_device(device)
    , m_instance(instance)
{
}

Adapter::~Adapter() = default;

size_t Adapter::enumerateFeatures(WGPUFeatureName*)
{
    // We support no optional features right now.
    return 0;
}

bool Adapter::getLimits(WGPUSupportedLimits& limits)
{
    if (limits.nextInChain != nullptr)
        return false;

    // FIXME: Implement this.
    limits.limits = { };
    return true;
}

void Adapter::getProperties(WGPUAdapterProperties& properties)
{
    // FIXME: What should the vendorID and deviceID be?
    properties.vendorID = 0;
    properties.deviceID = 0;
    properties.name = m_device.name.UTF8String;
    properties.driverDescription = "";
    properties.adapterType = m_device.hasUnifiedMemory ? WGPUAdapterType_IntegratedGPU : WGPUAdapterType_DiscreteGPU;
    properties.backendType = WGPUBackendType_Metal;
}

bool Adapter::hasFeature(WGPUFeatureName)
{
    // We support no optional features right now.
    return false;
}

static bool deviceMeetsRequiredLimits(id<MTLDevice>, const WGPURequiredLimits& requiredLimits)
{
    // FIXME: Implement this.
    return !requiredLimits.nextInChain
        && !requiredLimits.limits.maxTextureDimension1D
        && !requiredLimits.limits.maxTextureDimension2D
        && !requiredLimits.limits.maxTextureDimension3D
        && !requiredLimits.limits.maxTextureArrayLayers
        && !requiredLimits.limits.maxBindGroups
        && !requiredLimits.limits.maxDynamicUniformBuffersPerPipelineLayout
        && !requiredLimits.limits.maxDynamicStorageBuffersPerPipelineLayout
        && !requiredLimits.limits.maxSampledTexturesPerShaderStage
        && !requiredLimits.limits.maxSamplersPerShaderStage
        && !requiredLimits.limits.maxStorageBuffersPerShaderStage
        && !requiredLimits.limits.maxStorageTexturesPerShaderStage
        && !requiredLimits.limits.maxUniformBuffersPerShaderStage
        && !requiredLimits.limits.maxUniformBufferBindingSize
        && !requiredLimits.limits.maxStorageBufferBindingSize
        && !requiredLimits.limits.minUniformBufferOffsetAlignment
        && !requiredLimits.limits.minStorageBufferOffsetAlignment
        && !requiredLimits.limits.maxVertexBuffers
        && !requiredLimits.limits.maxVertexAttributes
        && !requiredLimits.limits.maxVertexBufferArrayStride
        && !requiredLimits.limits.maxInterStageShaderComponents
        && !requiredLimits.limits.maxComputeWorkgroupStorageSize
        && !requiredLimits.limits.maxComputeInvocationsPerWorkgroup
        && !requiredLimits.limits.maxComputeWorkgroupSizeX
        && !requiredLimits.limits.maxComputeWorkgroupSizeY
        && !requiredLimits.limits.maxComputeWorkgroupSizeZ
        && !requiredLimits.limits.maxComputeWorkgroupsPerDimension;
}

void Adapter::requestDevice(const WGPUDeviceDescriptor& descriptor, CompletionHandler<void(WGPURequestDeviceStatus, RefPtr<Device>&&, String&&)>&& callback)
{
    if (descriptor.nextInChain) {
        callback(WGPURequestDeviceStatus_Error, nullptr, "Unknown descriptor type"_s);
        return;
    }

    if (descriptor.requiredFeaturesCount) {
        callback(WGPURequestDeviceStatus_Error, nullptr, "Device does not support requested features"_s);
        return;
    }

    if (descriptor.requiredLimits && !deviceMeetsRequiredLimits(m_device, *descriptor.requiredLimits)) {
        callback(WGPURequestDeviceStatus_Error, nullptr, "Device does not support requested limits"_s);
        return;
    }

    callback(WGPURequestDeviceStatus_Success, Device::create(m_device, descriptor.label, m_instance), { });
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuAdapterRelease(WGPUAdapter adapter)
{
    delete adapter;
}

size_t wgpuAdapterEnumerateFeatures(WGPUAdapter adapter, WGPUFeatureName* features)
{
    return WebGPU::fromAPI(adapter).enumerateFeatures(features);
}

bool wgpuAdapterGetLimits(WGPUAdapter adapter, WGPUSupportedLimits* limits)
{
    return WebGPU::fromAPI(adapter).getLimits(*limits);
}

void wgpuAdapterGetProperties(WGPUAdapter adapter, WGPUAdapterProperties* properties)
{
    WebGPU::fromAPI(adapter).getProperties(*properties);
}

bool wgpuAdapterHasFeature(WGPUAdapter adapter, WGPUFeatureName feature)
{
    return WebGPU::fromAPI(adapter).hasFeature(feature);
}

void wgpuAdapterRequestDevice(WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor, WGPURequestDeviceCallback callback, void* userdata)
{
    WebGPU::fromAPI(adapter).requestDevice(*descriptor, [callback, userdata](WGPURequestDeviceStatus status, RefPtr<WebGPU::Device>&& device, String&& message) {
        if (device) {
            auto& queue = device->getQueue();
            callback(status, new WGPUDeviceImpl { device.releaseNonNull(), { queue } }, message.utf8().data(), userdata);
        } else
            callback(status, nullptr, message.utf8().data(), userdata);
    });
}

void wgpuAdapterRequestDeviceWithBlock(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor, WGPURequestDeviceBlockCallback callback)
{
    WebGPU::fromAPI(adapter).requestDevice(*descriptor, [callback = WTFMove(callback)](WGPURequestDeviceStatus status, RefPtr<WebGPU::Device>&& device, String&& message) {
        if (device) {
            auto& queue = device->getQueue();
            callback(status, new WGPUDeviceImpl { device.releaseNonNull(), { queue } }, message.utf8().data());
        } else
            callback(status, nullptr, message.utf8().data());
    });
}
