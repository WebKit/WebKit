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

static constexpr auto invalidComputePipelineDescriptorMessage = "GPUComputePipelineDescriptor has no shader module"_s;
static constexpr auto invalidRenderPipelineDescriptorMessage = "GPURenderPipelineDescriptor has no shader module or an invalid enum value or color write mask bit"_s;

NODELETE void wgpuDeviceAddRef(WGPUDevice device)
{
    WebGPU::CAPI::fromAPI(device).ref();
}

void wgpuDeviceRelease(WGPUDevice device)
{
    @autoreleasepool {
        WebGPU::CAPI::fromAPI(device).deref();
    }
}

WGPUBindGroup wgpuDeviceCreateBindGroup(WGPUDevice device, const WGPUBindGroupDescriptor* descriptor)
{
    @autoreleasepool {
        Ref protectedDevice = WebGPU::CAPI::fromAPI(device);
        WebGPU::CAPI::BindGroupDescriptorStorage storage;
        auto apiDescriptor = WebGPU::CAPI::fromAPI(*descriptor, storage);
        if (!apiDescriptor) {
            protectedDevice->generateAValidationError("GPUBindGroupDescriptor has no layout or an entry without exactly one resource"_s);
            return WebGPU::CAPI::releaseToAPI(protectedDevice->createInvalidBindGroup());
        }
        return WebGPU::CAPI::releaseToAPI(protectedDevice->createBindGroup(*apiDescriptor));
    }
}

WGPUBindGroupLayout wgpuDeviceCreateBindGroupLayout(WGPUDevice device, const WGPUBindGroupLayoutDescriptor* descriptor)
{
    @autoreleasepool {
        Ref protectedDevice = WebGPU::CAPI::fromAPI(device);
        WebGPU::CAPI::BindGroupLayoutDescriptorStorage storage;
        auto apiDescriptor = WebGPU::CAPI::fromAPI(*descriptor, storage);
        if (!apiDescriptor) {
            protectedDevice->generateAValidationError("GPUBindGroupLayoutDescriptor has an invalid enum value or visibility bit"_s);
            return WebGPU::CAPI::releaseToAPI(protectedDevice->createInvalidBindGroupLayout());
        }
        return WebGPU::CAPI::releaseToAPI(protectedDevice->createBindGroupLayout(*apiDescriptor));
    }
}

WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice device, const WGPUBufferDescriptor* descriptor)
{
    @autoreleasepool {
        Ref protectedDevice = WebGPU::CAPI::fromAPI(device);
        auto apiDescriptor = WebGPU::CAPI::fromAPI(*descriptor);
        if (!apiDescriptor) {
            protectedDevice->generateAValidationError("GPUBufferDescriptor.usage has unknown bits"_s);
            return WebGPU::CAPI::releaseToAPI(protectedDevice->createInvalidBuffer());
        }
        return WebGPU::CAPI::releaseToAPI(protectedDevice->createBuffer(*apiDescriptor));
    }
}

WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice device, const WGPUCommandEncoderDescriptor* descriptor)
{
    @autoreleasepool {
        return WebGPU::CAPI::releaseToAPI(protect(WebGPU::CAPI::fromAPI(device))->createCommandEncoder({ .label = descriptor ? WebGPU::CAPI::fromAPI(descriptor->label) : String() }));
    }
}

WGPUComputePipeline wgpuDeviceCreateComputePipeline(WGPUDevice device, const WGPUComputePipelineDescriptor* descriptor)
{
    @autoreleasepool {
        Ref protectedDevice = WebGPU::CAPI::fromAPI(device);
        WebGPU::CAPI::ComputePipelineDescriptorStorage storage;
        auto apiDescriptor = WebGPU::CAPI::fromAPI(*descriptor, storage);
        if (!apiDescriptor) {
            protectedDevice->generateAValidationError(invalidComputePipelineDescriptorMessage);
            return WebGPU::CAPI::releaseToAPI(protectedDevice->createInvalidComputePipeline());
        }
        return WebGPU::CAPI::releaseToAPI(protectedDevice->createComputePipeline(*apiDescriptor));
    }
}

// The C API reports a failed asynchronous pipeline creation with a status and no pipeline.
template<typename T>
static auto createPipelineAsyncCompletion(Function<void(WGPUCreatePipelineAsyncStatus, T*, String&&)>&& callback)
{
    return [callback = WTF::move(callback)](auto&& pipeline) mutable {
        if (!pipeline)
            return callback(WGPUCreatePipelineAsyncStatus_ValidationError, nullptr, WTF::move(pipeline.error().message));
        callback(WGPUCreatePipelineAsyncStatus_Success, WebGPU::CAPI::releaseToAPI(WTF::move(*pipeline)), { });
    };
}

