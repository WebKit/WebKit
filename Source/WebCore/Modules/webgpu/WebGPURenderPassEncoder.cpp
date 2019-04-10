/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "WebGPURenderPassEncoder.h"

#if ENABLE(WEBGPU)

#include "GPUColor.h"
#include "GPULimits.h"
#include "GPUProgrammablePassEncoder.h"
#include "GPURenderPassEncoder.h"
#include "Logging.h"
#include "WebGPUBuffer.h"
#include "WebGPURenderPipeline.h"

namespace WebCore {

Ref<WebGPURenderPassEncoder> WebGPURenderPassEncoder::create(RefPtr<GPURenderPassEncoder>&& encoder)
{
    return adoptRef(*new WebGPURenderPassEncoder(WTFMove(encoder)));
}

WebGPURenderPassEncoder::WebGPURenderPassEncoder(RefPtr<GPURenderPassEncoder>&& encoder)
    : m_passEncoder { WTFMove(encoder) }
{
}

void WebGPURenderPassEncoder::setPipeline(const WebGPURenderPipeline& pipeline)
{
    if (!m_passEncoder) {
        LOG(WebGPU, "GPURenderPassEncoder::setPipeline(): Invalid operation!");
        return;
    }
    if (!pipeline.renderPipeline()) {
        LOG(WebGPU, "GPURenderPassEncoder::setPipeline(): Invalid pipeline!");
        return;
    }
    m_passEncoder->setPipeline(makeRef(*pipeline.renderPipeline()));
}

void WebGPURenderPassEncoder::setBlendColor(const GPUColor& color)
{
    if (!m_passEncoder) {
        LOG(WebGPU, "GPURenderPassEncoder::setBlendColor(): Invalid operation!");
        return;
    }
    m_passEncoder->setBlendColor(color);
}

void WebGPURenderPassEncoder::setViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
    if (!m_passEncoder) {
        LOG(WebGPU, "GPURenderPassEncoder::setViewport(): Invalid operation!");
        return;
    }
    m_passEncoder->setViewport(x, y, width, height, minDepth, maxDepth);
}

void WebGPURenderPassEncoder::setScissorRect(unsigned x, unsigned y, unsigned width, unsigned height)
{
    if (!m_passEncoder) {
        LOG(WebGPU, "GPURenderPassEncoder::setScissorRect(): Invalid operation!");
        return;
    }
    if (!width || !height) {
        LOG(WebGPU, "GPURenderPassEncoder::setScissorRect(): Width or height must be greater than 0!");
        return;
    }
    m_passEncoder->setScissorRect(x, y, width, height);
}

void WebGPURenderPassEncoder::setIndexBuffer(WebGPUBuffer& buffer, uint64_t offset)
{
    if (!m_passEncoder) {
        LOG(WebGPU, "GPURenderPassEncoder::setIndexBuffer(): Invalid operation!");
        return;
    }
    if (!buffer.buffer() || !buffer.buffer()->isIndex()) {
        LOG(WebGPU, "GPURenderPassEncoder::setIndexBuffer(): Invalid GPUBuffer!");
        return;
    }

    m_passEncoder->setIndexBuffer(*buffer.buffer(), offset);
}

void WebGPURenderPassEncoder::setVertexBuffers(unsigned startSlot, const Vector<RefPtr<WebGPUBuffer>>& buffers, const Vector<uint64_t>& offsets)
{
#if !LOG_DISABLED
    const char* const functionName = "GPURenderPassEncoder::setVertexBuffers()";
#endif
    if (!m_passEncoder) {
        LOG(WebGPU, "%s: Invalid operation!", functionName);
        return;
    }
    if (buffers.isEmpty() || buffers.size() != offsets.size()) {
        LOG(WebGPU, "%s: Invalid number of buffers or offsets!", functionName);
        return;
    }
    if (startSlot + buffers.size() > maxVertexBuffers) {
        LOG(WebGPU, "%s: Invalid startSlot %u for %lu buffers!", functionName, startSlot, buffers.size());
        return;
    }

    Vector<Ref<GPUBuffer>> gpuBuffers;
    gpuBuffers.reserveCapacity(buffers.size());

    for (auto& buffer : buffers) {
        if (!buffer || !buffer->buffer()) {
            LOG(WebGPU, "%s: Invalid or destroyed buffer in list!", functionName);
            return;
        }

        if (!buffer->buffer()->isVertex()) {
            LOG(WebGPU, "%s: Buffer was not created with VERTEX usage!", functionName);
            return;
        }

        gpuBuffers.uncheckedAppend(makeRef(*buffer->buffer()));
    }

    m_passEncoder->setVertexBuffers(startSlot, gpuBuffers, offsets);
}

void WebGPURenderPassEncoder::draw(unsigned vertexCount, unsigned instanceCount, unsigned firstVertex, unsigned firstInstance)
{
    if (!m_passEncoder) {
        LOG(WebGPU, "GPURenderPassEncoder::draw(): Invalid operation!");
        return;
    }
    // FIXME: What kind of validation do we need to handle here?
    m_passEncoder->draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void WebGPURenderPassEncoder::drawIndexed(unsigned indexCount, unsigned instanceCount, unsigned firstIndex, int baseVertex, unsigned firstInstance)
{
    if (!m_passEncoder) {
        LOG(WebGPU, "GPURenderPassEncoder::draw(): Invalid operation!");
        return;
    }
    // FIXME: Add Web GPU validation.
    m_passEncoder->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

GPUProgrammablePassEncoder* WebGPURenderPassEncoder::passEncoder()
{
    return m_passEncoder.get();
}

const GPUProgrammablePassEncoder* WebGPURenderPassEncoder::passEncoder() const
{
    return m_passEncoder.get();
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
