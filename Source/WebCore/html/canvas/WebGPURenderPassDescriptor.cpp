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
#include "WebGPURenderPassDescriptor.h"

#if ENABLE(WEBGPU)

#include "GPURenderPassColorAttachmentDescriptor.h"
#include "GPURenderPassDepthAttachmentDescriptor.h"
#include "GPURenderPassDescriptor.h"
#include "WebGPURenderPassColorAttachmentDescriptor.h"
#include "WebGPURenderPassDepthAttachmentDescriptor.h"
#include "WebGPURenderingContext.h"

namespace WebCore {

Ref<WebGPURenderPassDescriptor> WebGPURenderPassDescriptor::create()
{
    return adoptRef(*new WebGPURenderPassDescriptor());
}

WebGPURenderPassDescriptor::WebGPURenderPassDescriptor()
    : WebGPUObject()
{
    m_renderPassDescriptor = GPURenderPassDescriptor::create();
}

WebGPURenderPassDescriptor::~WebGPURenderPassDescriptor() = default;

RefPtr<WebGPURenderPassDepthAttachmentDescriptor> WebGPURenderPassDescriptor::depthAttachment()
{
    if (!m_renderPassDescriptor)
        return nullptr;

    if (!m_depthAttachmentDescriptor) {
        RefPtr<GPURenderPassDepthAttachmentDescriptor> platformDepthAttachment = m_renderPassDescriptor->depthAttachment();
        m_depthAttachmentDescriptor = WebGPURenderPassDepthAttachmentDescriptor::create(this->context(), platformDepthAttachment.get());
    }
    return m_depthAttachmentDescriptor;
}

Vector<RefPtr<WebGPURenderPassColorAttachmentDescriptor>> WebGPURenderPassDescriptor::colorAttachments()
{
    if (!m_renderPassDescriptor)
        return Vector<RefPtr<WebGPURenderPassColorAttachmentDescriptor>>();

    if (!m_colorAttachmentDescriptors.size()) {
        Vector<RefPtr<GPURenderPassColorAttachmentDescriptor>> platformColorAttachments = m_renderPassDescriptor->colorAttachments();
        for (auto& attachment : platformColorAttachments)
            m_colorAttachmentDescriptors.append(WebGPURenderPassColorAttachmentDescriptor::create(this->context(), attachment.get()));
    }
    return m_colorAttachmentDescriptors;
}

} // namespace WebCore

#endif
