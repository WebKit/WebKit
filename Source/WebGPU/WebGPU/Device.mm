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
#import <algorithm>
#import <notify.h>
#import <wtf/StdLibExtras.h>
#import <wtf/WeakPtr.h>

namespace WebGPU {

struct GPUFrameCapture {
    static void captureSingleFrameIfNeeded(id<MTLDevice> captureObject)
    {
        if (enabled) {
            captureFrame(captureObject);
            enabled = false;
        }
    }

    static void registerForFrameCapture(id<MTLDevice> captureObject)
    {
        // Allow GPU frame capture "notifyutil -p com.apple.WebKit.WebGPU.CaptureFrame" when process is
        // run with __XPC_METAL_CAPTURE_ENABLED=1
        static std::once_flag onceFlag;
        std::call_once(onceFlag, [] {
            int captureFrameToken;
            notify_register_dispatch("com.apple.WebKit.WebGPU.CaptureFrame", &captureFrameToken, dispatch_get_main_queue(), ^(int) {
                enabled = true;
            });

            int captureFirstFrameToken;
            notify_register_dispatch("com.apple.WebKit.WebGPU.ToggleCaptureFirstFrame", &captureFirstFrameToken, dispatch_get_main_queue(), ^(int) {
                captureFirstFrame = !captureFirstFrame;
            });
        });

        if (captureFirstFrame)
            captureFrame(captureObject);
    }
private:
    static void captureFrame(id<MTLDevice> captureObject)
    {
        MTLCaptureManager* captureManager = [MTLCaptureManager sharedCaptureManager];
        if ([captureManager isCapturing])
            return;

        MTLCaptureDescriptor* captureDescriptor = [[MTLCaptureDescriptor alloc] init];
        captureDescriptor.captureObject = captureObject;
        captureDescriptor.destination = MTLCaptureDestinationGPUTraceDocument;
        captureDescriptor.outputURL = [[NSFileManager.defaultManager temporaryDirectory] URLByAppendingPathComponent:[NSString stringWithFormat:@"%@.gputrace", NSUUID.UUID.UUIDString]];

        NSError *error;
        if (![captureManager startCaptureWithDescriptor:captureDescriptor error:&error])
            WTFLogAlways("Failed to start GPU frame capture at path %@, error %@", captureDescriptor.outputURL.absoluteString, error);
        else
            WTFLogAlways("Success starting GPU frame capture at path %@", captureDescriptor.outputURL.absoluteString);
    }

    static bool captureFirstFrame;
    static bool enabled;
};

bool GPUFrameCapture::captureFirstFrame = false;
bool GPUFrameCapture::enabled = false;

Ref<Device> Device::create(id<MTLDevice> device, String&& deviceLabel, HardwareCapabilities&& capabilities, Adapter& adapter)
{
    id<MTLCommandQueue> commandQueue = [device newCommandQueue];
    if (!commandQueue)
        return Device::createInvalid(adapter);

    // See the comment in Device::setLabel() about why we're not setting the label on the MTLDevice here.

    commandQueue.label = @"Default queue";
    if (!deviceLabel.isEmpty())
        commandQueue.label = [NSString stringWithFormat:@"Default queue for device %s", deviceLabel.utf8().data()];

    return adoptRef(*new Device(device, commandQueue, WTFMove(capabilities), adapter));
}

Device::Device(id<MTLDevice> device, id<MTLCommandQueue> defaultQueue, HardwareCapabilities&& capabilities, Adapter& adapter)
    : m_device(device)
    , m_defaultQueue(Queue::create(defaultQueue, *this))
    , m_capabilities(WTFMove(capabilities))
    , m_adapter(adapter)
{
#if PLATFORM(MAC)
    auto devices = MTLCopyAllDevicesWithObserver(&m_deviceObserver, [weakThis = ThreadSafeWeakPtr { *this }](id<MTLDevice> device, MTLDeviceNotificationName) {
        RefPtr<Device> strongThis = weakThis.get();
        if (!strongThis)
            return;
        strongThis->instance().scheduleWork([strongThis = WTFMove(strongThis), device = device]() {
            if (![strongThis->m_device isEqual:device])
                return;
            strongThis->loseTheDevice(WGPUDeviceLostReason_Undefined);
        });
    });

#if ASSERT_ENABLED
    bool found = false;
    for (id<MTLDevice> observedDevice in devices) {
        if ([observedDevice isEqual:device]) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#else
    UNUSED_VARIABLE(devices);
#endif
#endif

    GPUFrameCapture::registerForFrameCapture(m_device);
}

Device::Device(Adapter& adapter)
    : m_defaultQueue(Queue::createInvalid(*this))
    , m_adapter(adapter)
{
}

Device::~Device()
{
#if PLATFORM(MAC)
    MTLRemoveDeviceObserver(m_deviceObserver);
#endif
}

void Device::loseTheDevice(WGPUDeviceLostReason reason)
{
    // https://gpuweb.github.io/gpuweb/#lose-the-device

    m_adapter->makeInvalid();

    makeInvalid();

    if (m_deviceLostCallback) {
        instance().scheduleWork([deviceLostCallback = WTFMove(m_deviceLostCallback), reason]() {
            deviceLostCallback(reason, "Device lost."_s);
        });
    }

    // FIXME: The spec doesn't actually say to do this, but it's pretty important because
    // the total number of command queues alive at a time is limited to a pretty low limit.
    // We should make sure either that this is unobservable or that the spec says to do this.
    m_defaultQueue->makeInvalid();

    m_isLost = true;
}

void Device::destroy()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-destroy

    loseTheDevice(WGPUDeviceLostReason_Destroyed);
}

size_t Device::enumerateFeatures(WGPUFeatureName* features)
{
    // The API contract for this requires that sufficient space has already been allocated for the output.
    // This requires the caller calling us twice: once to get the amount of space to allocate, and once to fill the space.
    if (features)
        std::copy(m_capabilities.features.begin(), m_capabilities.features.end(), features);
    return m_capabilities.features.size();
}

bool Device::getLimits(WGPUSupportedLimits& limits)
{
    if (limits.nextInChain != nullptr)
        return false;

    limits.limits = m_capabilities.limits;
    return true;
}

Queue& Device::getQueue()
{
    return m_defaultQueue;
}

bool Device::hasFeature(WGPUFeatureName feature)
{
    return std::find(m_capabilities.features.begin(), m_capabilities.features.end(), feature);
}

auto Device::currentErrorScope(WGPUErrorFilter type) -> ErrorScope*
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-current-error-scope

