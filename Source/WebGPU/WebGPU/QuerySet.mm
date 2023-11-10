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
#import "Device.h"

namespace WebGPU {

Ref<QuerySet> Device::createQuerySet(const WGPUQuerySetDescriptor& descriptor)
{
    if (descriptor.nextInChain || !isValid())
        return QuerySet::createInvalid(*this);

    auto count = descriptor.count;
    const char* label = descriptor.label;
    auto type = descriptor.type;

    switch (type) {
    case WGPUQueryType_Timestamp: {
        if (!std::binary_search(features().begin(), features().end(), WGPUFeatureName_TimestampQuery))
            return QuerySet::createInvalid(*this);

        ASSERT(baseCapabilities().timestampCounterSet);
        MTLCounterSampleBufferDescriptor *descriptor = [MTLCounterSampleBufferDescriptor new];
        descriptor.counterSet = baseCapabilities().timestampCounterSet;
        descriptor.label = fromAPI(label);
        descriptor.storageMode = MTLStorageModePrivate;
        descriptor.sampleCount = count;
        auto timestampBuffer = [m_device newCounterSampleBufferWithDescriptor:descriptor error:nil];
        return timestampBuffer ? QuerySet::create(timestampBuffer, count, type, *this) : QuerySet::createInvalid(*this);
    }
    case WGPUQueryType_Occlusion: {
        auto buffer = safeCreateBuffer(sizeof(uint64_t) * count, MTLStorageModePrivate);
        buffer.label = fromAPI(label);
        return QuerySet::create(buffer, count, type, *this);
    }
    case WGPUQueryType_PipelineStatistics:
        // FIXME: Implement pipeline statistics query sets.
        return QuerySet::createInvalid(*this);
    case WGPUQueryType_Force32:
        ASSERT_NOT_REACHED("unexpected queryType");
        return QuerySet::createInvalid(*this);
    }
}

QuerySet::QuerySet(id<MTLBuffer> buffer, uint32_t count, WGPUQueryType type, Device& device)
    : m_device(device)
    , m_visibilityBuffer(buffer)
    , m_count(count)
    , m_type(type)
{
}

QuerySet::QuerySet(id<MTLCounterSampleBuffer> buffer, uint32_t count, WGPUQueryType type, Device& device)
    : m_device(device)
    , m_timestampBuffer(buffer)
    , m_count(count)
    , m_type(type)
{
    if (m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::StageBoundary)
        m_overrideLocations = Vector<std::optional<OverrideLocation>>(m_count);
}

QuerySet::QuerySet(Device& device)
    : m_device(device)
{
}

QuerySet::~QuerySet() = default;

void QuerySet::destroy()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpuqueryset-destroy
    m_visibilityBuffer = nil;
    m_timestampBuffer = nil;
    m_overrideLocations.clear();
}

void QuerySet::setLabel(String&& label)
{
    m_visibilityBuffer.label = label;
    // MTLCounterSampleBuffer's label property is read-only.
}

void QuerySet::setOverrideLocation(uint32_t myIndex, QuerySet& otherQuerySet, uint32_t otherIndex)
{
    ASSERT(m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::StageBoundary);
    ASSERT(m_overrideLocations.size() == m_count);
    ASSERT(myIndex < m_overrideLocations.size());

    m_overrideLocations[myIndex] = { { otherQuerySet, otherIndex } };
}

void QuerySet::encodeResolveCommands(id<MTLBlitCommandEncoder> commandEncoder, uint32_t firstQuery, uint32_t queryCount, const Buffer& destination, uint64_t destinationOffset) const
{
    if (!queryCount)
        return;

    auto encode = [&](id<MTLCounterSampleBuffer> counterSampleBuffer, uint32_t localFirstQuery, uint32_t localQueryCount) {
        ASSERT(localQueryCount);
        [commandEncoder resolveCounters:counterSampleBuffer inRange:NSMakeRange(localFirstQuery, localQueryCount) destinationBuffer:destination.buffer() destinationOffset:destinationOffset + sizeof(MTLCounterResultTimestamp) * (localFirstQuery - firstQuery)];
    };

    struct State {
        const QuerySet* querySet;
        uint32_t index;
    };

    auto getState = [&](uint32_t queryIndex) -> State {
        if (const auto& overrideLocation = m_overrideLocations[queryIndex])
            return { overrideLocation->other.ptr(), overrideLocation->otherIndex };
        return { this, queryIndex };
    };

    auto isSuccessive = [](const State& before, const State& after) {
        return before.querySet == after.querySet && before.index + 1 == after.index;
    };

    auto state = getState(firstQuery);
    auto initialState = state;
    uint32_t lastBoundary = firstQuery;
    for (uint32_t i = firstQuery + 1; i < firstQuery + queryCount; ++i) {
        auto currentState = getState(i);
        if (isSuccessive(state, currentState)) {
            state = currentState;
            continue;
        }
        encode(initialState.querySet->counterSampleBuffer(), initialState.index, i - lastBoundary);
        initialState = currentState;
        lastBoundary = i;
    }
    encode(state.querySet->counterSampleBuffer(), initialState.index, firstQuery + queryCount - lastBoundary);
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
