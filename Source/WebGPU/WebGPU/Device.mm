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

auto Device::currentErrorScope(WGPUErrorFilter type) -> ErrorScope*
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-current-error-scope

    // "Let scope be the last item of device.[[errorScopeStack]]."
    // "While scope is not undefined:"
    for (auto iterator = m_errorScopeStack.rbegin(); iterator != m_errorScopeStack.rend(); ++iterator) {
        // "If scope.[[filter]] is type, return scope."
        if (iterator->filter == type)
            return &*iterator;
        // "Set scope to the previous item of device.[[errorScopeStack]]."
    }
    // "Return undefined."
    return nullptr;
}

void Device::generateAValidationError(String&& message)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-generate-a-validation-error

    // "Let scope be the current error scope for error and device."
    auto* scope = currentErrorScope(WGPUErrorFilter_Validation);

    // "If scope is not undefined:"
    if (scope) {
        // "If scope.[[error]] is null, set scope.[[error]] to error."
        if (!scope->error)
            scope->error = Error { WGPUErrorType_Validation, WTFMove(message) };
        // "Stop."
        return;
    }

    // "Otherwise issue the following steps to the Content timeline:"
    // "If the user agent chooses, queue a task to fire a GPUUncapturedErrorEvent named "uncapturederror" on device with an error of error."
    if (m_uncapturedErrorCallback) {
        m_instance->scheduleWork([protectedThis = Ref { *this }, message = WTFMove(message)]() mutable {
            protectedThis->m_uncapturedErrorCallback(WGPUErrorType_Validation, WTFMove(message));
        });
    }
}

bool Device::validatePopErrorScope() const
{
    // FIXME: "this must not be lost."

    // "this.[[errorScopeStack]].size > 0."
    if (m_errorScopeStack.isEmpty())
        return false;

    return true;
}

bool Device::popErrorScope(CompletionHandler<void(WGPUErrorType, String&&)>&& callback)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-poperrorscope

    // "If any of the following requirements are unmet"
    if (!validatePopErrorScope()) {
        // "reject promise with an OperationError and stop."
        callback(WGPUErrorType_Unknown, "popErrorScope() failed validation."_s);
        return false;
    }

    // "Let scope be the result of popping an item off of this.[[errorScopeStack]]."
    auto scope = m_errorScopeStack.takeLast();

    // "Resolve promise with scope.[[error]]."
    m_instance->scheduleWork([scope = WTFMove(scope), callback = WTFMove(callback)]() mutable {
        if (scope.error)
            callback(scope.error->type, WTFMove(scope.error->message));
        else
            callback(WGPUErrorType_NoError, { });
    });

    // FIXME: Make sure this is the right thing to return.
    return true;
}

void Device::pushErrorScope(WGPUErrorFilter filter)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-pusherrorscope

    // "Let scope be a new GPU error scope."
    // "Set scope.[[filter]] to filter."
    ErrorScope scope { std::nullopt, filter };

    // "Push scope onto this.[[errorScopeStack]]."
    m_errorScopeStack.append(WTFMove(scope));
}

void Device::setDeviceLostCallback(Function<void(WGPUDeviceLostReason, String&&)>&& callback)
{
    // FIXME: Implement this.
    UNUSED_PARAM(callback);
}

void Device::setUncapturedErrorCallback(Function<void(WGPUErrorType, String&&)>&& callback)
{
    m_uncapturedErrorCallback = WTFMove(callback);
}

void Device::setLabel(String&&)
{
    // Because MTLDevices are process-global, we can't set the label on it, because 2 contexts' labels would fight each other.
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuDeviceRelease(WGPUDevice device)
{
    WebGPU::fromAPI(device).deref();
}

WGPUBindGroup wgpuDeviceCreateBindGroup(WGPUDevice device, const WGPUBindGroupDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(device).createBindGroup(*descriptor));
}

WGPUBindGroupLayout wgpuDeviceCreateBindGroupLayout(WGPUDevice device, const WGPUBindGroupLayoutDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(device).createBindGroupLayout(*descriptor));
}

WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice device, const WGPUBufferDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(device).createBuffer(*descriptor));
}

WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice device, const WGPUCommandEncoderDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(device).createCommandEncoder(*descriptor));
}

WGPUComputePipeline wgpuDeviceCreateComputePipeline(WGPUDevice device, const WGPUComputePipelineDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(device).createComputePipeline(*descriptor));
}

void wgpuDeviceCreateComputePipelineAsync(WGPUDevice device, const WGPUComputePipelineDescriptor* descriptor, WGPUCreateComputePipelineAsyncCallback callback, void* userdata)
{
    WebGPU::fromAPI(device).createComputePipelineAsync(*descriptor, [callback, userdata](WGPUCreatePipelineAsyncStatus status, RefPtr<WebGPU::ComputePipeline>&& pipeline, String&& message) {
        callback(status, WebGPU::releaseToAPI(WTFMove(pipeline)), message.utf8().data(), userdata);
    });
}

void wgpuDeviceCreateComputePipelineAsyncWithBlock(WGPUDevice device, WGPUComputePipelineDescriptor const * descriptor, WGPUCreateComputePipelineAsyncBlockCallback callback)
{
    WebGPU::fromAPI(device).createComputePipelineAsync(*descriptor, [callback = WTFMove(callback)](WGPUCreatePipelineAsyncStatus status, RefPtr<WebGPU::ComputePipeline>&& pipeline, String&& message) {
        callback(status, WebGPU::releaseToAPI(WTFMove(pipeline)), message.utf8().data());
    });
}

