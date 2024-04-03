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
#import "QuerySet.h"

#import "APIConversions.h"
#import "Buffer.h"
#import "CommandEncoder.h"
#import "Device.h"

namespace WebGPU {

Ref<QuerySet> Device::createQuerySet(const WGPUQuerySetDescriptor& descriptor)
{
    auto count = descriptor.count;
    constexpr auto maxCountAllowed = 4096;
    if (descriptor.nextInChain || count > maxCountAllowed || !isValid()) {
        generateAValidationError("GPUQuerySetDescriptor.count must be <= 4096"_s);
        return QuerySet::createInvalid(*this);
    }

    const char* label = descriptor.label;
    auto type = descriptor.type;

    switch (type) {
    case WGPUQueryType_Timestamp: {
        if (!hasFeature(WGPUFeatureName_TimestampQuery))
            return QuerySet::createInvalid(*this);

        ASSERT(baseCapabilities().timestampCounterSet);
        MTLCounterSampleBufferDescriptor *descriptor = [MTLCounterSampleBufferDescriptor new];
        descriptor.counterSet = baseCapabilities().timestampCounterSet;
        descriptor.label = fromAPI(label);
        descriptor.storageMode = MTLStorageModePrivate;
        descriptor.sampleCount = count;
        NSError *error;
        auto timestampBuffer = [m_device newCounterSampleBufferWithDescriptor:descriptor error:&error];
        if (error)
            WTFLogAlways("GPUDevice.createQuerySet failed: newCounterSampleBufferWithDescriptor: descriptor.counterSet %@, count %d, error %@", descriptor.counterSet, descriptor.sampleCount, error);
        return timestampBuffer ? QuerySet::create(timestampBuffer, count, type, *this) : QuerySet::createInvalid(*this);
    }
    case WGPUQueryType_Occlusion: {
        auto buffer = safeCreateBuffer(sizeof(uint64_t) * count, MTLStorageModePrivate);
        buffer.label = fromAPI(label);
        return QuerySet::create(buffer, count, type, *this);
    }
    case WGPUQueryType_Force32:
        return QuerySet::createInvalid(*this);
    }
}

QuerySet::QuerySet(id<MTLBuffer> buffer, uint32_t count, WGPUQueryType type, Device& device)
    : m_device(device)
    , m_visibilityBuffer(buffer)
    , m_count(count)
    , m_type(type)
{
    RELEASE_ASSERT(m_type != WGPUQueryType_Force32);
}

QuerySet::QuerySet(id<MTLCounterSampleBuffer> buffer, uint32_t count, WGPUQueryType type, Device& device)
    : m_device(device)
    , m_timestampBuffer(buffer)
    , m_count(count)
    , m_type(type)
{
    RELEASE_ASSERT(m_type != WGPUQueryType_Force32);
    if (m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::StageBoundary)
        m_overrideLocations = Vector<std::optional<OverrideLocation>>(m_count);
}

QuerySet::QuerySet(Device& device)
    : m_device(device)
    , m_type(WGPUQueryType_Force32)
{
}

QuerySet::~QuerySet() = default;

bool QuerySet::isValid() const
{
    return isDestroyed() || m_visibilityBuffer || m_timestampBuffer;
}

bool QuerySet::isDestroyed() const
{
    return m_destroyed;
}

void QuerySet::destroy()
{
    m_destroyed = true;
    // https://gpuweb.github.io/gpuweb/#dom-gpuqueryset-destroy
    m_visibilityBuffer = nil;
    m_timestampBuffer = nil;
    m_overrideLocations.clear();
    if (m_cachedCommandEncoder)
        m_cachedCommandEncoder.get()->makeSubmitInvalid();
    m_cachedCommandEncoder = nullptr;
}

void QuerySet::setLabel(String&& label)
{
    m_visibilityBuffer.label = label;
    // MTLCounterSampleBuffer's label property is read-only.
}

void QuerySet::setOverrideLocation(QuerySet& otherQuerySet, uint32_t beginningOfPassIndex, uint32_t endOfPassIndex)
{
    ASSERT(m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::StageBoundary);
    ASSERT(m_overrideLocations.size() == m_count);

    m_overrideLocations[beginningOfPassIndex] = { { otherQuerySet, endOfPassIndex } };
}

void QuerySet::encodeResolveCommands(id<MTLBlitCommandEncoder> commandEncoder, uint32_t firstQuery, uint32_t queryCount, const Buffer& destination, uint64_t destinationOffset) const
{
    if (!queryCount)
        return;

    auto encode = [&](id<MTLCounterSampleBuffer> counterSampleBuffer, uint32_t indexIntoDestinationBuffer) {
        constexpr auto countersToResolve = 2;
        RELEASE_ASSERT(counterSampleBuffer.sampleCount >= countersToResolve);
        [commandEncoder resolveCounters:counterSampleBuffer inRange:NSMakeRange(0, countersToResolve) destinationBuffer:destination.buffer() destinationOffset:destinationOffset + sizeof(MTLCounterResultTimestamp) * indexIntoDestinationBuffer];
    };

    struct State {
        const QuerySet* querySet;
        uint32_t index;
    };

    auto getState = [&](uint32_t queryIndex) -> std::optional<State> {
        std::optional<State> result;
        if (const auto& overrideLocation = m_overrideLocations[queryIndex]) {
            RELEASE_ASSERT(overrideLocation->otherIndex == queryIndex + 1);
            result = { overrideLocation->other.ptr(), queryIndex };
        }
        return result;
    };

    for (uint32_t i = firstQuery; i < firstQuery + queryCount; ++i) {
        auto state = getState(i);
        if (!state)
            continue;

        encode(state->querySet->counterSampleBuffer(), state->index);
    }
}

void QuerySet::setCommandEncoder(CommandEncoder& commandEncoder) const
{
    m_cachedCommandEncoder = commandEncoder;
    if (isDestroyed())
        commandEncoder.makeSubmitInvalid();
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuQuerySetReference(WGPUQuerySet querySet)
{
    WebGPU::fromAPI(querySet).ref();
}

void wgpuQuerySetRelease(WGPUQuerySet querySet)
{
    WebGPU::fromAPI(querySet).deref();
}

void wgpuQuerySetDestroy(WGPUQuerySet querySet)
{
    WebGPU::fromAPI(querySet).destroy();
}

void wgpuQuerySetSetLabel(WGPUQuerySet querySet, const char* label)
{
    WebGPU::fromAPI(querySet).setLabel(WebGPU::fromAPI(label));
}

uint32_t wgpuQuerySetGetCount(WGPUQuerySet querySet)
{
    return WebGPU::fromAPI(querySet).count();
}

WGPUQueryType wgpuQuerySetGetType(WGPUQuerySet querySet)
{
    return WebGPU::fromAPI(querySet).type();
}
