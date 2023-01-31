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

struct WGPUSamplerImpl {
};

namespace WebGPU {

class Device;

// https://gpuweb.github.io/gpuweb/#gpusampler
class Sampler : public WGPUSamplerImpl, public RefCounted<Sampler> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<Sampler> create(id<MTLSamplerState> samplerState, const WGPUSamplerDescriptor& descriptor, Device& device)
    {
        return adoptRef(*new Sampler(samplerState, descriptor, device));
    }
    static Ref<Sampler> createInvalid(Device& device)
    {
        return adoptRef(*new Sampler(device));
    }

    ~Sampler();

    void setLabel(String&&);

    bool isValid() const { return m_samplerState; }

    id<MTLSamplerState> samplerState() const { return m_samplerState; }
    const WGPUSamplerDescriptor& descriptor() const { return m_descriptor; }
    bool isComparison() const { return descriptor().compare != WGPUCompareFunction_Undefined; }
    bool isFiltering() const { return descriptor().minFilter == WGPUFilterMode_Linear || descriptor().magFilter == WGPUFilterMode_Linear || descriptor().mipmapFilter == WGPUMipmapFilterMode_Linear; }

    Device& device() const { return m_device; }

private:
    Sampler(id<MTLSamplerState>, const WGPUSamplerDescriptor&, Device&);
    Sampler(Device&);

    const id<MTLSamplerState> m_samplerState { nil };

    const WGPUSamplerDescriptor m_descriptor { };

    const Ref<Device> m_device;
};

} // namespace WebGPU
