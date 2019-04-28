/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WebGPUComputePassEncoder.h"

#if ENABLE(WEBGPU)

#include "GPUComputePassEncoder.h"
#include "GPUProgrammablePassEncoder.h"
#include "Logging.h"
#include "WebGPUComputePipeline.h"

namespace WebCore {

Ref<WebGPUComputePassEncoder> WebGPUComputePassEncoder::create(RefPtr<GPUComputePassEncoder>&& encoder)
{
    return adoptRef(*new WebGPUComputePassEncoder(WTFMove(encoder)));
}

WebGPUComputePassEncoder::WebGPUComputePassEncoder(RefPtr<GPUComputePassEncoder>&& encoder)
    : m_passEncoder { WTFMove(encoder) }
{
}

void WebGPUComputePassEncoder::setPipeline(const WebGPUComputePipeline& pipeline)
{
    if (!m_passEncoder) {
        LOG(WebGPU, "GPUComputePassEncoder::setPipeline(): Invalid operation!");
        return;
    }
    if (!pipeline.computePipeline()) {
        LOG(WebGPU, "GPUComputePassEncoder::setPipeline(): Invalid pipeline!");
        return;
    }
    m_passEncoder->setPipeline(makeRef(*pipeline.computePipeline()));
}

void WebGPUComputePassEncoder::dispatch(unsigned x, unsigned y, unsigned z)
{
    if (!m_passEncoder) {
        LOG(WebGPU, "GPUComputePassEncoder::dispatch(): Invalid operation!");
        return;
    }

    // FIXME: Add Web GPU validation.
    m_passEncoder->dispatch(x, y, z);
}

GPUProgrammablePassEncoder* WebGPUComputePassEncoder::passEncoder()
{
    return m_passEncoder.get();
}

const GPUProgrammablePassEncoder* WebGPUComputePassEncoder::passEncoder() const
{
    return m_passEncoder.get();
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
