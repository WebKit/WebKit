/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "TextureLayerChromium.h"

#include "cc/CCTextureLayerImpl.h"

namespace WebCore {

PassRefPtr<TextureLayerChromium> TextureLayerChromium::create()
{
    return adoptRef(new TextureLayerChromium);
}

TextureLayerChromium::TextureLayerChromium()
    : LayerChromium()
    , m_textureId(0)
    , m_flipped(true)
    , m_uvRect(0, 0, 1, 1)
    , m_ioSurfaceId(0)
{
}

PassOwnPtr<CCLayerImpl> TextureLayerChromium::createCCLayerImpl()
{
    return CCTextureLayerImpl::create(m_layerId);
}

bool TextureLayerChromium::drawsContent() const
{
    return (m_textureId || m_ioSurfaceId) && LayerChromium::drawsContent();
}

void TextureLayerChromium::setTextureId(unsigned id)
{
    m_textureId = id;
    setNeedsCommit();
}

void TextureLayerChromium::setFlipped(bool flipped)
{
    m_flipped = flipped;
    setNeedsCommit();
}

void TextureLayerChromium::setUVRect(const FloatRect& rect)
{
    m_uvRect = rect;
    setNeedsCommit();
}

void TextureLayerChromium::setIOSurfaceProperties(int width, int height, uint32_t ioSurfaceId)
{
    m_ioSurfaceSize = IntSize(width, height);
    m_ioSurfaceId = ioSurfaceId;
    setNeedsCommit();
}

void TextureLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    LayerChromium::pushPropertiesTo(layer);

    CCTextureLayerImpl* textureLayer = static_cast<CCTextureLayerImpl*>(layer);
    textureLayer->setTextureId(m_textureId);
    textureLayer->setFlipped(m_flipped);
    textureLayer->setUVRect(m_uvRect);
    textureLayer->setIOSurfaceProperties(m_ioSurfaceSize, m_ioSurfaceId);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
