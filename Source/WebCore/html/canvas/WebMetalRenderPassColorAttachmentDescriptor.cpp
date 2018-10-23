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
#include "WebMetalRenderPassColorAttachmentDescriptor.h"

#if ENABLE(WEBMETAL)

#include "GPULegacyTexture.h"
#include "WebMetalTexture.h"
#include <wtf/Vector.h>

namespace WebCore {

Ref<WebMetalRenderPassColorAttachmentDescriptor> WebMetalRenderPassColorAttachmentDescriptor::create(GPULegacyRenderPassColorAttachmentDescriptor&& descriptor)
{
    return adoptRef(*new WebMetalRenderPassColorAttachmentDescriptor(WTFMove(descriptor)));
}

WebMetalRenderPassColorAttachmentDescriptor::WebMetalRenderPassColorAttachmentDescriptor(GPULegacyRenderPassColorAttachmentDescriptor&& descriptor)
    : m_descriptor { WTFMove(descriptor) }
{
}

WebMetalRenderPassColorAttachmentDescriptor::~WebMetalRenderPassColorAttachmentDescriptor() = default;

const GPULegacyRenderPassAttachmentDescriptor& WebMetalRenderPassColorAttachmentDescriptor::descriptor() const
{
    return m_descriptor;
}

Vector<float> WebMetalRenderPassColorAttachmentDescriptor::clearColor() const
{
    return m_descriptor.clearColor();
}

void WebMetalRenderPassColorAttachmentDescriptor::setClearColor(const Vector<float>& newClearColor)
{
    m_descriptor.setClearColor(newClearColor);
}

} // namespace WebCore

#endif
