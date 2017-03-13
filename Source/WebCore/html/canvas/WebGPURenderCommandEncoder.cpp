/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebGPURenderCommandEncoder.h"

#if ENABLE(WEBGPU)

#include "GPUCommandBuffer.h"
#include "GPURenderCommandEncoder.h"
#include "GPURenderPassDescriptor.h"
#include "WebGPUBuffer.h"
#include "WebGPUCommandBuffer.h"
#include "WebGPUDepthStencilState.h"
#include "WebGPURenderPassDescriptor.h"
#include "WebGPURenderPipelineState.h"
#include "WebGPURenderingContext.h"

namespace WebCore {

Ref<WebGPURenderCommandEncoder> WebGPURenderCommandEncoder::create(WebGPURenderingContext* context, WebGPUCommandBuffer* buffer, WebGPURenderPassDescriptor* descriptor)
{
    return adoptRef(*new WebGPURenderCommandEncoder(context, buffer, descriptor));
}

WebGPURenderCommandEncoder::WebGPURenderCommandEncoder(WebGPURenderingContext* context, WebGPUCommandBuffer* buffer, WebGPURenderPassDescriptor* descriptor)
    : WebGPUObject(context)
{
    m_renderCommandEncoder = buffer->commandBuffer()->createRenderCommandEncoder(descriptor->renderPassDescriptor());
}

WebGPURenderCommandEncoder::~WebGPURenderCommandEncoder()
{
}

void WebGPURenderCommandEncoder::setRenderPipelineState(WebGPURenderPipelineState& pipelineState)
{
    if (!m_renderCommandEncoder)
        return;

    m_renderCommandEncoder->setRenderPipelineState(pipelineState.renderPipelineState());
}

void WebGPURenderCommandEncoder::setDepthStencilState(WebGPUDepthStencilState& depthStencilState)
{
    if (!m_renderCommandEncoder)
        return;

    m_renderCommandEncoder->setDepthStencilState(depthStencilState.depthStencilState());
}

void WebGPURenderCommandEncoder::setVertexBuffer(WebGPUBuffer& buffer, unsigned offset, unsigned index)
{
    if (!m_renderCommandEncoder)
        return;

    m_renderCommandEncoder->setVertexBuffer(buffer.buffer(), offset, index);
}

void WebGPURenderCommandEncoder::setFragmentBuffer(WebGPUBuffer& buffer, unsigned offset, unsigned index)
{
    if (!m_renderCommandEncoder)
        return;

    m_renderCommandEncoder->setFragmentBuffer(buffer.buffer(), offset, index);
}

void WebGPURenderCommandEncoder::drawPrimitives(unsigned type, unsigned start, unsigned count)
{
    if (!m_renderCommandEncoder)
        return;

    m_renderCommandEncoder->drawPrimitives(type, start, count);
}

void WebGPURenderCommandEncoder::endEncoding()
{
    if (!m_renderCommandEncoder)
        return;
    return m_renderCommandEncoder->endEncoding();
}

} // namespace WebCore

#endif
