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
#import "Instance.h"

#import "APIConversions.h"
#import "Adapter.h"
#import "CommandBuffer.h"
#import "HardwareCapabilities.h"
#import "PresentationContext.h"
#import <cstring>
#import <dlfcn.h>
#import <wtf/BlockPtr.h>
#import <wtf/MachSendRight.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebGPU::Metal {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Instance);

static NSArray<id<MTLDevice>>* getDevices()
{
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    NSArray<id<MTLDevice>> *devices = MTLCopyAllDevices();
#else
    NSMutableArray<id<MTLDevice>> *devices = [NSMutableArray array];
    if (id<MTLDevice> device = MTLCreateSystemDefaultDevice())
        [devices addObject:device];
#endif
    return devices;
}

Ref<Instance> Instance::create(WebGPU::InstanceDescriptor&& descriptor)
{
    return adoptRef(*new Instance(WTF::move(descriptor.scheduleWork), descriptor.webProcessResourceOwner ? &*descriptor.webProcessResourceOwner : nullptr));
}

Instance::Instance(Function<void(WorkItem&&)>&& scheduleWork, const MachSendRight* webProcessResourceOwner)
    : m_webProcessID(webProcessResourceOwner ? std::optional<MachSendRight>(*webProcessResourceOwner) : std::nullopt)
    , m_scheduleWork(WTF::move(scheduleWork))
{
}

Instance::Instance()
    : m_isValid(false)
{
}

Instance::~Instance() = default;

void Instance::waitForCommandBufferCompletions()
{
    auto retainedCommandBuffers { std::exchange(m_retainedCommandBufferInstances, { }) };
    auto retainedDevices { std::exchange(retainedDeviceInstances, { }) };
    for (const auto& [_, weakCommandBuffer] : retainedCommandBuffers) {
        if (id<MTLCommandBuffer> commandBuffer = weakCommandBuffer.get().get(); commandBuffer.status >= MTLCommandBufferStatusCommitted)
            [commandBuffer waitUntilCompleted];
    }
    for (const auto& container : retainedDevices.values()) {
        for (const auto& weakCommandBuffer : container) {
            if (id<MTLCommandBuffer> commandBuffer = weakCommandBuffer.get().get(); commandBuffer.status >= MTLCommandBufferStatusCommitted)
                [commandBuffer waitUntilCompleted];
        }
    }
}

RefPtr<WebGPU::PresentationContext> Instance::createPresentationContext(const WebGPU::PresentationContextDescriptor& descriptor)
{
    return PresentationContext::create(descriptor, *this);
}

void Instance::scheduleWork(WorkItem&& workItem)
{
    if (!m_scheduleWork) {
        defaultScheduleWork(WTF::move(workItem));
        return;
    }
    m_scheduleWork(WTF::move(workItem));
}

const std::optional<const MachSendRight>& Instance::webProcessID() const
{
    return m_webProcessID;
}

void Instance::defaultScheduleWork(WorkItem&& workItem)
{
    Locker locker(m_lock);
    m_pendingWork.append(WTF::move(workItem));
}

void Instance::processEvents()
{
    while (true) {
        Deque<WorkItem> localWork;
        {
            Locker locker(m_lock);
            std::swap(m_pendingWork, localWork);
        }
        if (localWork.isEmpty())
            return;
        for (auto& workItem : localWork)
            workItem();
    }
}

static NSArray<id<MTLDevice>> *sortedDevices(NSArray<id<MTLDevice>> *devices, std::optional<WebGPU::PowerPreference> powerPreference)
{
    if (!powerPreference)
        return devices;
    switch (*powerPreference) {
    case WebGPU::PowerPreference::LowPower:
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        return [devices sortedArrayWithOptions:NSSortStable usingComparator:^NSComparisonResult (id<MTLDevice> obj1, id<MTLDevice> obj2)
        {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (obj1.lowPower == obj2.lowPower)
                return NSOrderedSame;
            if (obj1.lowPower)
                return NSOrderedAscending;
            ALLOW_DEPRECATED_DECLARATIONS_END
            return NSOrderedDescending;
        }];
#else
        return devices;
#endif
    case WebGPU::PowerPreference::HighPerformance:
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        return [devices sortedArrayWithOptions:NSSortStable usingComparator:^NSComparisonResult (id<MTLDevice> obj1, id<MTLDevice> obj2)
        {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (obj1.lowPower == obj2.lowPower)
                return NSOrderedSame;
            if (obj1.lowPower)
                return NSOrderedDescending;
            ALLOW_DEPRECATED_DECLARATIONS_END
            return NSOrderedAscending;
        }];
#else
        return devices;
#endif
    }
}

