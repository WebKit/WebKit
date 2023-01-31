/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebGPUComputePipelineImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUBindGroupLayoutImpl.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebGPU/WebGPUExt.h>

namespace PAL::WebGPU {

ComputePipelineImpl::ComputePipelineImpl(WGPUComputePipeline computePipeline, ConvertToBackingContext& convertToBackingContext)
    : m_backing(computePipeline)
    , m_convertToBackingContext(convertToBackingContext)
{
}

ComputePipelineImpl::~ComputePipelineImpl()
{
    wgpuComputePipelineRelease(m_backing);
}

Ref<BindGroupLayout> ComputePipelineImpl::getBindGroupLayout(uint32_t index)
{
    return m_bindGroupLayouts.ensure(index, [this, index] {
        return BindGroupLayoutImpl::create(wgpuComputePipelineGetBindGroupLayout(m_backing, index), m_convertToBackingContext);
    }).iterator->value;
}

void ComputePipelineImpl::setLabelInternal(const String& label)
{
    wgpuComputePipelineSetLabel(m_backing, label.utf8().data());
}

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