    for (auto iterator = m_errorScopeStack.rbegin(); iterator != m_errorScopeStack.rend(); ++iterator) {
        if (iterator->filter == type)
            return &*iterator;
    }
    return nullptr;
}

void Device::generateAValidationError(String&& message)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-generate-a-validation-error

    auto* scope = currentErrorScope(WGPUErrorFilter_Validation);

    if (scope) {
        if (!scope->error)
            scope->error = Error { WGPUErrorType_Validation, WTFMove(message) };
        return;
    }

    if (m_uncapturedErrorCallback) {
        instance().scheduleWork([protectedThis = Ref { *this }, message = WTFMove(message)]() mutable {
            protectedThis->m_uncapturedErrorCallback(WGPUErrorType_Validation, WTFMove(message));
        });
    }
}

void Device::captureFrameIfNeeded() const
{
    GPUFrameCapture::captureSingleFrameIfNeeded(m_device);
}

bool Device::validatePopErrorScope() const
{
    if (m_isLost)
        return false;

    if (m_errorScopeStack.isEmpty())
        return false;

    return true;
}

bool Device::popErrorScope(CompletionHandler<void(WGPUErrorType, String&&)>&& callback)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-poperrorscope

    if (!validatePopErrorScope()) {
        instance().scheduleWork([callback = WTFMove(callback)]() mutable {
            callback(WGPUErrorType_Unknown, "popErrorScope() failed validation."_s);
        });
        return false;
    }

    auto scope = m_errorScopeStack.takeLast();

    instance().scheduleWork([scope = WTFMove(scope), callback = WTFMove(callback)]() mutable {
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

    ErrorScope scope { std::nullopt, filter };

    m_errorScopeStack.append(WTFMove(scope));
}

void Device::setDeviceLostCallback(Function<void(WGPUDeviceLostReason, String&&)>&& callback)
{
    m_deviceLostCallback = WTFMove(callback);
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
    WebGPU::fromAPI(device).createComputePipelineAsync(*descriptor, [callback, userdata](WGPUCreatePipelineAsyncStatus status, Ref<WebGPU::ComputePipeline>&& pipeline, String&& message) {
        callback(status, WebGPU::releaseToAPI(WTFMove(pipeline)), message.utf8().data(), userdata);
    });
}

void wgpuDeviceCreateComputePipelineAsyncWithBlock(WGPUDevice device, WGPUComputePipelineDescriptor const * descriptor, WGPUCreateComputePipelineAsyncBlockCallback callback)
{
    WebGPU::fromAPI(device).createComputePipelineAsync(*descriptor, [callback = WTFMove(callback)](WGPUCreatePipelineAsyncStatus status, Ref<WebGPU::ComputePipeline>&& pipeline, String&& message) {
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
    WebGPU::fromAPI(device).createRenderPipelineAsync(*descriptor, [callback, userdata](WGPUCreatePipelineAsyncStatus status, Ref<WebGPU::RenderPipeline>&& pipeline, String&& message) {
        callback(status, WebGPU::releaseToAPI(WTFMove(pipeline)), message.utf8().data(), userdata);
    });
}

void wgpuDeviceCreateRenderPipelineAsyncWithBlock(WGPUDevice device, WGPURenderPipelineDescriptor const * descriptor, WGPUCreateRenderPipelineAsyncBlockCallback callback)
{
    WebGPU::fromAPI(device).createRenderPipelineAsync(*descriptor, [callback = WTFMove(callback)](WGPUCreatePipelineAsyncStatus status, Ref<WebGPU::RenderPipeline>&& pipeline, String&& message) {
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
    return WebGPU::releaseToAPI(WebGPU::fromAPI(device).createSwapChain(surface, *descriptor));
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
