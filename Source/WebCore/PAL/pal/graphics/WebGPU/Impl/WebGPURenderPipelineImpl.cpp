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
#include "WebGPURenderPipelineImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUBindGroupLayoutImpl.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebGPU/WebGPUExt.h>

namespace PAL::WebGPU {

RenderPipelineImpl::RenderPipelineImpl(WGPURenderPipeline renderPipeline, ConvertToBackingContext& convertToBackingContext)
    : m_backing(renderPipeline)
    , m_convertToBackingContext(convertToBackingContext)
{
}

RenderPipelineImpl::~RenderPipelineImpl()
{
    wgpuRenderPipelineRelease(m_backing);
}

Ref<BindGroupLayout> RenderPipelineImpl::getBindGroupLayout(uint32_t index)
{
    return m_bindGroupLayouts.ensure(index, [this, index] {
        return BindGroupLayoutImpl::create(wgpuRenderPipelineGetBindGroupLayout(m_backing, index), m_convertToBackingContext);
    }).iterator->value;
}

void RenderPipelineImpl::setLabelInternal(const String& label)
{
    wgpuRenderPipelineSetLabel(m_backing, label.utf8().data());
}

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