// Reports a descriptor that did not convert, for the asynchronous pipeline creations.
static void reportInvalidPipelineDescriptor(WebGPU::Device& device, ASCIILiteral message, auto&& callback)
{
    if (RefPtr instance = device.instance()) {
        WebGPU::CAPI::EventManager::ensure(*instance)->scheduleWork([callback = WTF::move(callback), message] mutable {
            callback(WGPUCreatePipelineAsyncStatus_ValidationError, nullptr, message);
        });
        return;
    }
    callback(WGPUCreatePipelineAsyncStatus_ValidationError, nullptr, message);
}

static void createComputePipelineAsync(WGPUDevice device, const WGPUComputePipelineDescriptor& descriptor, Function<void(WGPUCreatePipelineAsyncStatus, WGPUComputePipeline, String&&)>&& callback)
{
    Ref protectedDevice = WebGPU::CAPI::fromAPI(device);
    WebGPU::CAPI::ComputePipelineDescriptorStorage storage;
    auto apiDescriptor = WebGPU::CAPI::fromAPI(descriptor, storage);
    if (!apiDescriptor)
        return reportInvalidPipelineDescriptor(protectedDevice, invalidComputePipelineDescriptorMessage, WTF::move(callback));
    protectedDevice->createComputePipelineAsync(*apiDescriptor, createPipelineAsyncCompletion(WTF::move(callback)));
}

WGPUFuture wgpuDeviceCreateComputePipelineAsync(WGPUDevice device, const WGPUComputePipelineDescriptor* descriptor, WGPUCreateComputePipelineAsyncCallbackInfo callbackInfo)
{
    @autoreleasepool {
        WebGPU::CAPI::Future future { protect(WebGPU::CAPI::fromAPI(device))->instance() };
        createComputePipelineAsync(device, *descriptor, [callbackInfo, future](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline pipeline, String&& message) {
            auto utf8 = message.utf8();
            callbackInfo.callback(status, pipeline, WebGPU::CAPI::toAPI(utf8), callbackInfo.userdata1, callbackInfo.userdata2);
            future.complete();
        });
        return future.future();
    }
}

WGPUPipelineLayout wgpuDeviceCreatePipelineLayout(WGPUDevice device, const WGPUPipelineLayoutDescriptor* descriptor)
{
    @autoreleasepool {
        Ref protectedDevice = WebGPU::CAPI::fromAPI(device);
        WebGPU::CAPI::PipelineLayoutDescriptorStorage storage;
        auto apiDescriptor = WebGPU::CAPI::fromAPI(*descriptor, storage);
        if (!apiDescriptor) {
            protectedDevice->generateAValidationError("GPUPipelineLayoutDescriptor has a null bind group layout"_s);
            return WebGPU::CAPI::releaseToAPI(protectedDevice->createInvalidPipelineLayout());
        }
        return WebGPU::CAPI::releaseToAPI(protectedDevice->createPipelineLayout(*apiDescriptor));
    }
}

WGPUQuerySet wgpuDeviceCreateQuerySet(WGPUDevice device, const WGPUQuerySetDescriptor* descriptor)
{
    @autoreleasepool {
        Ref protectedDevice = WebGPU::CAPI::fromAPI(device);
        auto apiDescriptor = WebGPU::CAPI::fromAPI(*descriptor);
        // An unknown query type makes an invalid query set without a validation error.
        if (!apiDescriptor)
            return WebGPU::CAPI::releaseToAPI(protectedDevice->createInvalidQuerySet());
        return WebGPU::CAPI::releaseToAPI(protectedDevice->createQuerySet(*apiDescriptor));
    }
}

WGPURenderBundleEncoder wgpuDeviceCreateRenderBundleEncoder(WGPUDevice device, const WGPURenderBundleEncoderDescriptor* descriptor)
{
    @autoreleasepool {
        Ref protectedDevice = WebGPU::CAPI::fromAPI(device);
        WebGPU::CAPI::RenderBundleEncoderDescriptorStorage storage;
        auto apiDescriptor = WebGPU::CAPI::fromAPI(*descriptor, storage);
        if (!apiDescriptor) {
            constexpr auto error = "GPURenderBundleEncoderDescriptor has an invalid texture format"_s;
            protectedDevice->generateAValidationError(error);
            return WebGPU::CAPI::releaseToAPI(protectedDevice->createInvalidRenderBundleEncoder(error));
        }
        return WebGPU::CAPI::releaseToAPI(protectedDevice->createRenderBundleEncoder(*apiDescriptor));
    }
}

WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice device, const WGPURenderPipelineDescriptor* descriptor)
{
    @autoreleasepool {
        Ref protectedDevice = WebGPU::CAPI::fromAPI(device);
        WebGPU::CAPI::RenderPipelineDescriptorStorage storage;
        auto apiDescriptor = WebGPU::CAPI::fromAPI(*descriptor, storage);
        if (!apiDescriptor) {
            protectedDevice->generateAValidationError(invalidRenderPipelineDescriptorMessage);
            return WebGPU::CAPI::releaseToAPI(protectedDevice->createInvalidRenderPipeline());
        }
        return WebGPU::CAPI::releaseToAPI(protectedDevice->createRenderPipeline(*apiDescriptor));
    }
}

static void createRenderPipelineAsync(WGPUDevice device, const WGPURenderPipelineDescriptor& descriptor, Function<void(WGPUCreatePipelineAsyncStatus, WGPURenderPipeline, String&&)>&& callback)
{
    Ref protectedDevice = WebGPU::CAPI::fromAPI(device);
    WebGPU::CAPI::RenderPipelineDescriptorStorage storage;
    auto apiDescriptor = WebGPU::CAPI::fromAPI(descriptor, storage);
    if (!apiDescriptor)
        return reportInvalidPipelineDescriptor(protectedDevice, invalidRenderPipelineDescriptorMessage, WTF::move(callback));
    protectedDevice->createRenderPipelineAsync(*apiDescriptor, createPipelineAsyncCompletion(WTF::move(callback)));
}

WGPUFuture wgpuDeviceCreateRenderPipelineAsync(WGPUDevice device, const WGPURenderPipelineDescriptor* descriptor, WGPUCreateRenderPipelineAsyncCallbackInfo callbackInfo)
{
    @autoreleasepool {
        WebGPU::CAPI::Future future { protect(WebGPU::CAPI::fromAPI(device))->instance() };
        createRenderPipelineAsync(device, *descriptor, [callbackInfo, future](WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline pipeline, String&& message) {
            auto utf8 = message.utf8();
            callbackInfo.callback(status, pipeline, WebGPU::CAPI::toAPI(utf8), callbackInfo.userdata1, callbackInfo.userdata2);
            future.complete();
        });
        return future.future();
    }
}

WGPUSampler wgpuDeviceCreateSampler(WGPUDevice device, const WGPUSamplerDescriptor* descriptor)
{
    @autoreleasepool {
        Ref protectedDevice = WebGPU::CAPI::fromAPI(device);
        // A null descriptor is the default one.
        WGPUSamplerDescriptor defaultDescriptor = WGPU_SAMPLER_DESCRIPTOR_INIT;
        auto apiDescriptor = WebGPU::CAPI::fromAPI(descriptor ? *descriptor : defaultDescriptor);
        if (!apiDescriptor) {
            protectedDevice->generateAValidationError("GPUSamplerDescriptor has an invalid enum value"_s);
            return WebGPU::CAPI::releaseToAPI(protectedDevice->createInvalidSampler());
        }
        return WebGPU::CAPI::releaseToAPI(protectedDevice->createSampler(*apiDescriptor));
    }
}

WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice device, const WGPUShaderModuleDescriptor* descriptor)
{
    @autoreleasepool {
        Ref protectedDevice = WebGPU::CAPI::fromAPI(device);
        WebGPU::CAPI::ShaderModuleDescriptorStorage storage;
        auto apiDescriptor = WebGPU::CAPI::fromAPI(*descriptor, storage);
        if (!apiDescriptor) {
            protectedDevice->generateAValidationError("GPUShaderModuleDescriptor has no WGSL code or a null pipeline layout hint"_s);
            return WebGPU::CAPI::releaseToAPI(protectedDevice->createInvalidShaderModule());
        }
        return WebGPU::CAPI::releaseToAPI(protectedDevice->createShaderModule(*apiDescriptor));
    }
}

