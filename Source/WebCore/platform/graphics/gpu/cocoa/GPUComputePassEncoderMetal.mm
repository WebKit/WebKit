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
#include "GPUComputePassEncoder.h"

#if ENABLE(WEBGPU)

#include "GPUComputePipeline.h"
#include "Logging.h"
#include <Metal/Metal.h>
#include <wtf/BlockObjCExceptions.h>

namespace WebCore {

RefPtr<GPUComputePassEncoder> GPUComputePassEncoder::tryCreate(Ref<GPUCommandBuffer>&& buffer)
{
    if (buffer->isEncodingPass()) {
        LOG(WebGPU, "GPUComputePassEncoder::tryCreate(): Existing pass encoder must be ended first!");
        return nullptr;
    }

    buffer->endBlitEncoding();

    RetainPtr<MTLComputeCommandEncoder> mtlEncoder;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    mtlEncoder = [buffer->platformCommandBuffer() computeCommandEncoder];
    END_BLOCK_OBJC_EXCEPTIONS;

    if (!mtlEncoder) {
        LOG(WebGPU, "GPUComputePassEncoder::tryCreate(): Unable to create MTLComputeCommandEncoder!");
        return nullptr;
    }

    return adoptRef(new GPUComputePassEncoder(WTFMove(buffer), WTFMove(mtlEncoder)));
}

GPUComputePassEncoder::GPUComputePassEncoder(Ref<GPUCommandBuffer>&& buffer, RetainPtr<MTLComputeCommandEncoder>&& encoder)
    : GPUProgrammablePassEncoder(WTFMove(buffer))
    , m_platformComputePassEncoder(WTFMove(encoder))
{
}

void GPUComputePassEncoder::setPipeline(Ref<const GPUComputePipeline>&& pipeline)
{
    if (!m_platformComputePassEncoder) {
        LOG(WebGPU, "GPUComputePassEncoder::setPipeline(): Invalid operation!");
        return;
    }

    ASSERT(pipeline->platformComputePipeline());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_platformComputePassEncoder setComputePipelineState:pipeline->platformComputePipeline()];
    END_BLOCK_OBJC_EXCEPTIONS;

    m_pipeline = WTFMove(pipeline);
}

void GPUComputePassEncoder::dispatch(unsigned x, unsigned y, unsigned z)
{
    if (!m_platformComputePassEncoder) {
        LOG(WebGPU, "GPUComputePassEncoder::dispatch(): Invalid operation!");
        return;
    }

    if (!m_pipeline) {
        LOG(WebGPU, "GPUComputePassEncoder::dispatch(): No valid GPUComputePipeline found!");
        return;
    }

    ASSERT(m_pipeline->platformComputePipeline());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    auto threadsPerThreadgroup = MTLSizeMake(m_pipeline->computeDimensions().width, m_pipeline->computeDimensions().height, m_pipeline->computeDimensions().depth);

    auto threadgroupsPerGrid = MTLSizeMake(x, y, z);

    [m_platformComputePassEncoder dispatchThreadgroups:threadgroupsPerGrid threadsPerThreadgroup:threadsPerThreadgroup];

    END_BLOCK_OBJC_EXCEPTIONS;
}

const MTLCommandEncoder *GPUComputePassEncoder::platformPassEncoder() const
{
    return m_platformComputePassEncoder.get();
}

#if USE(METAL)

void GPUComputePassEncoder::useResource(const MTLResource *resource, unsigned usage)
{
    ASSERT(m_platformComputePassEncoder);

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_platformComputePassEncoder useResource:resource usage:usage];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void GPUComputePassEncoder::setComputeBuffer(const MTLBuffer *buffer, NSUInteger offset, unsigned index)
{
    ASSERT(m_platformComputePassEncoder);

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_platformComputePassEncoder setBuffer:buffer offset:offset atIndex:index];
    END_BLOCK_OBJC_EXCEPTIONS;
}

#endif

} // namespace WebCore

#endif // ENABLE(WEBGPU)