WGPUPipelineLayout wgpuDeviceCreatePipelineLayout(WGPUDevice device, const WGPUPipelineLayoutDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(device).createPipelineLayout(*descriptor));
}

WGPUQuerySet wgpuDeviceCreateQuerySet(WGPUDevice device, const WGPUQuerySetDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(device).createQuerySet(*descriptor));
}

WGPURenderBundleEncoder wgpuDeviceCreateRenderBundleEncoder(WGPUDevice device, const WGPURenderBundleEncoderDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(device).createRenderBundleEncoder(*descriptor));
}

WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice device, const WGPURenderPipelineDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(device).createRenderPipeline(*descriptor));
}

void wgpuDeviceCreateRenderPipelineAsync(WGPUDevice device, const WGPURenderPipelineDescriptor* descriptor, WGPUCreateRenderPipelineAsyncCallback callback, void* userdata)
{
    WebGPU::fromAPI(device).createRenderPipelineAsync(*descriptor, [callback, userdata](WGPUCreatePipelineAsyncStatus status, RefPtr<WebGPU::RenderPipeline>&& pipeline, String&& message) {
        callback(status, WebGPU::releaseToAPI(WTFMove(pipeline)), message.utf8().data(), userdata);
    });
}

void wgpuDeviceCreateRenderPipelineAsyncWithBlock(WGPUDevice device, WGPURenderPipelineDescriptor const * descriptor, WGPUCreateRenderPipelineAsyncBlockCallback callback)
{
    WebGPU::fromAPI(device).createRenderPipelineAsync(*descriptor, [callback = WTFMove(callback)](WGPUCreatePipelineAsyncStatus status, RefPtr<WebGPU::RenderPipeline>&& pipeline, String&& message) {
        callback(status, WebGPU::releaseToAPI(WTFMove(pipeline)), message.utf8().data());
    });
}

WGPUSampler wgpuDeviceCreateSampler(WGPUDevice device, const WGPUSamplerDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(device).createSampler(*descriptor));
}

WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice device, const WGPUShaderModuleDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(device).createShaderModule(*descriptor));
}

WGPUSwapChain wgpuDeviceCreateSwapChain(WGPUDevice device, WGPUSurface surface, const WGPUSwapChainDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(device).createSwapChain(WebGPU::fromAPI(surface), *descriptor));
}

WGPUTexture wgpuDeviceCreateTexture(WGPUDevice device, const WGPUTextureDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(device).createTexture(*descriptor));
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
    return &WebGPU::fromAPI(device).getQueue();
}

bool wgpuDeviceHasFeature(WGPUDevice device, WGPUFeatureName feature)
{
    return WebGPU::fromAPI(device).hasFeature(feature);
}

bool wgpuDevicePopErrorScope(WGPUDevice device, WGPUErrorCallback callback, void* userdata)
{
    return WebGPU::fromAPI(device).popErrorScope([callback, userdata](WGPUErrorType type, String&& message) {
        callback(type, message.utf8().data(), userdata);
    });
}

bool wgpuDevicePopErrorScopeWithBlock(WGPUDevice device, WGPUErrorBlockCallback callback)
{
    return WebGPU::fromAPI(device).popErrorScope([callback = WTFMove(callback)](WGPUErrorType type, String&& message) {
        callback(type, message.utf8().data());
    });
}

void wgpuDevicePushErrorScope(WGPUDevice device, WGPUErrorFilter filter)
{
    WebGPU::fromAPI(device).pushErrorScope(filter);
}

void wgpuDeviceSetDeviceLostCallback(WGPUDevice device, WGPUDeviceLostCallback callback, void* userdata)
{
    return WebGPU::fromAPI(device).setDeviceLostCallback([callback, userdata](WGPUDeviceLostReason reason, String&& message) {
        if (callback)
            callback(reason, message.utf8().data(), userdata);
    });
}

void wgpuDeviceSetDeviceLostCallbackWithBlock(WGPUDevice device, WGPUDeviceLostBlockCallback callback)
{
    return WebGPU::fromAPI(device).setDeviceLostCallback([callback = WTFMove(callback)](WGPUDeviceLostReason reason, String&& message) {
        if (callback)
            callback(reason, message.utf8().data());
    });
}

void wgpuDeviceSetUncapturedErrorCallback(WGPUDevice device, WGPUErrorCallback callback, void* userdata)
{
    return WebGPU::fromAPI(device).setUncapturedErrorCallback([callback, userdata](WGPUErrorType type, String&& message) {
        if (callback)
            callback(type, message.utf8().data(), userdata);
    });
}

void wgpuDeviceSetUncapturedErrorCallbackWithBlock(WGPUDevice device, WGPUErrorBlockCallback callback)
{
    return WebGPU::fromAPI(device).setUncapturedErrorCallback([callback = WTFMove(callback)](WGPUErrorType type, String&& message) {
        if (callback)
            callback(type, message.utf8().data());
    });
}

void wgpuDeviceSetLabel(WGPUDevice device, const char* label)
{
    WebGPU::fromAPI(device).setLabel(WebGPU::fromAPI(label));
}
