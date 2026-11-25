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
#import <wtf/Borrow.h>
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

void Instance::destroy()
{
    // The Metal completion handlers of the asynchronous compiles hold the instance until they have
    // scheduled their work, and the Metal command buffers in flight run handlers that use it. Once
    // these are done, nothing on Metal's threads can release the last reference to the instance.
    for (; m_startedAsyncCompileCount; --m_startedAsyncCompileCount)
        m_asyncCompileCompletions->wait();
    waitForCommandBufferCompletions();
}

void Instance::waitForCommandBufferCompletions()
{
    auto retainedCommandBuffers { std::exchange(m_retainedCommandBufferInstances, { }) };
    auto retainedDevices { std::exchange(retainedDeviceInstances, { }) };
    for (const auto& [_, weakCommandBuffer] : borrow(retainedCommandBuffers)) {
        if (id<MTLCommandBuffer> commandBuffer = weakCommandBuffer.get().get(); commandBuffer.status >= MTLCommandBufferStatusCommitted)
            [commandBuffer waitUntilCompleted];
    }
    for (const auto& container : retainedDevices.values()) {
        for (const auto& weakCommandBuffer : borrow(container)) {
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
        workItem();
        return;
    }
    m_scheduleWork(WTF::move(workItem));
}

const std::optional<const MachSendRight>& Instance::webProcessID() const
{
    return m_webProcessID;
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
