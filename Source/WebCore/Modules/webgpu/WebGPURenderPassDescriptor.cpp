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
#include "WebGPURenderPassDescriptor.h"

#if ENABLE(WEBGPU)

#include "GPURenderPassDescriptor.h"
#include "Logging.h"

namespace WebCore {

GPURenderPassColorAttachmentDescriptor::GPURenderPassColorAttachmentDescriptor(Ref<GPUTexture>&& texture, const GPURenderPassColorAttachmentDescriptorBase& base)
    : GPURenderPassColorAttachmentDescriptorBase(base)
    , attachment(WTFMove(texture))
{
}

GPURenderPassDepthStencilAttachmentDescriptor::GPURenderPassDepthStencilAttachmentDescriptor(Ref<GPUTexture>&& texture, const GPURenderPassDepthStencilAttachmentDescriptorBase& base)
    : GPURenderPassDepthStencilAttachmentDescriptorBase(base)
    , attachment(WTFMove(texture))
{
}

Optional<GPURenderPassDescriptor> WebGPURenderPassDescriptor::tryCreateGPURenderPassDescriptor() const
{
    // FIXME: Improve error checking as WebGPURenderPassDescriptor is added to spec.
    if (colorAttachments.isEmpty()) {
        LOG(WebGPU, "GPURenderPassDescriptor: No color attachments specified for GPURenderPassDescriptor!");
        return WTF::nullopt;
    }

    Vector<GPURenderPassColorAttachmentDescriptor> gpuColorAttachments;

    for (const auto& colorAttachment : colorAttachments) {
        if (!colorAttachment.attachment
            || !colorAttachment.attachment->texture()
            || !colorAttachment.attachment->texture()->isOutputAttachment()) {
            LOG(WebGPU, "GPURenderPassDescriptor: Invalid attachment in GPURenderPassColorAttachmentDescriptor!");
            return WTF::nullopt;
        }
        gpuColorAttachments.append(GPURenderPassColorAttachmentDescriptor { makeRef(*colorAttachment.attachment->texture()), colorAttachment });
    }

    Optional<GPURenderPassDepthStencilAttachmentDescriptor> gpuDepthAttachment;

    if (depthStencilAttachment) {
        if (!depthStencilAttachment->attachment
            || !depthStencilAttachment->attachment->texture()
            || !depthStencilAttachment->attachment->texture()->isOutputAttachment()) {
            LOG(WebGPU, "GPURenderPassDescriptor: Invalid attachment in GPURenderPassDepthStencilAttachmentDescriptor!");
            return WTF::nullopt;
        }
        gpuDepthAttachment = GPURenderPassDepthStencilAttachmentDescriptor { makeRef(*depthStencilAttachment->attachment->texture()), *depthStencilAttachment };
    }

    return GPURenderPassDescriptor { WTFMove(gpuColorAttachments), WTFMove(gpuDepthAttachment) };
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