void Instance::requestAdapter(const WebGPU::RequestAdapterOptions& options, CompletionHandler<void(RefPtr<WebGPU::Adapter>&&)>&& callback)
{
    auto devices = getDevices();

    // FIXME: Deal with options.compatibleSurface.

    auto sortedDevices = WebGPU::Metal::sortedDevices(devices, options.powerPreference);

    // There is no fallback adapter.
    if (options.forceFallbackAdapter || !sortedDevices || !sortedDevices.count || !sortedDevices[0]) {
        callback(nullptr);
        return;
    }

    auto device = sortedDevices[0];

    // The device does not support WebGPU.
    auto deviceCapabilities = hardwareCapabilities(device);
    if (!deviceCapabilities) {
        callback(nullptr);
        return;
    }

    // FIXME: this should be asynchronous
    callback(Adapter::create(sortedDevices[0], *this, options.xrCompatible, WTF::move(*deviceCapabilities)));
}

void Instance::retainDevice(Device& device, id<MTLCommandBuffer> commandBuffer)
{
    auto& container = retainedDeviceInstances.ensure(device, [] {
        return CommandBufferContainer { };
    }).iterator->value;

    container.append(commandBuffer);

    releaseCompletedCommandBuffers();
}

void Instance::retainCommandBuffer(CommandBuffer& commandBuffer, id<MTLCommandBuffer> mtlCommandBuffer)
{
    releaseCompletedCommandBuffers();
    m_retainedCommandBufferInstances.append({ commandBuffer, mtlCommandBuffer });
}

void Instance::releaseCompletedCommandBuffers()
{
    m_retainedCommandBufferInstances.removeAllMatching([](auto& pair) {
        return !pair.second;
    });

    for (auto& container : retainedDeviceInstances.values()) {
        container.removeAllMatching([&](auto& pair) {
            return !pair;
        });
    }
    retainedDeviceInstances.removeIf([&](auto& pair) {
        return !pair.value.size();
    });
}

id<MTLDevice> Instance::device() const
{
    return getDevices().firstObject;
}

} // namespace WebGPU::Metal

