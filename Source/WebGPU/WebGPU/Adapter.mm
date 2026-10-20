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

#import "config.h"
#import "Adapter.h"

#import "APIConversions.h"
#import "Device.h"
#import "Instance.h"
#import <algorithm>
#import <ranges>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebGPU::Metal {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Adapter);

Adapter::Adapter(id<MTLDevice> device, Instance& instance, bool xrCompatible, HardwareCapabilities&& capabilities)
    : m_device(device)
    , m_instance(&instance)
    , m_capabilities(WTF::move(capabilities))
    , m_xrCompatible(xrCompatible)
{
}

Adapter::Adapter(Instance& instance)
    : m_instance(&instance)
{
}

Adapter::~Adapter() = default;

Vector<WebGPU::FeatureName> Adapter::features() const
{
    return m_capabilities.features;
}


static uint32_t subgroupSize(id<MTLDevice> device)
{
    // Apple Silicon GPUs have a fixed SIMD-group width of 32,
    // so there's no need to compile a pipeline just to discover it.
    if ([device supportsFamily:MTLGPUFamilyApple4])
        return 32;

    // On non-Apple (Intel/AMD) GPUs the SIMD-group width isn't a fixed device
    // constant, so query it from a compute pipeline state's threadExecutionWidth.
    // Fall back to 32 if the probe pipeline can't be built for any reason.
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:@"#include <metal_stdlib>\nusing namespace metal;\nkernel void _webgpu_subgroup_size_probe() { }" options:nil error:&error];
    if (!library)
        return 32;
    id<MTLFunction> function = [library newFunctionWithName:@"_webgpu_subgroup_size_probe"];
    if (!function)
        return 32;
    id<MTLComputePipelineState> pipelineState = [device newComputePipelineStateWithFunction:function error:&error];
    if (!pipelineState)
        return 32;
    return static_cast<uint32_t>(pipelineState.threadExecutionWidth);
}

WebGPU::AdapterInfo Adapter::info()
{
    WebGPU::AdapterInfo info {
        .name = m_device.name,
        // A Metal device is never a fallback (software) adapter.
        .isFallbackAdapter = false,
    };
    if (hasFeature(WebGPU::FeatureName::Subgroups)) {
        // Metal exposes a single SIMD-group (subgroup) width per device, so
        // min and max are equal. It's a fixed 32 on Apple Silicon; on other
        // GPUs it's derived from a compute pipeline's threadExecutionWidth.
        uint32_t size = subgroupSize(m_device);
        info.subgroupMinSize = size;
        info.subgroupMaxSize = size;
    } else {
        // Spec defaults when the feature is unsupported: https://github.com/gpuweb/gpuweb/pull/4963
        info.subgroupMinSize = 4;
        info.subgroupMaxSize = 128;
    }
    return info;
}

bool Adapter::hasFeature(WebGPU::FeatureName feature)
{
    return m_capabilities.features.contains(feature);
}

void Adapter::requestDevice(const WebGPU::DeviceDescriptor& descriptor, CompletionHandler<void(RefPtr<WebGPU::Device>&&)>&& callback)
{
    requestDeviceOrError(descriptor, [callback = WTF::move(callback)](std::expected<Ref<Device>, String>&& device) mutable {
        callback(device ? RefPtr<WebGPU::Device> { WTF::move(*device) } : nullptr);
    });
}

