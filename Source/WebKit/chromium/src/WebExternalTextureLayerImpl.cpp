/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "WebExternalTextureLayerImpl.h"

#include "CCTextureUpdateQueue.h"
#include "TextureLayerChromium.h"
#include "WebLayerImpl.h"
#include <public/WebExternalTextureLayerClient.h>
#include <public/WebFloatRect.h>
#include <public/WebSize.h>

using namespace WebCore;

namespace WebKit {

WebExternalTextureLayer* WebExternalTextureLayer::create(WebExternalTextureLayerClient* client)
{
    return new WebExternalTextureLayerImpl(client);
}

WebExternalTextureLayerImpl::WebExternalTextureLayerImpl(WebExternalTextureLayerClient* client)
    : m_client(client)
{
    RefPtr<TextureLayerChromium> layer;
    if (m_client)
        layer = TextureLayerChromium::create(this);
    else
        layer = TextureLayerChromium::create(0);
    layer->setIsDrawable(true);
    m_layer = adoptPtr(new WebLayerImpl(layer.release()));
}

WebExternalTextureLayerImpl::~WebExternalTextureLayerImpl()
{
    static_cast<TextureLayerChromium*>(m_layer->layer())->clearClient();
}

WebLayer* WebExternalTextureLayerImpl::layer()
{
    return m_layer.get();
}

void WebExternalTextureLayerImpl::setTextureId(unsigned id)
{
    static_cast<TextureLayerChromium*>(m_layer->layer())->setTextureId(id);
}

void WebExternalTextureLayerImpl::setFlipped(bool flipped)
{
    static_cast<TextureLayerChromium*>(m_layer->layer())->setFlipped(flipped);
}

void WebExternalTextureLayerImpl::setUVRect(const WebFloatRect& rect)
{
    static_cast<TextureLayerChromium*>(m_layer->layer())->setUVRect(rect);
}

void WebExternalTextureLayerImpl::setOpaque(bool opaque)
{
    static_cast<TextureLayerChromium*>(m_layer->layer())->setOpaque(opaque);
}

void WebExternalTextureLayerImpl::setPremultipliedAlpha(bool premultipliedAlpha)
{
    static_cast<TextureLayerChromium*>(m_layer->layer())->setPremultipliedAlpha(premultipliedAlpha);
}

void WebExternalTextureLayerImpl::willModifyTexture()
{
    static_cast<TextureLayerChromium*>(m_layer->layer())->willModifyTexture();
}

void WebExternalTextureLayerImpl::setRateLimitContext(bool rateLimit)
{
    static_cast<TextureLayerChromium*>(m_layer->layer())->setRateLimitContext(rateLimit);
}

class WebTextureUpdaterImpl : public WebTextureUpdater {
public:
    explicit WebTextureUpdaterImpl(CCTextureUpdateQueue& queue)
        : m_queue(queue)
    {
    }

    virtual void appendCopy(unsigned sourceTexture, unsigned destinationTexture, WebSize size) OVERRIDE
    {
        TextureCopier::Parameters copy = { sourceTexture, destinationTexture, size };
        m_queue.appendCopy(copy);
    }

private:
    CCTextureUpdateQueue& m_queue;
};

unsigned WebExternalTextureLayerImpl::prepareTexture(CCTextureUpdateQueue& queue)
{
    ASSERT(m_client);
    WebTextureUpdaterImpl updaterImpl(queue);
    return m_client->prepareTexture(updaterImpl);
}

WebGraphicsContext3D* WebExternalTextureLayerImpl::context()
{
    ASSERT(m_client);
    return m_client->context();
}

} // namespace WebKit