namespace WebGPU {

RefPtr<Instance> createInstance(InstanceDescriptor&& descriptor)
{
    return Metal::Instance::create(WTF::move(descriptor));
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void LEGACY_NODELETE wgpuInstanceAddRef(WGPUInstance instance)
{
    WebGPU::Metal::fromAPI(instance).ref();
}

void wgpuInstanceRelease(WGPUInstance instance)
{
    protect(WebGPU::Metal::fromAPI(instance))->waitForCommandBufferCompletions();
    WebGPU::Metal::fromAPI(instance).deref();
}

WGPUInstance wgpuCreateInstance(const WGPUInstanceDescriptor* descriptor)
{
    WebGPU::InstanceDescriptor apiDescriptor;
    if (auto* cocoaDescriptor = WebGPU::Metal::findChainedStruct<WGPUInstanceCocoaDescriptor>(descriptor->nextInChain)) {
        if (auto scheduleWorkBlock = makeBlockPtr(cocoaDescriptor->scheduleWorkBlock)) {
            apiDescriptor.scheduleWork = [scheduleWorkBlock = WTF::move(scheduleWorkBlock)](Function<void()>&& workItem) {
                scheduleWorkBlock(makeBlockPtr(WTF::move(workItem)).get());
            };
        }
        if (cocoaDescriptor->webProcessResourceOwner)
            apiDescriptor.webProcessResourceOwner.emplace(*reinterpret_cast<const MachSendRight*>(cocoaDescriptor->webProcessResourceOwner));
    }
    return WebGPU::Metal::releaseToAPI(WebGPU::Metal::Instance::create(WTF::move(apiDescriptor)));
}

WGPUProc LEGACY_NODELETE wgpuGetProcAddress(WGPUDevice, const char*)
{
    return nullptr;
}

WGPUSurface wgpuInstanceCreateSurface(WGPUInstance instance, const WGPUSurfaceDescriptor* descriptor)
{
    WebGPU::PresentationContextDescriptor apiDescriptor;
    if (auto* customSurface = WebGPU::Metal::findChainedStruct<WGPUSurfaceDescriptorCocoaCustomSurface>(descriptor->nextInChain)) {
        apiDescriptor.registerCompositorIntegration = [compositorIntegrationRegister = makeBlockPtr(customSurface->compositorIntegrationRegister)](Function<void(std::span<const IOSurfaceRef>)>&& renderBuffersWereRecreated, Function<void(CompletionHandler<void()>&&)>&& onSubmittedWorkScheduled) {
            compositorIntegrationRegister(makeBlockPtr([renderBuffersWereRecreated = WTF::move(renderBuffersWereRecreated)](CFArrayRef ioSurfaces) {
                Vector<IOSurfaceRef> surfaces;
                for (CFIndex i = 0, count = CFArrayGetCount(ioSurfaces); i < count; ++i)
                    surfaces.append(static_cast<IOSurfaceRef>(const_cast<void*>(CFArrayGetValueAtIndex(ioSurfaces, i))));
                renderBuffersWereRecreated(surfaces.span());
            }).get(), makeBlockPtr([onSubmittedWorkScheduled = WTF::move(onSubmittedWorkScheduled)](WGPUWorkItem workItem) {
                onSubmittedWorkScheduled([workItem = makeBlockPtr(workItem)] {
                    workItem();
                });
            }).get());
        };
    }
    return WebGPU::Metal::releaseToAPIAs<WebGPU::Metal::PresentationContext>(protect(WebGPU::Metal::fromAPI(instance))->createPresentationContext(apiDescriptor));
}

void wgpuInstanceProcessEvents(WGPUInstance instance)
{
    protect(WebGPU::Metal::fromAPI(instance))->processEvents();
}

// The C API reports an adapter that is not available with WGPURequestAdapterStatus_Unavailable and no adapter.
static void requestAdapter(WGPUInstance instance, const WGPURequestAdapterOptions& options, Function<void(WGPURequestAdapterStatus, WGPUAdapter, const char*)>&& callback)
{
    Ref protectedInstance = WebGPU::Metal::fromAPI(instance);
    auto apiOptions = WebGPU::Metal::fromAPI(options);
    if (!apiOptions)
        return callback(WGPURequestAdapterStatus_Error, nullptr, "Unknown power preference");
    protectedInstance->requestAdapter(*apiOptions, [callback = WTF::move(callback)](RefPtr<WebGPU::Adapter>&& adapter) {
        if (!adapter)
            return callback(WGPURequestAdapterStatus_Unavailable, nullptr, "No adapters present");
        callback(WGPURequestAdapterStatus_Success, WebGPU::Metal::releaseToAPIAs<WebGPU::Metal::Adapter>(WTF::move(adapter)), "");
    });
}

void wgpuInstanceRequestAdapter(WGPUInstance instance, const WGPURequestAdapterOptions* options, WGPURequestAdapterCallback callback, void* userdata)
{
    requestAdapter(instance, *options, [callback, userdata](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message) {
        callback(status, adapter, message, userdata);
    });
}

void wgpuInstanceRequestAdapterWithBlock(WGPUInstance instance, WGPURequestAdapterOptions const * options, WGPURequestAdapterBlockCallback callback)
{
    requestAdapter(instance, *options, [callback = WebGPU::Metal::fromAPI(WTF::move(callback))](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message) {
        callback(status, adapter, message);
    });
}

// Fuzzer things
WGPUBool wgpuBufferIsValid(WGPUBuffer buffer)
{
    return protect(WebGPU::Metal::fromAPI(buffer))->isValid();
}

WGPUBool wgpuAdapterIsValid(WGPUAdapter adapter)
{
    return protect(WebGPU::Metal::fromAPI(adapter))->isValid();
}

WGPUBool wgpuBindGroupIsValid(WGPUBindGroup bindGroup)
{
    return protect(WebGPU::Metal::fromAPI(bindGroup))->isValid();
}

WGPUBool wgpuBindGroupLayoutIsValid(WGPUBindGroupLayout bindGroupLayout)
{
    return protect(WebGPU::Metal::fromAPI(bindGroupLayout))->isValid();
}

WGPUBool wgpuCommandBufferIsValid(WGPUCommandBuffer commandBuffer)
{
    return protect(WebGPU::Metal::fromAPI(commandBuffer))->isValid();
}

WGPUBool wgpuCommandEncoderIsValid(WGPUCommandEncoder commandEncoder)
{
    return protect(WebGPU::Metal::fromAPI(commandEncoder))->isValid();
}

WGPUBool wgpuComputePassEncoderIsValid(WGPUComputePassEncoder computePassEncoder)
{
    return protect(WebGPU::Metal::fromAPI(computePassEncoder))->isValid();
}

WGPUBool wgpuComputePipelineIsValid(WGPUComputePipeline computePipeline)
{
    return protect(WebGPU::Metal::fromAPI(computePipeline))->isValid();
}

WGPUBool wgpuDeviceIsValid(WGPUDevice device)
{
    return protect(WebGPU::Metal::fromAPI(device))->isValid();
}

WGPUBool wgpuExternalTextureIsValid(WGPUExternalTexture externalTexture)
{
    return protect(WebGPU::Metal::fromAPI(externalTexture))->isValid();
}

WGPUBool wgpuPipelineLayoutIsValid(WGPUPipelineLayout pipelineLayout)
{
    return protect(WebGPU::Metal::fromAPI(pipelineLayout))->isValid();
}

WGPUBool wgpuPresentationContextIsValid(WGPUSurface presentationContext)
{
    return protect(WebGPU::Metal::fromAPI(presentationContext))->isValid();
}

WGPUBool wgpuQuerySetIsValid(WGPUQuerySet querySet)
{
    return protect(WebGPU::Metal::fromAPI(querySet))->isValid();
}

WGPUBool wgpuQueueIsValid(WGPUQueue queue)
{
    return protect(WebGPU::Metal::fromAPI(queue))->isValid();
}

WGPUBool wgpuRenderBundleEncoderIsValid(WGPURenderBundleEncoder renderBundleEncoder)
{
    return protect(WebGPU::Metal::fromAPI(renderBundleEncoder))->isValid();
}

WGPUBool wgpuRenderBundleIsValid(WGPURenderBundle renderBundle)
{
    return protect(WebGPU::Metal::fromAPI(renderBundle))->isValid();
}

WGPUBool wgpuRenderPassEncoderIsValid(WGPURenderPassEncoder renderPassEncoder)
{
    return protect(WebGPU::Metal::fromAPI(renderPassEncoder))->isValid();
}

WGPUBool wgpuRenderPipelineIsValid(WGPURenderPipeline renderPipeline)
{
    return protect(WebGPU::Metal::fromAPI(renderPipeline))->isValid();
}

WGPUBool wgpuSamplerIsValid(WGPUSampler sampler)
{
    return protect(WebGPU::Metal::fromAPI(sampler))->isValid();
}

WGPUBool wgpuShaderModuleIsValid(WGPUShaderModule shaderModule)
{
    return protect(WebGPU::Metal::fromAPI(shaderModule))->isValid();
}

WGPUBool wgpuTextureIsValid(WGPUTexture texture)
{
    return protect(WebGPU::Metal::fromAPI(texture))->isValid();
}

WGPUBool wgpuTextureViewIsValid(WGPUTextureView textureView)
{
    return protect(WebGPU::Metal::fromAPI(textureView))->isValid();
}

WGPUBool wgpuXRBindingIsValid(WGPUXRBinding binding)
{
    return protect(WebGPU::Metal::fromAPI(binding))->isValid();
}

WGPUBool wgpuXRSubImageIsValid(WGPUXRSubImage subImage)
{
    return protect(WebGPU::Metal::fromAPI(subImage))->isValid();
}

WGPUBool wgpuXRProjectionLayerIsValid(WGPUXRProjectionLayer layer)
{
    return protect(WebGPU::Metal::fromAPI(layer))->isValid();
}

WGPUBool wgpuXRViewIsValid(WGPUXRView view)
{
    return protect(WebGPU::Metal::fromAPI(view))->isValid();
}
