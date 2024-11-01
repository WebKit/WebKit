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
#import <wtf/TZoneMallocInlines.h>

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
#if !PLATFORM(WATCHOS)
        MTLCounterSampleBufferDescriptor* sampleBufferDesc = [MTLCounterSampleBufferDescriptor new];
        sampleBufferDesc.sampleCount = count;
        sampleBufferDesc.storageMode = MTLStorageModeShared;
        sampleBufferDesc.counterSet = m_capabilities.baseCapabilities.timestampCounterSet;

        NSError* error = nil;
        id<MTLCounterSampleBuffer> buffer = [m_device newCounterSampleBufferWithDescriptor:sampleBufferDesc error:&error];
        if (error)
            return QuerySet::createInvalid(*this);
        return QuerySet::create(buffer, count, type, *this);
#else
        return QuerySet::createInvalid(*this);
#endif
    } case WGPUQueryType_Occlusion: {
        auto buffer = safeCreateBuffer(sizeof(uint64_t) * count, MTLStorageModePrivate);
        buffer.label = fromAPI(label);
        return QuerySet::create(buffer, count, type, *this);
    }
    case WGPUQueryType_Force32:
        return QuerySet::createInvalid(*this);
    }
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(QuerySet);

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
    for (Ref commandEncoder : m_commandEncoders)
        commandEncoder->makeSubmitInvalid();

    m_commandEncoders.clear();
}

void QuerySet::setLabel(String&& label)
{
    m_visibilityBuffer.label = label;
    // MTLCounterSampleBuffer's label property is read-only.
}

void QuerySet::setOverrideLocation(QuerySet&, uint32_t, uint32_t)
{
}

void QuerySet::setCommandEncoder(CommandEncoder& commandEncoder) const
{
    CommandEncoder::trackEncoder(commandEncoder, m_commandEncoders);
    commandEncoder.addBuffer(m_visibilityBuffer);
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
    WebGPU::protectedFromAPI(querySet)->destroy();
}

void wgpuQuerySetSetLabel(WGPUQuerySet querySet, const char* label)
{
    WebGPU::protectedFromAPI(querySet)->setLabel(WebGPU::fromAPI(label));
}

uint32_t wgpuQuerySetGetCount(WGPUQuerySet querySet)
{
    return WebGPU::protectedFromAPI(querySet)->count();
}

WGPUQueryType wgpuQuerySetGetType(WGPUQuerySet querySet)
{
    return WebGPU::protectedFromAPI(querySet)->type();
}
