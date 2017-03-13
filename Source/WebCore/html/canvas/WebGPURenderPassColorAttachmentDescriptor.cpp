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
#include "WebGPURenderPassColorAttachmentDescriptor.h"

#if ENABLE(WEBGPU)

#include "GPURenderPassColorAttachmentDescriptor.h"
#include "GPUTexture.h"
#include "WebGPURenderingContext.h"
#include "WebGPUTexture.h"

namespace WebCore {

Ref<WebGPURenderPassColorAttachmentDescriptor> WebGPURenderPassColorAttachmentDescriptor::create(WebGPURenderingContext* context, GPURenderPassColorAttachmentDescriptor* descriptor)
{
    return adoptRef(*new WebGPURenderPassColorAttachmentDescriptor(context, descriptor));
}

WebGPURenderPassColorAttachmentDescriptor::WebGPURenderPassColorAttachmentDescriptor(WebGPURenderingContext* context, GPURenderPassColorAttachmentDescriptor* descriptor)
    : WebGPURenderPassAttachmentDescriptor(context, descriptor)
{
}

WebGPURenderPassColorAttachmentDescriptor::~WebGPURenderPassColorAttachmentDescriptor()
{
}

GPURenderPassColorAttachmentDescriptor* WebGPURenderPassColorAttachmentDescriptor::renderPassColorAttachmentDescriptor() const
{
    return static_cast<GPURenderPassColorAttachmentDescriptor*>(renderPassAttachmentDescriptor());
}

Vector<float> WebGPURenderPassColorAttachmentDescriptor::clearColor() const
{
    GPURenderPassColorAttachmentDescriptor* descriptor = renderPassColorAttachmentDescriptor();
    if (!descriptor) {
        Vector<float> black = { 0.0, 0.0, 0.0, 1.0 };
        return black;
    }
    return descriptor->clearColor();
}

void WebGPURenderPassColorAttachmentDescriptor::setClearColor(const Vector<float>& newClearColor)
{
    GPURenderPassColorAttachmentDescriptor* descriptor = renderPassColorAttachmentDescriptor();
    if (!descriptor)
        return;

    descriptor->setClearColor(newClearColor);
}

} // namespace WebCore

#endif
