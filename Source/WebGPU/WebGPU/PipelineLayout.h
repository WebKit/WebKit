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
#import <wtf/Vector.h>

struct WGPUPipelineLayoutImpl {
};

namespace WebGPU {

class BindGroupLayout;
class Device;

// https://gpuweb.github.io/gpuweb/#gpupipelinelayout
class PipelineLayout : public WGPUPipelineLayoutImpl, public RefCounted<PipelineLayout> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<PipelineLayout> create(Vector<Ref<BindGroupLayout>>&& bindGroupLayouts, Device& device)
    {
        return adoptRef(*new PipelineLayout(WTFMove(bindGroupLayouts), device));
    }
    static Ref<PipelineLayout> createInvalid(Device& device)
    {
        return adoptRef(*new PipelineLayout(device));
    }

    ~PipelineLayout();

    void setLabel(String&&);

    // FIXME: Is it impossible to legitimately have no bind group layouts in a valid object?
    bool isValid() const { return !m_bindGroupLayouts.isEmpty(); }

    bool operator==(const PipelineLayout&) const;
    bool operator!=(const PipelineLayout&) const;

    size_t numberOfBindGroupLayouts() const { return m_bindGroupLayouts.size(); }
    const BindGroupLayout& bindGroupLayout(size_t i) const { return m_bindGroupLayouts[i]; }

    Device& device() const { return m_device; }

private:
    PipelineLayout(Vector<Ref<BindGroupLayout>>&&, Device&);
    PipelineLayout(Device&);

    const Vector<Ref<BindGroupLayout>> m_bindGroupLayouts;

    const Ref<Device> m_device;
};

} // namespace WebGPU
