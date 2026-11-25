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

#import "CAPIConversions.h"
#import "CAPIFutures.h"
#import "WebGPUCppConversions.h"
#import <wtf/text/MakeString.h>

// The caller owns the reference to the instance. Null when the instance is gone. The wgpuInstance*
// functions use the instance as a Metal instance, which it is: only Metal objects reach the C API.
WGPUInstance wgpuAdapterGetInstance(WGPUAdapter adapter)
{
    @autoreleasepool {
        return WebGPU::CAPI::releaseToAPI(protect(WebGPU::CAPI::fromAPI(adapter))->instance());
    }
}

NODELETE void wgpuAdapterAddRef(WGPUAdapter adapter)
{
    WebGPU::CAPI::fromAPI(adapter).ref();
}

void wgpuAdapterRelease(WGPUAdapter adapter)
{
    @autoreleasepool {
        WebGPU::CAPI::fromAPI(adapter).deref();
    }
}

// The chained structs of limits are not filled in.
WGPUStatus wgpuAdapterGetLimits(WGPUAdapter adapter, WGPULimits* limits)
{
    @autoreleasepool {
        auto* nextInChain = limits->nextInChain;
        *limits = WebGPU::CAPI::toAPI(protect(WebGPU::CAPI::fromAPI(adapter))->limits());
        limits->nextInChain = nextInChain;
        return WGPUStatus_Success;
    }
}

// The strings of info are freed with wgpuAdapterInfoFreeMembers().
WGPUStatus wgpuAdapterGetInfo(WGPUAdapter adapter, WGPUAdapterInfo* info)
{
    @autoreleasepool {
        auto apiInfo = protect(WebGPU::CAPI::fromAPI(adapter))->info();
        // FIXME: What should the vendorID and deviceID be?
        info->vendor = WebGPU::CAPI::toAPIAllocated(emptyString());
        info->architecture = WebGPU::CAPI::toAPIAllocated(emptyString());
        info->device = WebGPU::CAPI::toAPIAllocated(apiInfo.name);
        info->description = WebGPU::CAPI::toAPIAllocated(emptyString());
        info->backendType = WGPUBackendType_Metal;
        info->adapterType = apiInfo.hasUnifiedMemory ? WGPUAdapterType_IntegratedGPU : WGPUAdapterType_DiscreteGPU;
        info->vendorID = 0;
        info->deviceID = 0;
        info->subgroupMinSize = apiInfo.subgroupMinSize;
        info->subgroupMaxSize = apiInfo.subgroupMaxSize;
        return WGPUStatus_Success;
    }
}

void wgpuAdapterInfoFreeMembers(WGPUAdapterInfo info)
{
    @autoreleasepool {
        WebGPU::CAPI::freeAllocated(info.vendor);
        WebGPU::CAPI::freeAllocated(info.architecture);
        WebGPU::CAPI::freeAllocated(info.device);
        WebGPU::CAPI::freeAllocated(info.description);
    }
}

WGPUBool wgpuAdapterHasFeature(WGPUAdapter adapter, WGPUFeatureName feature)
{
    @autoreleasepool {
        auto apiFeature = WebGPU::CAPI::fromAPI(feature);
        return apiFeature && protect(WebGPU::CAPI::fromAPI(adapter))->features().contains(*apiFeature);
    }
}

// Calls the uncaptured error callback of the C API with each error of the device that no error scope
// catches, until the device is lost. As in webgpu.h, the callback has no callback mode: it is called
// in the call that generates the error, on its thread.
static void listenForUncapturedErrors(WebGPU::Device& device, const WGPUUncapturedErrorCallbackInfo& callbackInfo)
{
    device.setUncapturedErrorCallback([weakDevice = ThreadSafeWeakPtr { device }, callbackInfo](WebGPU::Error&& error) {
        RefPtr device = weakDevice;
        if (!device || device->isLost())
            return;
        auto message = error.message.utf8();
        auto type = WebGPU::CAPI::toAPI(std::optional<WebGPU::Error> { WTF::move(error) });
        WGPUDevice handle = WebGPU::CAPI::toAPI(*device);
        callbackInfo.callback(&handle, type, WebGPU::CAPI::toAPI(message), callbackInfo.userdata1, callbackInfo.userdata2);
    });
}

// The features for which wgpuAdapterHasFeature() is true, freed with
// wgpuSupportedFeaturesFreeMembers().
void wgpuAdapterGetFeatures(WGPUAdapter adapter, WGPUSupportedFeatures* features)
{
    @autoreleasepool {
        *features = WebGPU::CAPI::toAPIAllocated(protect(WebGPU::CAPI::fromAPI(adapter))->features());
    }
}

void wgpuSupportedFeaturesFreeMembers(WGPUSupportedFeatures features)
{
    @autoreleasepool {
        WebGPU::CAPI::freeAllocated(features);
    }
}

// The C API reports a device that could not be created with WGPURequestDeviceStatus_Error and no
// device. A null descriptor is the default one.
WGPUFuture wgpuAdapterRequestDevice(WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor, WGPURequestDeviceCallbackInfo callbackInfo)
{
    @autoreleasepool {
        Ref protectedAdapter = WebGPU::CAPI::fromAPI(adapter);
        WebGPU::CAPI::Future future { protectedAdapter->instance() };
        auto callback = [callbackInfo, future](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message) {
            callbackInfo.callback(status, device, message, callbackInfo.userdata1, callbackInfo.userdata2);
            future.complete();
        };
        WGPUDeviceDescriptor defaultDescriptor = WGPU_DEVICE_DESCRIPTOR_INIT;
        if (!descriptor)
            descriptor = &defaultDescriptor;
        WebGPU::CAPI::DeviceDescriptorStorage storage;
        auto apiDescriptor = WebGPU::CAPI::fromAPI(*descriptor, storage);
        if (!apiDescriptor) {
            callback(WGPURequestDeviceStatus_Error, nullptr, WebGPU::CAPI::toAPI("Device does not support requested features"_s));
            return future.future();
        }
        protectedAdapter->requestDevice(*apiDescriptor, [callback = WTF::move(callback), uncapturedErrorCallbackInfo = descriptor->uncapturedErrorCallbackInfo](std::expected<Ref<WebGPU::Device>, String>&& device) {
            if (!device) {
                auto message = device.error().utf8();
                return callback(WGPURequestDeviceStatus_Error, nullptr, WebGPU::CAPI::toAPI(message));
            }
            if (uncapturedErrorCallbackInfo.callback)
                listenForUncapturedErrors(device->get(), uncapturedErrorCallbackInfo);
            callback(WGPURequestDeviceStatus_Success, WebGPU::CAPI::releaseToAPI(WTF::move(*device)), WebGPU::CAPI::toAPI(""_s));
        });
        return future.future();
    }
}