WGPUTexture wgpuDeviceCreateTexture(WGPUDevice device, const WGPUTextureDescriptor* descriptor)
{
    @autoreleasepool {
        Ref protectedDevice = WebGPU::CAPI::fromAPI(device);
        WebGPU::CAPI::TextureDescriptorStorage storage;
        auto apiDescriptor = WebGPU::CAPI::fromAPI(*descriptor, storage);
        if (!apiDescriptor) {
            protectedDevice->generateAValidationError("GPUTextureDescriptor has an invalid enum value or usage bit"_s);
            return WebGPU::CAPI::releaseToAPI(protectedDevice->createInvalidTexture());
        }
        return WebGPU::CAPI::releaseToAPI(protectedDevice->createTexture(*apiDescriptor));
    }
}

void wgpuDeviceDestroy(WGPUDevice device)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(device))->destroy();
    }
}

// The chained structs of limits are not filled in.
WGPUStatus wgpuDeviceGetLimits(WGPUDevice device, WGPULimits* limits)
{
    @autoreleasepool {
        auto* nextInChain = limits->nextInChain;
        *limits = WebGPU::CAPI::toAPI(protect(WebGPU::CAPI::fromAPI(device))->limits());
        limits->nextInChain = nextInChain;
        return WGPUStatus_Success;
    }
}

// The caller owns the reference to the adapter, as for every object the C API returns.
WGPUAdapter wgpuDeviceGetAdapter(WGPUDevice device)
{
    @autoreleasepool {
        return WebGPU::CAPI::releaseToAPI(Ref { protect(WebGPU::CAPI::fromAPI(device))->adapter() });
    }
}

// The caller owns the reference to the queue, as for every object the C API returns.
WGPUQueue wgpuDeviceGetQueue(WGPUDevice device)
{
    @autoreleasepool {
        return WebGPU::CAPI::releaseToAPI(protect(WebGPU::CAPI::fromAPI(device))->queue());
    }
}

WGPUBool wgpuDeviceHasFeature(WGPUDevice device, WGPUFeatureName feature)
{
    @autoreleasepool {
        auto apiFeature = WebGPU::CAPI::fromAPI(feature);
        return apiFeature && protect(WebGPU::CAPI::fromAPI(device))->features().contains(*apiFeature);
    }
}

// The features for which wgpuDeviceHasFeature() is true, freed with
// wgpuSupportedFeaturesFreeMembers().
void wgpuDeviceGetFeatures(WGPUDevice device, WGPUSupportedFeatures* features)
{
    @autoreleasepool {
        *features = WebGPU::CAPI::toAPIAllocated(protect(WebGPU::CAPI::fromAPI(device))->features());
    }
}

// A scope that could not be popped is WGPUPopErrorScopeStatus_Error.
WGPUFuture wgpuDevicePopErrorScope(WGPUDevice device, WGPUPopErrorScopeCallbackInfo callbackInfo)
{
    @autoreleasepool {
        Ref protectedDevice = WebGPU::CAPI::fromAPI(device);
        WebGPU::CAPI::Future future { protectedDevice->instance() };
        protectedDevice->popErrorScope([callbackInfo, future](bool succeeded, std::optional<WebGPU::Error>&& error) {
            if (error) {
                auto message = error->message.utf8();
                callbackInfo.callback(WGPUPopErrorScopeStatus_Success, WebGPU::CAPI::toAPI(error), WebGPU::CAPI::toAPI(message), callbackInfo.userdata1, callbackInfo.userdata2);
            } else if (succeeded)
                callbackInfo.callback(WGPUPopErrorScopeStatus_Success, WGPUErrorType_NoError, WebGPU::CAPI::toAPI(""_s), callbackInfo.userdata1, callbackInfo.userdata2);
            else
                callbackInfo.callback(WGPUPopErrorScopeStatus_Error, WGPUErrorType_NoError, WebGPU::CAPI::toAPI("popErrorScope() failed validation."_s), callbackInfo.userdata1, callbackInfo.userdata2);
            future.complete();
        });
        return future.future();
    }
}

void wgpuDevicePushErrorScope(WGPUDevice device, WGPUErrorFilter filter)
{
    @autoreleasepool {
        Ref protectedDevice = WebGPU::CAPI::fromAPI(device);
        auto apiFilter = WebGPU::CAPI::fromAPI(filter);
        if (!apiFilter) {
            protectedDevice->generateAValidationError("pushErrorScope: invalid error filter"_s);
            return;
        }
        protectedDevice->pushErrorScope(*apiFilter);
    }
}

void wgpuDeviceSetLabel(WGPUDevice device, WGPUStringView label)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(device))->setLabel(WebGPU::CAPI::fromAPI(label));
    }
}
