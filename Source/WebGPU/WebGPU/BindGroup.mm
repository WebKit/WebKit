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
#import "BindGroup.h"

#import "Device.h"

namespace WebGPU {

RefPtr<BindGroup> Device::createBindGroup(const WGPUBindGroupDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return BindGroup::create(nil, nil, nil);
}

BindGroup::BindGroup(id <MTLBuffer> vertexArgumentBuffer, id <MTLBuffer> fragmentArgumentBuffer, id <MTLBuffer> computeArgumentBuffer)
    : m_vertexArgumentBuffer(vertexArgumentBuffer)
    , m_fragmentArgumentBuffer(fragmentArgumentBuffer)
    , m_computeArgumentBuffer(computeArgumentBuffer)
{
    UNUSED_VARIABLE(m_vertexArgumentBuffer);
    UNUSED_VARIABLE(m_fragmentArgumentBuffer);
    UNUSED_VARIABLE(m_computeArgumentBuffer);
}

BindGroup::~BindGroup() = default;

void BindGroup::setLabel(const char* label)
{
    UNUSED_PARAM(label);
}

}

void wgpuBindGroupRelease(WGPUBindGroup bindGroup)
{
    delete bindGroup;
}

void wgpuBindGroupSetLabel(WGPUBindGroup bindGroup, const char* label)
{
    bindGroup->bindGroup->setLabel(label);
}
