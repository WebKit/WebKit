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

#include "Extensions3D.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCTextureLayerImpl.h"

namespace WebCore {

PassRefPtr<TextureLayerChromium> TextureLayerChromium::create(TextureLayerChromiumClient* client)
{
    return adoptRef(new TextureLayerChromium(client));
}

TextureLayerChromium::TextureLayerChromium(TextureLayerChromiumClient* client)
    : LayerChromium()
    , m_client(client)
    , m_flipped(true)
    , m_uvRect(0, 0, 1, 1)
    , m_premultipliedAlpha(true)
    , m_rateLimitContext(false)
    , m_contextLost(false)
    , m_textureId(0)
{
}

TextureLayerChromium::~TextureLayerChromium()
{
    if (m_rateLimitContext && m_client && layerTreeHost())
        layerTreeHost()->stopRateLimiter(m_client->context());
}

PassOwnPtr<CCLayerImpl> TextureLayerChromium::createCCLayerImpl()
{
    return CCTextureLayerImpl::create(m_layerId);
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

void TextureLayerChromium::setPremultipliedAlpha(bool premultipliedAlpha)
{
    m_premultipliedAlpha = premultipliedAlpha;
    setNeedsCommit();
}

void TextureLayerChromium::setRateLimitContext(bool rateLimit)
{
    if (!rateLimit && m_rateLimitContext && m_client && layerTreeHost())
        layerTreeHost()->stopRateLimiter(m_client->context());

    m_rateLimitContext = rateLimit;
}

void TextureLayerChromium::setTextureId(unsigned id)
{
    m_textureId = id;
    setNeedsCommit();
}

void TextureLayerChromium::setNeedsDisplayRect(const FloatRect& dirtyRect)
{
    LayerChromium::setNeedsDisplayRect(dirtyRect);

    if (m_rateLimitContext && m_client && layerTreeHost())
        layerTreeHost()->startRateLimiter(m_client->context());
}

bool TextureLayerChromium::drawsContent() const
{
    return (m_client || m_textureId) && !m_contextLost && LayerChromium::drawsContent();
}

void TextureLayerChromium::update(CCTextureUpdater& updater, const CCOcclusionTracker*)
{
    if (m_client) {
        m_textureId = m_client->prepareTexture(updater);
        m_contextLost = m_client->context()->getExtensions()->getGraphicsResetStatusARB() != GraphicsContext3D::NO_ERROR;
    }

    m_needsDisplay = false;
}

void TextureLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    LayerChromium::pushPropertiesTo(layer);

    CCTextureLayerImpl* textureLayer = static_cast<CCTextureLayerImpl*>(layer);
    textureLayer->setFlipped(m_flipped);
    textureLayer->setUVRect(m_uvRect);
    textureLayer->setPremultipliedAlpha(m_premultipliedAlpha);
    textureLayer->setTextureId(m_textureId);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
