/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "GPURenderPassEncoder.h"

#if ENABLE(WEBGPU)

namespace WebCore {

RefPtr<GPURenderPassEncoder> GPURenderPassEncoder::tryCreate(Ref<GPUCommandBuffer>&& buffer, GPURenderPassDescriptor&& descriptor)
{
    return nullptr;
}

GPURenderPassEncoder::GPURenderPassEncoder(Ref<GPUCommandBuffer>&& commandBuffer, PlatformRenderPassEncoderSmartPtr&& encoder)
    : GPUProgrammablePassEncoder(WTFMove(commandBuffer))
    , m_platformRenderPassEncoder(WTFMove(encoder))
{
}

const PlatformProgrammablePassEncoder* GPURenderPassEncoder::platformPassEncoder() const
{
    return nullptr;
}

void GPURenderPassEncoder::setPipeline(Ref<const GPURenderPipeline>&& pipeline)
{
}

void GPURenderPassEncoder::setBlendColor(const GPUColor& color)
{
}

void GPURenderPassEncoder::setViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
}

void GPURenderPassEncoder::setScissorRect(unsigned x, unsigned y, unsigned width, unsigned height)
{
}

void GPURenderPassEncoder::setIndexBuffer(GPUBuffer& buffer, uint64_t offset)
{
}

void GPURenderPassEncoder::setVertexBuffers(unsigned index, const Vector<Ref<GPUBuffer>>& buffers, const Vector<uint64_t>& offsets)
{
}

void GPURenderPassEncoder::draw(unsigned vertexCount, unsigned instanceCount, unsigned firstVertex, unsigned firstInstance)
{
}

void GPURenderPassEncoder::drawIndexed(unsigned indexCount, unsigned instanceCount, unsigned firstIndex, int baseVertex, unsigned firstInstance)
{
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
