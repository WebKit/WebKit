/*
 * Copyright (C) 2010, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Mozilla Corporation. All rights reserved.
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
#include "GraphicsContext3DBase.h"

#if ENABLE(WEBGL)

#include "Extensions3D.h"

namespace WebCore {

GraphicsContext3DBase::GraphicsContext3DBase(GraphicsContext3DAttributes attrs, Destination destination, GraphicsContext3DBase*)
    : m_attrs(attrs)
    , m_destination(destination)
{
}

unsigned GraphicsContext3DBase::getClearBitsByAttachmentType(GC3Denum attachment)
{
    switch (attachment) {
    case GraphicsContext3DBase::COLOR_ATTACHMENT0:
    case Extensions3D::COLOR_ATTACHMENT1_EXT:
    case Extensions3D::COLOR_ATTACHMENT2_EXT:
    case Extensions3D::COLOR_ATTACHMENT3_EXT:
    case Extensions3D::COLOR_ATTACHMENT4_EXT:
    case Extensions3D::COLOR_ATTACHMENT5_EXT:
    case Extensions3D::COLOR_ATTACHMENT6_EXT:
    case Extensions3D::COLOR_ATTACHMENT7_EXT:
    case Extensions3D::COLOR_ATTACHMENT8_EXT:
    case Extensions3D::COLOR_ATTACHMENT9_EXT:
    case Extensions3D::COLOR_ATTACHMENT10_EXT:
    case Extensions3D::COLOR_ATTACHMENT11_EXT:
    case Extensions3D::COLOR_ATTACHMENT12_EXT:
    case Extensions3D::COLOR_ATTACHMENT13_EXT:
    case Extensions3D::COLOR_ATTACHMENT14_EXT:
    case Extensions3D::COLOR_ATTACHMENT15_EXT:
        return GraphicsContext3DBase::COLOR_BUFFER_BIT;
    case GraphicsContext3DBase::DEPTH_ATTACHMENT:
        return GraphicsContext3DBase::DEPTH_BUFFER_BIT;
    case GraphicsContext3DBase::STENCIL_ATTACHMENT:
        return GraphicsContext3DBase::STENCIL_BUFFER_BIT;
    case GraphicsContext3DBase::DEPTH_STENCIL_ATTACHMENT:
        return GraphicsContext3DBase::DEPTH_BUFFER_BIT | GraphicsContext3DBase::STENCIL_BUFFER_BIT;
    default:
        return 0;
    }
}

unsigned GraphicsContext3DBase::getClearBitsByFormat(GC3Denum format)
{
    switch (format) {
    case GraphicsContext3DBase::RGB:
    case GraphicsContext3DBase::RGBA:
    case GraphicsContext3DBase::LUMINANCE_ALPHA:
    case GraphicsContext3DBase::LUMINANCE:
    case GraphicsContext3DBase::ALPHA:
    case GraphicsContext3DBase::R8:
    case GraphicsContext3DBase::R8_SNORM:
    case GraphicsContext3DBase::R16F:
    case GraphicsContext3DBase::R32F:
    case GraphicsContext3DBase::R8UI:
    case GraphicsContext3DBase::R8I:
    case GraphicsContext3DBase::R16UI:
    case GraphicsContext3DBase::R16I:
    case GraphicsContext3DBase::R32UI:
    case GraphicsContext3DBase::R32I:
    case GraphicsContext3DBase::RG8:
    case GraphicsContext3DBase::RG8_SNORM:
    case GraphicsContext3DBase::RG16F:
    case GraphicsContext3DBase::RG32F:
    case GraphicsContext3DBase::RG8UI:
    case GraphicsContext3DBase::RG8I:
    case GraphicsContext3DBase::RG16UI:
    case GraphicsContext3DBase::RG16I:
    case GraphicsContext3DBase::RG32UI:
    case GraphicsContext3DBase::RG32I:
    case GraphicsContext3DBase::RGB8:
    case GraphicsContext3DBase::SRGB8:
    case GraphicsContext3DBase::RGB565:
    case GraphicsContext3DBase::RGB8_SNORM:
    case GraphicsContext3DBase::R11F_G11F_B10F:
    case GraphicsContext3DBase::RGB9_E5:
    case GraphicsContext3DBase::RGB16F:
    case GraphicsContext3DBase::RGB32F:
    case GraphicsContext3DBase::RGB8UI:
    case GraphicsContext3DBase::RGB8I:
    case GraphicsContext3DBase::RGB16UI:
    case GraphicsContext3DBase::RGB16I:
    case GraphicsContext3DBase::RGB32UI:
    case GraphicsContext3DBase::RGB32I:
    case GraphicsContext3DBase::RGBA8:
    case GraphicsContext3DBase::SRGB8_ALPHA8:
    case GraphicsContext3DBase::RGBA8_SNORM:
    case GraphicsContext3DBase::RGB5_A1:
    case GraphicsContext3DBase::RGBA4:
    case GraphicsContext3DBase::RGB10_A2:
    case GraphicsContext3DBase::RGBA16F:
    case GraphicsContext3DBase::RGBA32F:
    case GraphicsContext3DBase::RGBA8UI:
    case GraphicsContext3DBase::RGBA8I:
    case GraphicsContext3DBase::RGB10_A2UI:
    case GraphicsContext3DBase::RGBA16UI:
    case GraphicsContext3DBase::RGBA16I:
    case GraphicsContext3DBase::RGBA32I:
    case GraphicsContext3DBase::RGBA32UI:
    case Extensions3D::SRGB_EXT:
    case Extensions3D::SRGB_ALPHA_EXT:
        return GraphicsContext3DBase::COLOR_BUFFER_BIT;
    case GraphicsContext3DBase::DEPTH_COMPONENT16:
    case GraphicsContext3DBase::DEPTH_COMPONENT24:
    case GraphicsContext3DBase::DEPTH_COMPONENT32F:
    case GraphicsContext3DBase::DEPTH_COMPONENT:
        return GraphicsContext3DBase::DEPTH_BUFFER_BIT;
    case GraphicsContext3DBase::STENCIL_INDEX8:
        return GraphicsContext3DBase::STENCIL_BUFFER_BIT;
    case GraphicsContext3DBase::DEPTH_STENCIL:
    case GraphicsContext3DBase::DEPTH24_STENCIL8:
    case GraphicsContext3DBase::DEPTH32F_STENCIL8:
        return GraphicsContext3DBase::DEPTH_BUFFER_BIT | GraphicsContext3DBase::STENCIL_BUFFER_BIT;
    default:
        return 0;
    }
}

uint8_t GraphicsContext3DBase::getChannelBitsByFormat(GC3Denum format)
{
    switch (format) {
    case GraphicsContext3DBase::ALPHA:
        return static_cast<uint8_t>(ChannelBits::Alpha);
    case GraphicsContext3DBase::LUMINANCE:
        return static_cast<uint8_t>(ChannelBits::RGB);
    case GraphicsContext3DBase::LUMINANCE_ALPHA:
        return static_cast<uint8_t>(ChannelBits::RGBA);
    case GraphicsContext3DBase::RGB:
    case GraphicsContext3DBase::RGB565:
    case Extensions3D::SRGB_EXT:
        return static_cast<uint8_t>(ChannelBits::RGB);
    case GraphicsContext3DBase::RGBA:
    case GraphicsContext3DBase::RGBA4:
    case GraphicsContext3DBase::RGB5_A1:
    case Extensions3D::SRGB_ALPHA_EXT:
        return static_cast<uint8_t>(ChannelBits::RGBA);
    case GraphicsContext3DBase::DEPTH_COMPONENT16:
    case GraphicsContext3DBase::DEPTH_COMPONENT:
        return static_cast<uint8_t>(ChannelBits::Depth);
    case GraphicsContext3DBase::STENCIL_INDEX8:
        return static_cast<uint8_t>(ChannelBits::Stencil);
    case GraphicsContext3DBase::DEPTH_STENCIL:
        return static_cast<uint8_t>(ChannelBits::DepthStencil);
    default:
        return 0;
    }
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
