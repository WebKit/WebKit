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
#import "PresentationContextCoreAnimation.h"
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
    m_condition.notifyAll();
}

uint64_t Instance::createFuture()
{
    Locker locker(m_lock);
    return m_nextFuture++;
}

void Instance::completeFuture(uint64_t future)
{
    Locker locker(m_lock);
    m_completedFutures.add(future);
    m_condition.notifyAll();
}

bool Instance::waitForAnyFuture(std::span<const uint64_t> futures, Seconds timeout, NOESCAPE const Function<void(size_t)>& didComplete)
{
    auto deadline = MonotonicTime::timePointFromNow(timeout);
    while (true) {
        processEvents();
        {
            Locker locker(m_lock);
            if (anyFutureCompleted(futures)) {
                for (auto [index, future] : indexedRange(futures)) {
                    if (m_completedFutures.remove(future))
                        didComplete(index);
                }
                return true;
            }
            if (m_pendingWork.isEmpty() && !m_condition.waitUntil(m_lock, deadline, [&] WTF_REQUIRES_LOCK(m_lock) { return !m_pendingWork.isEmpty() || anyFutureCompleted(futures); }))
                return false;
        }
    }
}

bool Instance::anyFutureCompleted(std::span<const uint64_t> futures) const
{
    for (auto future : futures) {
        if (m_completedFutures.contains(future))
            return true;
    }
    return false;
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
    @autoreleasepool {
        protect(WebGPU::Metal::fromAPI(instance))->waitForCommandBufferCompletions();
        WebGPU::Metal::fromAPI(instance).deref();
    }
}

// The required instance features are not checked: the instance has all of them. The instance has
// no scheduler: its callbacks run in wgpuInstanceProcessEvents() and wgpuInstanceWaitAny().
WGPUInstance wgpuCreateInstance(const WGPUInstanceDescriptor*)
{
    @autoreleasepool {
        return WebGPU::Metal::releaseToAPI(WebGPU::Metal::Instance::create({ }));
    }
}

WGPUProc LEGACY_NODELETE wgpuGetProcAddress(WGPUStringView)
{
    return nullptr;
}

// A surface presents into the CAMetalLayer of its WGPUSurfaceSourceMetalLayer. The other surface
// sources are not supported: the surface presents nowhere.
WGPUSurface wgpuInstanceCreateSurface(WGPUInstance instance, const WGPUSurfaceDescriptor* descriptor)
{
    @autoreleasepool {
        if (auto* metalLayerSource = descriptor ? WebGPU::Metal::findChainedStruct<WGPUSurfaceSourceMetalLayer>(descriptor->nextInChain) : nullptr) {
            if (CAMetalLayer *layer = (__bridge CAMetalLayer *)metalLayerSource->layer)
                return WebGPU::Metal::releaseToAPI(WebGPU::Metal::PresentationContextCoreAnimation::create(layer));
        }
        return WebGPU::Metal::releaseToAPIAs<WebGPU::Metal::PresentationContext>(protect(WebGPU::Metal::fromAPI(instance))->createPresentationContext({ }));
    }
}

void wgpuInstanceProcessEvents(WGPUInstance instance)
{
    @autoreleasepool {
        protect(WebGPU::Metal::fromAPI(instance))->processEvents();
    }
}

// The C API reports an adapter that is not available with WGPURequestAdapterStatus_Unavailable
// and no adapter. Null options are the default ones.
WGPUFuture wgpuInstanceRequestAdapter(WGPUInstance instance, const WGPURequestAdapterOptions* options, WGPURequestAdapterCallbackInfo callbackInfo)
{
    @autoreleasepool {
        Ref protectedInstance = WebGPU::Metal::fromAPI(instance);
        WebGPU::Metal::CAPIFuture future { protectedInstance.copyRef() };
        auto callback = [callbackInfo, future](WGPURequestAdapterStatus status, WGPUAdapter adapter, ASCIILiteral message) {
            callbackInfo.callback(status, adapter, WebGPU::Metal::toAPI(message), callbackInfo.userdata1, callbackInfo.userdata2);
            future.complete();
        };
        WGPURequestAdapterOptions defaultOptions = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
        auto apiOptions = WebGPU::Metal::fromAPI(options ? *options : defaultOptions);
        if (!apiOptions) {
            callback(WGPURequestAdapterStatus_Error, nullptr, "Unknown power preference"_s);
            return future.future();
        }
        protectedInstance->requestAdapter(*apiOptions, [callback = WTF::move(callback)](RefPtr<WebGPU::Adapter>&& adapter) {
            if (!adapter)
                return callback(WGPURequestAdapterStatus_Unavailable, nullptr, "No adapters present"_s);
            callback(WGPURequestAdapterStatus_Success, WebGPU::Metal::releaseToAPIAs<WebGPU::Metal::Adapter>(WTF::move(adapter)), ""_s);
        });
        return future.future();
    }
}

// A timeout of UINT64_MAX never passes.
WGPUWaitStatus wgpuInstanceWaitAny(WGPUInstance instance, size_t futureCount, WGPUFutureWaitInfo* futures, uint64_t timeoutNS)
{
    @autoreleasepool {
        auto waitInfos = unsafeMakeSpan(futures, futureCount);
        auto futureIDs = WTF::map(waitInfos, [](auto& waitInfo) {
            return waitInfo.future.id;
        });
        auto timeout = timeoutNS == UINT64_MAX ? Seconds::infinity() : Seconds::fromNanoseconds(timeoutNS);
        bool completed = protect(WebGPU::Metal::fromAPI(instance))->waitForAnyFuture(futureIDs.span(), timeout, [&](size_t index) {
            waitInfos[index].completed = true;
        });
        return completed ? WGPUWaitStatus_Success : WGPUWaitStatus_TimedOut;
    }
}
