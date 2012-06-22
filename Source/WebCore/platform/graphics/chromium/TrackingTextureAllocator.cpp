/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "TrackingTextureAllocator.h"

#include "Extensions3DChromium.h"
#include "IntRect.h"
#include "LayerRendererChromium.h" // For the GLC() macro

namespace WebCore {

TrackingTextureAllocator::TrackingTextureAllocator(PassRefPtr<GraphicsContext3D> context)
    : m_context(context)
    , m_currentMemoryUseBytes(0)
    , m_textureUsageHint(Any)
    , m_useTextureStorageExt(false)
{
}

TrackingTextureAllocator::~TrackingTextureAllocator()
{
    ASSERT(!m_currentMemoryUseBytes);
}

static GC3Denum textureToStorageFormat(GC3Denum textureFormat)
{
    GC3Denum storageFormat = Extensions3D::RGBA8_OES;
    switch (textureFormat) {
    case GraphicsContext3D::RGBA:
        break;
    case Extensions3D::BGRA_EXT:
        storageFormat = Extensions3DChromium::BGRA8_EXT;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return storageFormat;
}

static bool isTextureFormatSupportedForStorage(GC3Denum format)
{
    return (format == GraphicsContext3D::RGBA || format == Extensions3D::BGRA_EXT);
}

unsigned TrackingTextureAllocator::createTexture(const IntSize& size, GC3Denum format)
{
    m_currentMemoryUseBytes += TextureManager::memoryUseBytes(size, format);

    unsigned textureId = 0;
    GLC(m_context.get(), textureId = m_context->createTexture());
    GLC(m_context.get(), m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId));
    // Do basic linear filtering on resize.
    GLC(m_context.get(), m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
    GLC(m_context.get(), m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));
    // NPOT textures in GL ES only work when the wrap mode is set to GraphicsContext3D::CLAMP_TO_EDGE.
    GLC(m_context.get(), m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE));
    GLC(m_context.get(), m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE));

    if (m_textureUsageHint == FramebufferAttachment)
        GLC(m_context.get(), m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, Extensions3DChromium::GL_TEXTURE_USAGE_ANGLE, Extensions3DChromium::GL_FRAMEBUFFER_ATTACHMENT_ANGLE));
    if (m_useTextureStorageExt && isTextureFormatSupportedForStorage(format)) {
        Extensions3DChromium* extensions = static_cast<Extensions3DChromium*>(m_context->getExtensions());
        GC3Denum storageFormat = textureToStorageFormat(format);
        extensions->texStorage2DEXT(GraphicsContext3D::TEXTURE_2D, 1, storageFormat, size.width(), size.height());
    } else
        GLC(m_context.get(), m_context->texImage2DResourceSafe(GraphicsContext3D::TEXTURE_2D, 0, format, size.width(), size.height(), 0, format, GraphicsContext3D::UNSIGNED_BYTE));
    return textureId;
}

void TrackingTextureAllocator::deleteTexture(unsigned textureId, const IntSize& size, GC3Denum format)
{
    m_currentMemoryUseBytes -= TextureManager::memoryUseBytes(size, format);
    GLC(m_context.get(), m_context->deleteTexture(textureId));
}

}

