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

Ref<QuerySet> Device::createQuerySet(const WGPUQuerySetDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return QuerySet::createInvalid(*this);
}

QuerySet::QuerySet(id<MTLCounterSampleBuffer> counterSampleBuffer, Device& device)
    : m_counterSampleBuffer(counterSampleBuffer)
    , m_device(device)
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

    m_counterSampleBuffer = nil;
}

void QuerySet::setLabel(String&&)
{
    // MTLCounterSampleBuffer's labels are read-only.
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