void Adapter::requestDeviceOrError(const WebGPU::DeviceDescriptor& descriptor, CompletionHandler<void(std::expected<Ref<Device>, String>&&)>&& callback)
{
    if (m_deviceRequested) {
        callback(makeUnexpected("Adapter can only request one device"_s));
        makeInvalid();
        return;
    }

    Limits limits { };

    if (descriptor.requiredLimits) {
        limits = *descriptor.requiredLimits;

        if (!WebGPU::Metal::isValid(limits)) {
            callback(makeUnexpected("Device does not support requested limits"_s));
            return;
        }

        if (anyLimitIsBetterThan(limits, m_capabilities.limits)) {
            callback(makeUnexpected("Device does not support requested limits"_s));
            return;
        }
    } else
        limits = defaultLimits();

    Vector<WebGPU::FeatureName> features { descriptor.requiredFeatures };
    if (includesUnsupportedFeatures(features, m_capabilities.features)) {
        callback(makeUnexpected("Device does not support requested features"_s));
        return;
    }

    HardwareCapabilities capabilities {
        limits,
        WTF::move(features),
        m_capabilities.baseCapabilities,
    };

    auto label = descriptor.label;
    m_deviceRequested = true;
    // FIXME: this should be asynchronous - https://bugs.webkit.org/show_bug.cgi?id=233621
    callback(Device::create(this->m_device, WTF::move(label), WTF::move(capabilities), *this));
}

bool Adapter::isXRCompatible() const
{
    return m_xrCompatible;
}

} // namespace WebGPU::Metal

#pragma mark WGPU Stubs

void LEGACY_NODELETE wgpuAdapterAddRef(WGPUAdapter adapter)
{
    WebGPU::Metal::fromAPI(adapter).ref();
}

void wgpuAdapterRelease(WGPUAdapter adapter)
{
    WebGPU::Metal::fromAPI(adapter).deref();
}

WGPUBool wgpuAdapterGetLimits(WGPUAdapter adapter, WGPUSupportedLimits* limits)
{
    limits->limits = WebGPU::Metal::toAPI(protect(WebGPU::Metal::fromAPI(adapter))->limits());
    return true;
}

void wgpuAdapterGetInfo(WGPUAdapter adapter, WGPUAdapterInfo* info)
{
    Ref protectedAdapter = WebGPU::Metal::fromAPI(adapter);
    auto apiInfo = protectedAdapter->info();
    id<MTLDevice> device = protectedAdapter->device();
    // FIXME: What should the vendorID and deviceID be?
    info->vendorID = 0;
    info->deviceID = 0;
    info->name = device.name.UTF8String;
    info->driverDescription = "";
    info->adapterType = device.hasUnifiedMemory ? WGPUAdapterType_IntegratedGPU : WGPUAdapterType_DiscreteGPU;
    info->backendType = WGPUBackendType_Metal;
    info->subgroupMinSize = apiInfo.subgroupMinSize;
    info->subgroupMaxSize = apiInfo.subgroupMaxSize;
}

WGPUBool wgpuAdapterHasFeature(WGPUAdapter adapter, WGPUFeatureName feature)
{
    auto apiFeature = WebGPU::Metal::fromAPI(feature);
    return apiFeature && protect(WebGPU::Metal::fromAPI(adapter))->hasFeature(*apiFeature);
}

// The C API reports a device that could not be created with WGPURequestDeviceStatus_Error and no device.
static void requestDevice(WGPUAdapter adapter, const WGPUDeviceDescriptor& descriptor, Function<void(WGPURequestDeviceStatus, WGPUDevice, const char*)>&& callback)
{
    Ref protectedAdapter = WebGPU::Metal::fromAPI(adapter);
    WebGPU::Metal::DeviceDescriptorStorage storage;
    auto apiDescriptor = WebGPU::Metal::fromAPI(descriptor, storage);
    if (!apiDescriptor)
        return callback(WGPURequestDeviceStatus_Error, nullptr, "Device does not support requested features");
    protectedAdapter->requestDeviceOrError(*apiDescriptor, [callback = WTF::move(callback)](std::expected<Ref<WebGPU::Metal::Device>, String>&& device) {
        if (!device)
            return callback(WGPURequestDeviceStatus_Error, nullptr, device.error().utf8().legacyCStringPointer());
        callback(WGPURequestDeviceStatus_Success, WebGPU::Metal::releaseToAPI(WTF::move(*device)), "");
    });
}

void wgpuAdapterRequestDevice(WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor, WGPURequestDeviceCallback callback, void* userdata)
{
    requestDevice(adapter, *descriptor, [callback, userdata](WGPURequestDeviceStatus status, WGPUDevice device, const char* message) {
        callback(status, device, message, userdata);
    });
}
