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

#import "config.h"
#import "GPURenderPipelineDescriptor.h"

#if ENABLE(WEBGPU)

#import "GPUFunction.h"
#import "GPURenderPipelineColorAttachmentDescriptor.h"
#import "Logging.h"

#import <Metal/Metal.h>

namespace WebCore {

GPURenderPipelineDescriptor::GPURenderPipelineDescriptor()
{
    LOG(WebGPU, "GPURenderPipelineDescriptor::GPURenderPipelineDescriptor()");

    m_renderPipelineDescriptor = adoptNS((MTLRenderPipelineDescriptor *)[MTLRenderPipelineDescriptor new]);
}

unsigned long GPURenderPipelineDescriptor::depthAttachmentPixelFormat() const
{
    if (!m_renderPipelineDescriptor)
        return 0;
    return [m_renderPipelineDescriptor depthAttachmentPixelFormat];
}

void GPURenderPipelineDescriptor::setDepthAttachmentPixelFormat(unsigned long newPixelFormat)
{
    ASSERT(m_renderPipelineDescriptor);
    [m_renderPipelineDescriptor setDepthAttachmentPixelFormat:static_cast<MTLPixelFormat>(newPixelFormat)];
}

void GPURenderPipelineDescriptor::setVertexFunction(RefPtr<GPUFunction> newFunction)
{
    ASSERT(m_renderPipelineDescriptor);
    [m_renderPipelineDescriptor setVertexFunction:(id<MTLFunction>)newFunction->platformFunction()];
}

void GPURenderPipelineDescriptor::setFragmentFunction(RefPtr<GPUFunction> newFunction)
{
    ASSERT(m_renderPipelineDescriptor);
    [m_renderPipelineDescriptor setFragmentFunction:(id<MTLFunction>)newFunction->platformFunction()];
}

Vector<RefPtr<GPURenderPipelineColorAttachmentDescriptor>> GPURenderPipelineDescriptor::colorAttachments()
{
    if (!m_renderPipelineDescriptor)
        return Vector<RefPtr<GPURenderPipelineColorAttachmentDescriptor>>();

    Vector<RefPtr<GPURenderPipelineColorAttachmentDescriptor>> platformColorAttachments;
    platformColorAttachments.append(GPURenderPipelineColorAttachmentDescriptor::create([[m_renderPipelineDescriptor colorAttachments] objectAtIndexedSubscript:0]));
    return platformColorAttachments;
}

void GPURenderPipelineDescriptor::reset()
{
    if (!m_renderPipelineDescriptor)
        return;

    [m_renderPipelineDescriptor reset];
}

MTLRenderPipelineDescriptor *GPURenderPipelineDescriptor::platformRenderPipelineDescriptor()
{
    return m_renderPipelineDescriptor.get();
}

} // namespace WebCore

#endif
