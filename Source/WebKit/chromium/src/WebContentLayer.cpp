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
#include "platform/WebContentLayer.h"

#include "platform/WebFloatRect.h"
#include "WebContentLayerImpl.h"

namespace WebKit {

WebContentLayer WebContentLayer::create(WebContentLayerClient* contentClient)
{
    return WebContentLayer(WebContentLayerImpl::create(contentClient));
}

WebContentLayer WebContentLayer::create(WebLayerClient*, WebContentLayerClient* contentClient)
{
    return WebContentLayer::create(contentClient);
}

void WebContentLayer::setDrawsContent(bool drawsContent)
{
    unwrap<WebContentLayerImpl>()->setDrawsContent(drawsContent);
}

bool WebContentLayer::drawsContent() const
{
    return constUnwrap<WebContentLayerImpl>()->drawsContent();
}

void WebContentLayer::invalidateRect(const WebFloatRect& dirtyRect)
{
    unwrap<WebContentLayerImpl>()->setNeedsDisplayRect(dirtyRect);
}

void WebContentLayer::invalidate()
{
    unwrap<WebContentLayerImpl>()->setNeedsDisplay();
}

WebContentLayer::WebContentLayer(const PassRefPtr<WebContentLayerImpl>& node)
    : WebLayer(node)
{
}

WebContentLayer& WebContentLayer::operator=(const PassRefPtr<WebContentLayerImpl>& node)
{
    m_private = node;
    return *this;
}

WebContentLayer::operator PassRefPtr<WebContentLayerImpl>() const
{
    return static_cast<WebContentLayerImpl*>(m_private.get());
}

} // namespace WebKit
