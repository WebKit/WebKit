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
#import "GPURenderPassDepthAttachmentDescriptor.h"

#if ENABLE(WEBGPU)

#import "GPURenderPassAttachmentDescriptor.h"
#import "Logging.h"

#import <Metal/Metal.h>

namespace WebCore {

RefPtr<GPURenderPassDepthAttachmentDescriptor> GPURenderPassDepthAttachmentDescriptor::create(MTLRenderPassDepthAttachmentDescriptor *depthAttachmentDescriptor)
{
    RefPtr<GPURenderPassDepthAttachmentDescriptor> descriptor = adoptRef(new GPURenderPassDepthAttachmentDescriptor(depthAttachmentDescriptor));
    return descriptor;
}

GPURenderPassDepthAttachmentDescriptor::GPURenderPassDepthAttachmentDescriptor(MTLRenderPassDepthAttachmentDescriptor *attachmentDescriptor)
    : GPURenderPassAttachmentDescriptor(attachmentDescriptor)
{
    LOG(WebGPU, "GPURenderPassDepthAttachmentDescriptor::GPURenderPassDepthAttachmentDescriptor()");
}

double GPURenderPassDepthAttachmentDescriptor::clearDepth() const
{
    MTLRenderPassDepthAttachmentDescriptor *depthAttachmentDescriptor = static_cast<MTLRenderPassDepthAttachmentDescriptor *>(m_renderPassAttachmentDescriptor.get());
    if (!depthAttachmentDescriptor)
        return 0;

    return [depthAttachmentDescriptor clearDepth];
}

void GPURenderPassDepthAttachmentDescriptor::setClearDepth(double newClearDepth)
{
    MTLRenderPassDepthAttachmentDescriptor *depthAttachmentDescriptor = platformRenderPassDepthAttachmentDescriptor();
    [depthAttachmentDescriptor setClearDepth:newClearDepth];
}

MTLRenderPassDepthAttachmentDescriptor *GPURenderPassDepthAttachmentDescriptor::platformRenderPassDepthAttachmentDescriptor()
{
    return static_cast<MTLRenderPassDepthAttachmentDescriptor *>(platformRenderPassAttachmentDescriptor());
}

} // namespace WebCore

#endif
