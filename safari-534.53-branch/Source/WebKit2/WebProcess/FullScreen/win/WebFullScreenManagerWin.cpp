/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebFullScreenManagerWin.h"

#if ENABLE(FULLSCREEN_API)

#include "MessageID.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/Page.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<WebFullScreenManager> WebFullScreenManager::create(WebPage* page)
{
    return WebFullScreenManagerWin::create(page);
}

PassRefPtr<WebFullScreenManagerWin> WebFullScreenManagerWin::create(WebPage* page)
{
    return adoptRef(new WebFullScreenManagerWin(page));
}

WebFullScreenManagerWin::WebFullScreenManagerWin(WebPage* page)
    : WebFullScreenManager(page)
{
}

WebFullScreenManagerWin::~WebFullScreenManagerWin()
{
    m_page->send(Messages::WebFullScreenManagerProxy::ExitAcceleratedCompositingMode());
}

void WebFullScreenManagerWin::setRootFullScreenLayer(WebCore::GraphicsLayer* layer)
{
    // Host the full screen layer if its given to us; otherwise it will be disconnected 
    // from the layer heirarchy and cause an ASSERT during resync.
    // FIXME: Disable setting RenderLayer::setAnimating() to make this unnecessary.
    if (m_fullScreenRootLayer == layer)
        return;
    m_fullScreenRootLayer = layer;

    if (!m_fullScreenRootLayer) {
        m_page->send(Messages::WebFullScreenManagerProxy::ExitAcceleratedCompositingMode());
        if (m_rootLayer) {
            m_rootLayer->removeAllChildren();
            m_rootLayer = nullptr;
        }
        return;
    }

    if (!m_rootLayer) {
        m_rootLayer = GraphicsLayer::create(0);
#ifndef NDEBUG
        m_rootLayer->setName("Full screen root layer");
#endif
        m_rootLayer->setDrawsContent(false);
        m_rootLayer->setSize(getFullScreenRect().size());
    }

    m_rootLayer->removeAllChildren();

    if (m_fullScreenRootLayer)
        m_rootLayer->addChild(m_fullScreenRootLayer);

    m_rootLayer->syncCompositingStateForThisLayerOnly();
    m_page->corePage()->mainFrame()->view()->syncCompositingStateIncludingSubframes();
}

void WebFullScreenManagerWin::beginEnterFullScreenAnimation(float)
{
    // FIXME: Add support for animating the content into full screen.
}

void WebFullScreenManagerWin::beginExitFullScreenAnimation(float)
{
    // FIXME: Add support for animating the content into full screen.
}

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
