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
#import "GPULegacyRenderPipelineDescriptor.h"

#if ENABLE(WEBMETAL)

#import "GPULegacyFunction.h"
#import "GPULegacyRenderPipelineColorAttachmentDescriptor.h"
#import "Logging.h"
#import <Metal/Metal.h>
#import <wtf/Vector.h>

namespace WebCore {

GPULegacyRenderPipelineDescriptor::GPULegacyRenderPipelineDescriptor()
    : m_metal { adoptNS([MTLRenderPipelineDescriptor new]) }
{
    LOG(WebMetal, "GPURenderPipelineDescriptor::GPURenderPipelineDescriptor()");
}

unsigned GPULegacyRenderPipelineDescriptor::depthAttachmentPixelFormat() const
{
    return [m_metal depthAttachmentPixelFormat];
}

void GPULegacyRenderPipelineDescriptor::setDepthAttachmentPixelFormat(unsigned newPixelFormat) const
{
    [m_metal setDepthAttachmentPixelFormat:static_cast<MTLPixelFormat>(newPixelFormat)];
}

void GPULegacyRenderPipelineDescriptor::setVertexFunction(const GPULegacyFunction& newFunction) const
{
    [m_metal setVertexFunction:newFunction.metal()];
}

void GPULegacyRenderPipelineDescriptor::setFragmentFunction(const GPULegacyFunction& newFunction) const
{
    [m_metal setFragmentFunction:newFunction.metal()];
}

Vector<GPULegacyRenderPipelineColorAttachmentDescriptor> GPULegacyRenderPipelineDescriptor::colorAttachments() const
{
    // FIXME: Why is it always OK to return exactly one attachment?
    return { { [[m_metal colorAttachments] objectAtIndexedSubscript:0] } };
}

void GPULegacyRenderPipelineDescriptor::reset() const
{
    [m_metal reset];
}

MTLRenderPipelineDescriptor *GPULegacyRenderPipelineDescriptor::metal() const
{
    return m_metal.get();
}

} // namespace WebCore

#endif
