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
#include "WebGPUTexture.h"

#if ENABLE(WEBGPU)

#include "GPUTexture.h"
#include "WebGPURenderingContext.h"
#include "WebGPUTextureDescriptor.h"

namespace WebCore {

Ref<WebGPUTexture> WebGPUTexture::createFromDrawableTexture(WebGPURenderingContext* context, RefPtr<GPUTexture>&& drawableTexture)
{
    return adoptRef(*new WebGPUTexture(context, WTFMove(drawableTexture)));
}

Ref<WebGPUTexture> WebGPUTexture::create(WebGPURenderingContext* context, WebGPUTextureDescriptor* descriptor)
{
    return adoptRef(*new WebGPUTexture(context, descriptor));
}

WebGPUTexture::WebGPUTexture(WebGPURenderingContext* context, RefPtr<GPUTexture>&& drawableTexture)
    : WebGPUObject(context)
    , m_texture(WTFMove(drawableTexture))
{
}

WebGPUTexture::WebGPUTexture(WebGPURenderingContext* context, WebGPUTextureDescriptor* descriptor)
    : WebGPUObject(context)
{
    m_texture = context->device()->createTexture(descriptor->textureDescriptor());
}

WebGPUTexture::~WebGPUTexture()
{
}

unsigned long WebGPUTexture::width() const
{
    if (!m_texture)
        return 0;
    
    return m_texture->width();
}

unsigned long WebGPUTexture::height() const
{
    if (!m_texture)
        return 0;

    return m_texture->height();
}

} // namespace WebCore

#endif
