/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#if USE(ACCELERATED_COMPOSITING)

#include "LayerTexture.h"

#include "GraphicsContext3D.h"
#include "TextureManager.h"

namespace WebCore {

LayerTexture::LayerTexture(GraphicsContext3D* context, TextureManager* manager)
    : m_context(context)
    , m_textureManager(manager)
    , m_token(0)
    , m_format(0)
    , m_textureId(0)
{
}

LayerTexture::~LayerTexture()
{
    if (m_token)
        m_textureManager->releaseToken(m_token);
}

bool LayerTexture::isValid(const IntSize& size, unsigned format)
{
    return m_token && size == m_size && format == m_format && m_textureManager->hasTexture(m_token);
}

bool LayerTexture::reserve(const IntSize& size, unsigned format)
{
    if (!m_token)
        m_token = m_textureManager->getToken();

    if (size == m_size && format == m_format && m_textureManager->hasTexture(m_token))
        m_textureManager->protectTexture(m_token);
    else {
        m_textureId = m_textureManager->requestTexture(m_token, size, format);
        if (m_textureId) {
            m_size = size;
            m_format = format;
        }
    }

    return m_textureId;
}

void LayerTexture::bindTexture()
{
    ASSERT(m_textureManager->hasTexture(m_token));
    m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_textureId);
}

void LayerTexture::framebufferTexture2D()
{
    m_context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, m_textureId, 0);
}

}

#endif // USE(ACCELERATED_COMPOSITING)

