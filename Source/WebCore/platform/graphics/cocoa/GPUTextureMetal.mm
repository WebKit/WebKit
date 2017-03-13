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
#import "GPUTexture.h"

#if ENABLE(WEBGPU)

#import "GPUDevice.h"
#import "GPUDrawable.h"
#import "GPUTextureDescriptor.h"
#import "Logging.h"

#import <Metal/Metal.h>

namespace WebCore {

GPUTexture::GPUTexture(GPUDevice* device, GPUTextureDescriptor* descriptor)
{
    LOG(WebGPU, "GPUTexture::GPUTexture()");

    if (!device || !device->platformDevice() || !descriptor || !descriptor->platformTextureDescriptor())
        return;

    m_texture = adoptNS((MTLTexture *)[device->platformDevice() newTextureWithDescriptor:descriptor->platformTextureDescriptor()]);
}

GPUTexture::GPUTexture(GPUDrawable* other)
{
    LOG(WebGPU, "GPUTexture::GPUTexture()");

    m_texture = other->platformTexture();
}

unsigned long GPUTexture::width() const
{
    if (!m_texture)
        return 0;

    id<MTLTexture> texture = static_cast<id<MTLTexture>>(m_texture.get());
    return texture.width;
}

unsigned long GPUTexture::height() const
{
    if (!m_texture)
        return 0;

    id<MTLTexture> texture = static_cast<id<MTLTexture>>(m_texture.get());
    return texture.height;
}

MTLTexture *GPUTexture::platformTexture()
{
    return m_texture.get();
}

} // namespace WebCore

#endif
