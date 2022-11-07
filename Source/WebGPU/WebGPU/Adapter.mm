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
#import <algorithm>
#import <wtf/StdLibExtras.h>

namespace WebGPU {

Adapter::Adapter(id<MTLDevice> device, Instance& instance, HardwareCapabilities&& capabilities)
    : m_device(device)
    , m_instance(instance)
    , m_capabilities(WTFMove(capabilities))
{
}

Adapter::Adapter(Instance& instance)
    : m_instance(instance)
{
}

Adapter::~Adapter() = default;

size_t Adapter::enumerateFeatures(WGPUFeatureName* features)
{
    // The API contract for this requires that sufficient space has already been allocated for the output.
    // This requires the caller calling us twice: once to get the amount of space to allocate, and once to fill the space.
    if (features)
        std::copy(m_capabilities.features.begin(), m_capabilities.features.end(), features);
    return m_capabilities.features.size();
}

bool Adapter::getLimits(WGPUSupportedLimits& limits)
{
    if (limits.nextInChain != nullptr)
        return false;

    limits.limits = m_capabilities.limits;
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

bool Adapter::hasFeature(WGPUFeatureName feature)
{
    return std::find(m_capabilities.features.begin(), m_capabilities.features.end(), feature);
}

void Adapter::requestDevice(const WGPUDeviceDescriptor& descriptor, CompletionHandler<void(WGPURequestDeviceStatus, Ref<Device>&&, String&&)>&& callback)
{
    if (descriptor.nextInChain) {
        instance().scheduleWork([strongThis = Ref { *this }, callback = WTFMove(callback)]() mutable {
            callback(WGPURequestDeviceStatus_Error, Device::createInvalid(strongThis), "Unknown descriptor type"_s);
        });
        return;
    }

    WGPULimits limits { };

    if (descriptor.requiredLimits) {
        if (descriptor.requiredLimits->nextInChain) {
            instance().scheduleWork([strongThis = Ref { *this }, callback = WTFMove(callback)]() mutable {
                callback(WGPURequestDeviceStatus_Error, Device::createInvalid(strongThis), "Unknown descriptor type"_s);
            });
            return;
        }

        if (!WebGPU::isValid(descriptor.requiredLimits->limits)) {
            instance().scheduleWork([strongThis = Ref { *this }, callback = WTFMove(callback)]() mutable {
                callback(WGPURequestDeviceStatus_Error, Device::createInvalid(strongThis), "Device does not support requested limits"_s);
            });
            return;
        }

        if (anyLimitIsBetterThan(descriptor.requiredLimits->limits, m_capabilities.limits)) {
            instance().scheduleWork([strongThis = Ref { *this }, callback = WTFMove(callback)]() mutable {
                callback(WGPURequestDeviceStatus_Error, Device::createInvalid(strongThis), "Device does not support requested limits"_s);
            });
            return;
        }

        limits = descriptor.requiredLimits->limits;
    } else
        limits = defaultLimits();

    auto features = Vector { descriptor.requiredFeatures, descriptor.requiredFeaturesCount };
    if (includesUnsupportedFeatures(features, m_capabilities.features)) {
        instance().scheduleWork([strongThis = Ref { *this }, callback = WTFMove(callback)]() mutable {
            callback(WGPURequestDeviceStatus_Error, Device::createInvalid(strongThis), "Device does not support requested features"_s);
        });
        return;
    }

    HardwareCapabilities capabilities {
        limits,
        WTFMove(features),
        m_capabilities.baseCapabilities,
    };

    auto label = fromAPI(descriptor.label);
    // FIXME: this should be asynchronous
    callback(WGPURequestDeviceStatus_Success, Device::create(this->m_device, WTFMove(label), WTFMove(capabilities), *this), { });
}

void Adapter::requestInvalidDevice(CompletionHandler<void(Ref<Device>&&)>&& callback)
{
    instance().scheduleWork([strongThis = Ref { *this }, callback = WTFMove(callback)]() mutable {
        callback(Device::createInvalid(strongThis));
    });
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuAdapterRelease(WGPUAdapter adapter)
{
    WebGPU::fromAPI(adapter).deref();
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
    WebGPU::fromAPI(adapter).requestDevice(*descriptor, [callback, userdata](WGPURequestDeviceStatus status, Ref<WebGPU::Device>&& device, String&& message) {
        callback(status, WebGPU::releaseToAPI(WTFMove(device)), message.utf8().data(), userdata);
    });
}

void wgpuAdapterRequestDeviceWithBlock(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor, WGPURequestDeviceBlockCallback callback)
{
    WebGPU::fromAPI(adapter).requestDevice(*descriptor, [callback = WTFMove(callback)](WGPURequestDeviceStatus status, Ref<WebGPU::Device>&& device, String&& message) {
        callback(status, WebGPU::releaseToAPI(WTFMove(device)), message.utf8().data());
    });
}

void wgpuAdapterRequestInvalidDeviceWithBlock(WGPUAdapter adapter, WGPURequestInvalidDeviceBlockCallback callback)
{
    WebGPU::fromAPI(adapter).requestInvalidDevice([callback = WTFMove(callback)](Ref<WebGPU::Device>&& device) {
        callback(WebGPU::releaseToAPI(WTFMove(device)));
    });
}
