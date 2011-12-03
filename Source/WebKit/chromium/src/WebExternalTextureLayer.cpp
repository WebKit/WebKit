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
#include "platform/WebExternalTextureLayer.h"

#include "platform/WebFloatRect.h"
#include "WebExternalTextureLayerImpl.h"

namespace WebKit {

WebExternalTextureLayer WebExternalTextureLayer::create(WebLayerClient* client)
{
    return WebExternalTextureLayer(WebExternalTextureLayerImpl::create(client));
}

void WebExternalTextureLayer::setTextureId(unsigned id)
{
    unwrap<WebExternalTextureLayerImpl>()->setTextureId(id);
}

unsigned WebExternalTextureLayer::textureId() const
{
    return constUnwrap<WebExternalTextureLayerImpl>()->textureId();
}

void WebExternalTextureLayer::setFlipped(bool flipped)
{
    unwrap<WebExternalTextureLayerImpl>()->setFlipped(flipped);
}

bool WebExternalTextureLayer::flipped() const
{
    return constUnwrap<WebExternalTextureLayerImpl>()->flipped();
}

void WebExternalTextureLayer::setUVRect(const WebFloatRect& rect)
{
    unwrap<WebExternalTextureLayerImpl>()->setUVRect(rect);
}

WebFloatRect WebExternalTextureLayer::uvRect() const
{
    return WebFloatRect(constUnwrap<WebExternalTextureLayerImpl>()->uvRect());
}

WebExternalTextureLayer::WebExternalTextureLayer(const PassRefPtr<WebExternalTextureLayerImpl>& node)
    : WebLayer(node)
{
}

WebExternalTextureLayer& WebExternalTextureLayer::operator=(const PassRefPtr<WebExternalTextureLayerImpl>& node)
{
    m_private = node;
    return *this;
}

WebExternalTextureLayer::operator PassRefPtr<WebExternalTextureLayerImpl>() const
{
    return static_cast<WebExternalTextureLayerImpl*>(m_private.get());
}

} // namespace WebKit
