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
#include "WebMetalRenderPassAttachmentDescriptor.h"

#if ENABLE(WEBMETAL)

#include "GPURenderPassAttachmentDescriptor.h"
#include "GPUTexture.h"
#include "WebMetalTexture.h"

namespace WebCore {

WebMetalRenderPassAttachmentDescriptor::WebMetalRenderPassAttachmentDescriptor()
{
}

WebMetalRenderPassAttachmentDescriptor::~WebMetalRenderPassAttachmentDescriptor() = default;

unsigned WebMetalRenderPassAttachmentDescriptor::loadAction() const
{
    return descriptor().loadAction();
}

void WebMetalRenderPassAttachmentDescriptor::setLoadAction(unsigned newLoadAction)
{
    descriptor().setLoadAction(newLoadAction);
}

unsigned WebMetalRenderPassAttachmentDescriptor::storeAction() const
{
    return descriptor().storeAction();
}

void WebMetalRenderPassAttachmentDescriptor::setStoreAction(unsigned newStoreAction)
{
    descriptor().setStoreAction(newStoreAction);
}

WebMetalTexture* WebMetalRenderPassAttachmentDescriptor::texture() const
{
    return m_texture.get();
}

void WebMetalRenderPassAttachmentDescriptor::setTexture(RefPtr<WebMetalTexture>&& newTexture)
{
    // FIXME: Why can't we set this to null?
    if (!newTexture)
        return;
    
    m_texture = WTFMove(newTexture);

    descriptor().setTexture(m_texture->texture());
}

} // namespace WebCore

#endif
