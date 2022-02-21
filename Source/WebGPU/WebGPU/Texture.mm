/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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
#import "Texture.h"

#import "Device.h"
#import "TextureView.h"
#import "WebGPUExt.h"

namespace WebGPU {

RefPtr<Texture> Device::createTexture(const WGPUTextureDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return Texture::create(nil);
}

Texture::Texture(id <MTLTexture> texture)
    : m_texture(texture)
{
    UNUSED_VARIABLE(m_texture);
}

Texture::~Texture() = default;

RefPtr<TextureView> Texture::createView(const WGPUTextureViewDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return TextureView::create(nil);
}

void Texture::destroy()
{
}

void Texture::setLabel(const char* label)
{
    UNUSED_PARAM(label);
}

} // namespace WebGPU

void wgpuTextureRelease(WGPUTexture texture)
{
    delete texture;
}

WGPUTextureView wgpuTextureCreateView(WGPUTexture texture, const WGPUTextureViewDescriptor* descriptor)
{
    auto result = texture->texture->createView(descriptor);
    return result ? new WGPUTextureViewImpl { result.releaseNonNull() } : nullptr;
}

void wgpuTextureDestroy(WGPUTexture texture)
{
    texture->texture->destroy();
}

void wgpuTextureSetLabel(WGPUTexture texture, const char* label)
{
    texture->texture->setLabel(label);
}
