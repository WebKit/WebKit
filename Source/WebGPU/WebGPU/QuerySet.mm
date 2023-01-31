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
#import "QuerySet.h"

#import "APIConversions.h"
#import "Device.h"

namespace WebGPU {

static id<MTLCounterSet> getCounterSet(MTLCommonCounterSet counterSetName, id<MTLDevice> device)
{
    for (id<MTLCounterSet> counterSet in device.counterSets) {
        if ([counterSetName caseInsensitiveCompare:counterSetName] == NSOrderedSame)
            return counterSet;
    }

    return nil;
}

Ref<QuerySet> Device::createQuerySet(const WGPUQuerySetDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return QuerySet::createInvalid(*this);

    auto queryCount = descriptor.count;

    switch (descriptor.type) {
    case WGPUQueryType_Timestamp: {
        ASSERT(queryCount == 4);
        MTLCounterSampleBufferDescriptor *descriptor = [MTLCounterSampleBufferDescriptor new];
        descriptor.counterSet = getCounterSet(MTLCommonCounterTimestamp, m_device);
        descriptor.storageMode = MTLStorageModeShared;
        descriptor.sampleCount = queryCount;
        auto timestampBuffer = [m_device newCounterSampleBufferWithDescriptor:descriptor error:nil];
        return timestampBuffer ? QuerySet::create(timestampBuffer, *this) : QuerySet::createInvalid(*this);
    }
    case WGPUQueryType_Occlusion:
        return QuerySet::create(safeCreateBuffer(sizeof(uint64_t) * queryCount, MTLStorageModeShared), *this);
    case WGPUQueryType_PipelineStatistics:
    case WGPUQueryType_Force32:
        ASSERT_NOT_REACHED("unexpected queryType");
        return QuerySet::createInvalid(*this);
    }
}

QuerySet::QuerySet(id<MTLBuffer> buffer, Device& device)
    : m_device(device)
    , m_visibilityBuffer(buffer)
    , m_queryCount(buffer.length / sizeof(uint64_t))
{
}

QuerySet::QuerySet(id<MTLCounterSampleBuffer> buffer, Device& device)
    : m_device(device)
    , m_timestampBuffer(buffer)
    , m_queryCount(buffer.sampleCount)
{
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
}

void QuerySet::setLabel(String&& label)
{
    m_visibilityBuffer.label = label;
}

Vector<MTLTimestamp> QuerySet::resolveTimestamps() const
{
    ASSERT(m_timestampBuffer);

    NSData *resolvedData = [m_timestampBuffer resolveCounterRange:NSMakeRange(0, m_queryCount)];
    ASSERT(resolvedData);
    Vector<MTLTimestamp> timestamps;
    size_t timestampCount = resolvedData.length / sizeof(uint64_t);
    timestamps.resize(timestampCount);
    auto gpuTimestamps = static_cast<const MTLTimestamp *>(resolvedData.bytes);
    // FIXME: some devices may require mapping GPU time to CPU time, however
    // this does not appear to be the case in testing
    for (size_t i = 0; i < timestampCount; ++i)
        timestamps[i] = gpuTimestamps[i];

    return timestamps;
}

WGPUQueryType QuerySet::queryType() const
{
    return visibilityBuffer() ? WGPUQueryType_Occlusion : WGPUQueryType_Timestamp;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

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
    return WebGPU::fromAPI(querySet).queryCount();
}

WGPUQueryType wgpuQuerySetGetType(WGPUQuerySet querySet)
{
    return WebGPU::fromAPI(querySet).queryType();
}
