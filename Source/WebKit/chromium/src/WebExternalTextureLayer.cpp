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
#include <public/WebExternalTextureLayer.h>

#include "CCTextureUpdateQueue.h"
#include "TextureLayerChromium.h"
#include <public/WebExternalTextureLayerClient.h>
#include <public/WebFloatRect.h>
#include <public/WebSize.h>

using namespace WebCore;

namespace WebKit {

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

class WebExternalTextureLayerImpl : public TextureLayerChromiumClient, public TextureLayerChromium {
public:
    explicit WebExternalTextureLayerImpl(WebExternalTextureLayerClient* client)
        : TextureLayerChromium(client ? this : 0)
        , m_client(client)
    {
    }

    virtual unsigned prepareTexture(CCTextureUpdateQueue& queue) OVERRIDE
    {
        WebTextureUpdaterImpl updaterImpl(queue);
        return m_client->prepareTexture(updaterImpl);
    }

    virtual WebKit::WebGraphicsContext3D* context() OVERRIDE
    {
        return m_client->context();
    }

private:
    WebExternalTextureLayerClient* m_client;
};

WebExternalTextureLayer WebExternalTextureLayer::create(WebExternalTextureLayerClient* client)
{
    RefPtr<TextureLayerChromium> layer = adoptRef(new WebExternalTextureLayerImpl(client));
    layer->setIsDrawable(true);
    return WebExternalTextureLayer(layer.release());
}

void WebExternalTextureLayer::clearClient()
{
    unwrap<TextureLayerChromium>()->clearClient();
}

void WebExternalTextureLayer::setTextureId(unsigned id)
{
    unwrap<TextureLayerChromium>()->setTextureId(id);
}

void WebExternalTextureLayer::setFlipped(bool flipped)
{
    unwrap<TextureLayerChromium>()->setFlipped(flipped);
}

void WebExternalTextureLayer::setUVRect(const WebFloatRect& rect)
{
    unwrap<TextureLayerChromium>()->setUVRect(rect);
}

void WebExternalTextureLayer::setOpaque(bool opaque)
{
    unwrap<TextureLayerChromium>()->setOpaque(opaque);
}

void WebExternalTextureLayer::setPremultipliedAlpha(bool premultipliedAlpha)
{
    unwrap<TextureLayerChromium>()->setPremultipliedAlpha(premultipliedAlpha);
}

void WebExternalTextureLayer::willModifyTexture()
{
    unwrap<TextureLayerChromium>()->willModifyTexture();
}

void WebExternalTextureLayer::setRateLimitContext(bool rateLimit)
{
    unwrap<TextureLayerChromium>()->setRateLimitContext(rateLimit);
}

WebExternalTextureLayer::WebExternalTextureLayer(PassRefPtr<TextureLayerChromium> layer)
    : WebLayer(layer)
{
}

} // namespace WebKit
