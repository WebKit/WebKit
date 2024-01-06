/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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
#import "TextureView.h"

#import "APIConversions.h"
#import "CommandEncoder.h"

namespace WebGPU {

TextureView::TextureView(id<MTLTexture> texture, const WGPUTextureViewDescriptor& descriptor, const std::optional<WGPUExtent3D>& renderExtent, Texture& parentTexture, Device& device)
    : m_texture(texture)
    , m_descriptor(descriptor)
    , m_renderExtent(renderExtent)
    , m_device(device)
    , m_parentTexture(parentTexture)
{
}

TextureView::TextureView(Texture& texture, Device& device)
    : m_descriptor { }
    , m_device(device)
    , m_parentTexture(texture)
{
}

TextureView::~TextureView() = default;

void TextureView::setLabel(String&& label)
{
    m_texture.label = label;
}

bool TextureView::previouslyCleared() const
{
    return m_parentTexture.previouslyCleared(m_texture.parentRelativeLevel, m_texture.parentRelativeSlice);
}

void TextureView::setPreviouslyCleared()
{
    m_parentTexture.setPreviouslyCleared(m_texture.parentRelativeLevel, m_texture.parentRelativeSlice);
}

uint32_t TextureView::width() const
{
    return static_cast<uint32_t>(m_texture.width);
}

uint32_t TextureView::height() const
{
    return static_cast<uint32_t>(m_texture.height);
}

WGPUTextureUsageFlags TextureView::usage() const
{
    return m_parentTexture.usage();
}

uint32_t TextureView::sampleCount() const
{
    return m_parentTexture.sampleCount();
}

WGPUTextureFormat TextureView::format() const
{
    return m_parentTexture.format();
}

uint32_t TextureView::mipLevelCount() const
{
    return m_parentTexture.mipLevelCount();
}

bool TextureView::isDestroyed() const
{
    return m_parentTexture.isDestroyed();
}

void TextureView::destroy()
{
    m_texture = nil;
    if (m_commandEncoder)
        m_commandEncoder.get()->makeInvalid();

    m_commandEncoder = nullptr;
}

void TextureView::setCommandEncoder(CommandEncoder& commandEncoder)
{
    m_commandEncoder = &commandEncoder;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuTextureViewReference(WGPUTextureView textureView)
{
    WebGPU::fromAPI(textureView).ref();
}

void wgpuTextureViewRelease(WGPUTextureView textureView)
{
    WebGPU::fromAPI(textureView).deref();
}

void wgpuTextureViewSetLabel(WGPUTextureView textureView, const char* label)
{
    WebGPU::fromAPI(textureView).setLabel(WebGPU::fromAPI(label));
}
