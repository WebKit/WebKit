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
#include "GraphicsContextGL.h"

#if ENABLE(GRAPHICS_CONTEXT_GL)

#include "ExtensionsGL.h"

namespace WebCore {

GraphicsContextGL::GraphicsContextGL(GraphicsContextGLAttributes attrs, Destination destination, GraphicsContextGL*)
    : m_attrs(attrs)
    , m_destination(destination)
{
}

void GraphicsContextGL::enablePreserveDrawingBuffer()
{
    // Canvas capture should not call this unless necessary.
    ASSERT(!m_attrs.preserveDrawingBuffer);
    m_attrs.preserveDrawingBuffer = true;
}

unsigned GraphicsContextGL::getClearBitsByAttachmentType(GCGLenum attachment)
{
    switch (attachment) {
    case GraphicsContextGL::COLOR_ATTACHMENT0:
    case ExtensionsGL::COLOR_ATTACHMENT1_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT2_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT3_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT4_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT5_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT6_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT7_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT8_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT9_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT10_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT11_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT12_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT13_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT14_EXT:
    case ExtensionsGL::COLOR_ATTACHMENT15_EXT:
        return GraphicsContextGL::COLOR_BUFFER_BIT;
    case GraphicsContextGL::DEPTH_ATTACHMENT:
        return GraphicsContextGL::DEPTH_BUFFER_BIT;
    case GraphicsContextGL::STENCIL_ATTACHMENT:
        return GraphicsContextGL::STENCIL_BUFFER_BIT;
    case GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT:
        return GraphicsContextGL::DEPTH_BUFFER_BIT | GraphicsContextGL::STENCIL_BUFFER_BIT;
    default:
        return 0;
    }
}

unsigned GraphicsContextGL::getClearBitsByFormat(GCGLenum format)
{
    switch (format) {
    case GraphicsContextGL::RGB:
    case GraphicsContextGL::RGBA:
    case GraphicsContextGL::LUMINANCE_ALPHA:
    case GraphicsContextGL::LUMINANCE:
    case GraphicsContextGL::ALPHA:
    case GraphicsContextGL::R8:
    case GraphicsContextGL::R8_SNORM:
    case GraphicsContextGL::R16F:
    case GraphicsContextGL::R32F:
    case GraphicsContextGL::R8UI:
    case GraphicsContextGL::R8I:
    case GraphicsContextGL::R16UI:
    case GraphicsContextGL::R16I:
    case GraphicsContextGL::R32UI:
    case GraphicsContextGL::R32I:
    case GraphicsContextGL::RG8:
    case GraphicsContextGL::RG8_SNORM:
    case GraphicsContextGL::RG16F:
    case GraphicsContextGL::RG32F:
    case GraphicsContextGL::RG8UI:
    case GraphicsContextGL::RG8I:
    case GraphicsContextGL::RG16UI:
    case GraphicsContextGL::RG16I:
    case GraphicsContextGL::RG32UI:
    case GraphicsContextGL::RG32I:
    case GraphicsContextGL::RGB8:
    case GraphicsContextGL::SRGB8:
    case GraphicsContextGL::RGB565:
    case GraphicsContextGL::RGB8_SNORM:
    case GraphicsContextGL::R11F_G11F_B10F:
    case GraphicsContextGL::RGB9_E5:
    case GraphicsContextGL::RGB16F:
    case GraphicsContextGL::RGB32F:
    case GraphicsContextGL::RGB8UI:
    case GraphicsContextGL::RGB8I:
    case GraphicsContextGL::RGB16UI:
    case GraphicsContextGL::RGB16I:
    case GraphicsContextGL::RGB32UI:
    case GraphicsContextGL::RGB32I:
    case GraphicsContextGL::RGBA8:
    case GraphicsContextGL::SRGB8_ALPHA8:
    case GraphicsContextGL::RGBA8_SNORM:
    case GraphicsContextGL::RGB5_A1:
    case GraphicsContextGL::RGBA4:
    case GraphicsContextGL::RGB10_A2:
    case GraphicsContextGL::RGBA16F:
    case GraphicsContextGL::RGBA32F:
    case GraphicsContextGL::RGBA8UI:
    case GraphicsContextGL::RGBA8I:
    case GraphicsContextGL::RGB10_A2UI:
    case GraphicsContextGL::RGBA16UI:
    case GraphicsContextGL::RGBA16I:
    case GraphicsContextGL::RGBA32I:
    case GraphicsContextGL::RGBA32UI:
    case ExtensionsGL::SRGB_EXT:
    case ExtensionsGL::SRGB_ALPHA_EXT:
        return GraphicsContextGL::COLOR_BUFFER_BIT;
    case GraphicsContextGL::DEPTH_COMPONENT16:
    case GraphicsContextGL::DEPTH_COMPONENT24:
    case GraphicsContextGL::DEPTH_COMPONENT32F:
    case GraphicsContextGL::DEPTH_COMPONENT:
        return GraphicsContextGL::DEPTH_BUFFER_BIT;
    case GraphicsContextGL::STENCIL_INDEX8:
        return GraphicsContextGL::STENCIL_BUFFER_BIT;
    case GraphicsContextGL::DEPTH_STENCIL:
    case GraphicsContextGL::DEPTH24_STENCIL8:
    case GraphicsContextGL::DEPTH32F_STENCIL8:
        return GraphicsContextGL::DEPTH_BUFFER_BIT | GraphicsContextGL::STENCIL_BUFFER_BIT;
    default:
        return 0;
    }
}

uint8_t GraphicsContextGL::getChannelBitsByFormat(GCGLenum format)
{
    switch (format) {
    case GraphicsContextGL::ALPHA:
        return static_cast<uint8_t>(ChannelBits::Alpha);
    case GraphicsContextGL::LUMINANCE:
        return static_cast<uint8_t>(ChannelBits::RGB);
    case GraphicsContextGL::LUMINANCE_ALPHA:
        return static_cast<uint8_t>(ChannelBits::RGBA);
    case GraphicsContextGL::RGB:
    case GraphicsContextGL::RGB565:
    case ExtensionsGL::SRGB_EXT:
        return static_cast<uint8_t>(ChannelBits::RGB);
    case GraphicsContextGL::RGBA:
    case GraphicsContextGL::RGBA4:
    case GraphicsContextGL::RGB5_A1:
    case ExtensionsGL::SRGB_ALPHA_EXT:
        return static_cast<uint8_t>(ChannelBits::RGBA);
    case GraphicsContextGL::DEPTH_COMPONENT16:
    case GraphicsContextGL::DEPTH_COMPONENT:
        return static_cast<uint8_t>(ChannelBits::Depth);
    case GraphicsContextGL::STENCIL_INDEX8:
        return static_cast<uint8_t>(ChannelBits::Stencil);
    case GraphicsContextGL::DEPTH_STENCIL:
        return static_cast<uint8_t>(ChannelBits::DepthStencil);
    default:
        return 0;
    }
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
