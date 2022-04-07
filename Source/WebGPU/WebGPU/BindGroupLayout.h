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

struct WGPUBindGroupLayoutImpl {
};

namespace WebGPU {

class Device;

// https://gpuweb.github.io/gpuweb/#gpubindgrouplayout
class BindGroupLayout : public WGPUBindGroupLayoutImpl, public RefCounted<BindGroupLayout> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<BindGroupLayout> create(id<MTLArgumentEncoder> vertexArgumentEncoder, id<MTLArgumentEncoder> fragmentArgumentEncoder, id<MTLArgumentEncoder> computeArgumentEncoder, Device& device)
    {
        return adoptRef(*new BindGroupLayout(vertexArgumentEncoder, fragmentArgumentEncoder, computeArgumentEncoder, device));
    }
    static Ref<BindGroupLayout> createInvalid(Device& device)
    {
        return adoptRef(*new BindGroupLayout(device));
    }

    ~BindGroupLayout();

    void setLabel(String&&);

    bool isValid() const { return m_vertexArgumentEncoder || m_fragmentArgumentEncoder || m_computeArgumentEncoder; }

    NSUInteger encodedLength() const;

    id<MTLArgumentEncoder> vertexArgumentEncoder() const { return m_vertexArgumentEncoder; }
    id<MTLArgumentEncoder> fragmentArgumentEncoder() const { return m_fragmentArgumentEncoder; }
    id<MTLArgumentEncoder> computeArgumentEncoder() const { return m_computeArgumentEncoder; }

    Device& device() const { return m_device; }

private:
    BindGroupLayout(id<MTLArgumentEncoder> vertexArgumentEncoder, id<MTLArgumentEncoder> fragmentArgumentEncoder, id<MTLArgumentEncoder> computeArgumentEncoder, Device&);
    BindGroupLayout(Device&);

    const id<MTLArgumentEncoder> m_vertexArgumentEncoder { nil };
    const id<MTLArgumentEncoder> m_fragmentArgumentEncoder { nil };
    const id<MTLArgumentEncoder> m_computeArgumentEncoder { nil };

    const Ref<Device> m_device;
};

} // namespace WebGPU
