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
#include "WebGPURenderPassAttachmentDescriptor.h"

#if ENABLE(WEBGPU)

#include "GPURenderPassAttachmentDescriptor.h"
#include "GPUTexture.h"
#include "WebGPURenderingContext.h"
#include "WebGPUTexture.h"

namespace WebCore {

WebGPURenderPassAttachmentDescriptor::WebGPURenderPassAttachmentDescriptor(WebGPURenderingContext* context, GPURenderPassAttachmentDescriptor* descriptor)
    : WebGPUObject(context)
    , m_renderPassAttachmentDescriptor(descriptor)
{
}

WebGPURenderPassAttachmentDescriptor::~WebGPURenderPassAttachmentDescriptor() = default;

unsigned long WebGPURenderPassAttachmentDescriptor::loadAction() const
{
    if (!m_renderPassAttachmentDescriptor)
        return 0; // FIXME: probably a real value for unknown

    return m_renderPassAttachmentDescriptor->loadAction();
}

void WebGPURenderPassAttachmentDescriptor::setLoadAction(unsigned long newLoadAction)
{
    if (!m_renderPassAttachmentDescriptor)
        return;

    m_renderPassAttachmentDescriptor->setLoadAction(newLoadAction);
}

unsigned long WebGPURenderPassAttachmentDescriptor::storeAction() const
{
    if (!m_renderPassAttachmentDescriptor)
        return 0; // FIXME: probably a real value for unknown

    return m_renderPassAttachmentDescriptor->storeAction();
}

void WebGPURenderPassAttachmentDescriptor::setStoreAction(unsigned long newStoreAction)
{
    if (!m_renderPassAttachmentDescriptor)
        return;

    m_renderPassAttachmentDescriptor->setStoreAction(newStoreAction);
}

RefPtr<WebGPUTexture> WebGPURenderPassAttachmentDescriptor::texture() const
{
    return m_texture;
}

void WebGPURenderPassAttachmentDescriptor::setTexture(RefPtr<WebGPUTexture> newTexture)
{
    if (!newTexture)
        return;
    
    m_texture = newTexture;
    
    if (m_renderPassAttachmentDescriptor)
        m_renderPassAttachmentDescriptor->setTexture(newTexture->texture());
}

} // namespace WebCore

#endif
