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
#include "WebContentLayerImpl.h"

#include "SkMatrix44.h"
#include <public/WebContentLayerClient.h>
#include <public/WebFloatPoint.h>
#include <public/WebFloatRect.h>
#include <public/WebRect.h>
#include <public/WebSize.h>

using namespace WebCore;

namespace WebKit {

WebContentLayer* WebContentLayer::create(WebContentLayerClient* client)
{
    return new WebContentLayerImpl(client);
}

WebContentLayerImpl::WebContentLayerImpl(WebContentLayerClient* client)
    : m_webLayerImpl(adoptPtr(new WebLayerImpl(ContentLayerChromium::create(this))))
    , m_client(client)
{
    m_webLayerImpl->layer()->setIsDrawable(true);
}

WebContentLayerImpl::~WebContentLayerImpl()
{
    static_cast<ContentLayerChromium*>(m_webLayerImpl->layer())->clearDelegate();
}

WebLayer* WebContentLayerImpl::layer()
{
    return m_webLayerImpl.get();
}

void WebContentLayerImpl::setDoubleSided(bool doubleSided)
{
    m_webLayerImpl->layer()->setDoubleSided(doubleSided);
}

void WebContentLayerImpl::setContentsScale(float scale)
{
    m_webLayerImpl->layer()->setContentsScale(scale);
}

void WebContentLayerImpl::setUseLCDText(bool enable)
{
    m_webLayerImpl->layer()->setUseLCDText(enable);
}

void WebContentLayerImpl::setDrawCheckerboardForMissingTiles(bool enable)
{
    m_webLayerImpl->layer()->setDrawCheckerboardForMissingTiles(enable);
}


void WebContentLayerImpl::paintContents(SkCanvas* canvas, const IntRect& clip, FloatRect& opaque)
{
    if (!m_client)
        return;
    WebFloatRect webOpaque;
    m_client->paintContents(canvas, WebRect(clip), webOpaque);
    opaque = webOpaque;
}

} // namespace WebKit
