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
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebGPUDrawable.h"

#if ENABLE(WEBGPU)

#include "GPUDrawable.h"
#include "GPUTexture.h"
#include "WebGPURenderingContext.h"
#include "WebGPUTexture.h"

namespace WebCore {

Ref<WebGPUDrawable> WebGPUDrawable::create(WebGPURenderingContext* context)
{
    return adoptRef(*new WebGPUDrawable(context));
}

WebGPUDrawable::WebGPUDrawable(WebGPURenderingContext* context)
    : WebGPUObject(context)
{
    m_drawable = context->device()->getFramebuffer();
    if (!m_drawable)
        return;

    auto drawableTexture = GPUTexture::createFromDrawable(m_drawable.get());
    m_texture = WebGPUTexture::createFromDrawableTexture(context, WTFMove(drawableTexture));
}

WebGPUDrawable::~WebGPUDrawable()
{
}

WebGPUTexture* WebGPUDrawable::texture()
{
    if (!m_texture)
        return nullptr;

    return m_texture.get();
}

} // namespace WebCore

#endif
