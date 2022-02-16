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
#import "Adapter.h"

#import "Device.h"
#import "WebGPUExt.h"
#import <wtf/StdLibExtras.h>

namespace WebGPU {

Adapter::Adapter() = default;

Adapter::~Adapter() = default;

size_t Adapter::enumerateFeatures(WGPUFeatureName* features)
{
    UNUSED_PARAM(features);
    return 0;
}

bool Adapter::getLimits(WGPUSupportedLimits* limits)
{
    UNUSED_PARAM(limits);
    return true;
}

void Adapter::getProperties(WGPUAdapterProperties* properties)
{
    UNUSED_PARAM(properties);
}

bool Adapter::hasFeature(WGPUFeatureName feature)
{
    UNUSED_PARAM(feature);
    return false;
}

void Adapter::requestDevice(const WGPUDeviceDescriptor* descriptor, WTF::Function<void(WGPURequestDeviceStatus, Ref<Device>&&, const char*)>&& callback)
{
    UNUSED_PARAM(descriptor);
    UNUSED_PARAM(callback);
}

} // namespace WebGPU

void wgpuAdapterRelease(WGPUAdapter adapter)
{
    delete adapter;
}

size_t wgpuAdapterEnumerateFeatures(WGPUAdapter adapter, WGPUFeatureName* features)
{
    return adapter->adapter->enumerateFeatures(features);
}

bool wgpuAdapterGetLimits(WGPUAdapter adapter, WGPUSupportedLimits* limits)
{
    return adapter->adapter->getLimits(limits);
}

void wgpuAdapterGetProperties(WGPUAdapter adapter, WGPUAdapterProperties* properties)
{
    adapter->adapter->getProperties(properties);
}

bool wgpuAdapterHasFeature(WGPUAdapter adapter, WGPUFeatureName feature)
{
    return adapter->adapter->hasFeature(feature);
}

void wgpuAdapterRequestDevice(WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor, WGPURequestDeviceCallback callback, void* userdata)
{
    adapter->adapter->requestDevice(descriptor, [callback, userdata] (WGPURequestDeviceStatus status, Ref<WebGPU::Device>&& device, const char* message) {
        callback(status, new WGPUDeviceImpl { WTFMove(device) }, message, userdata);
    });
}
