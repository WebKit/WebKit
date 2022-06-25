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

#pragma once

#import <wtf/FastMalloc.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>

struct WGPUQuerySetImpl {
};

namespace WebGPU {

class Device;

// https://gpuweb.github.io/gpuweb/#gpuqueryset
class QuerySet : public WGPUQuerySetImpl, public RefCounted<QuerySet> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<QuerySet> create(id<MTLCounterSampleBuffer> counterSampleBuffer, Device& device)
    {
        return adoptRef(*new QuerySet(counterSampleBuffer, device));
    }
    static Ref<QuerySet> createInvalid(Device& device)
    {
        return adoptRef(*new QuerySet(device));
    }

    ~QuerySet();

    void destroy();
    void setLabel(String&&);

    bool isValid() const { return m_counterSampleBuffer; }

    id<MTLCounterSampleBuffer> counterSampleBuffer() const { return m_counterSampleBuffer; }

    Device& device() const { return m_device; }

private:
    QuerySet(id<MTLCounterSampleBuffer>, Device&);
    QuerySet(Device&);

    id<MTLCounterSampleBuffer> m_counterSampleBuffer { nil };

    const Ref<Device> m_device;
};

} // namespace WebGPU
