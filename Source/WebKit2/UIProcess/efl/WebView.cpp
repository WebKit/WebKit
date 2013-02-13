/*
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebView.h"

#if ENABLE(FULLSCREEN_API)
#include "WebFullScreenManagerProxy.h"
#endif

namespace WebKit {

WebView::WebView(WebContext* context, PageClient* pageClient, WebPageGroup* pageGroup, Evas_Object* evasObject)
    : m_webPageProxy(context->createWebPage(pageClient, pageGroup))
    , m_evasObject(evasObject)
{
    m_webPageProxy->pageGroup()->preferences()->setAcceleratedCompositingEnabled(true);
    m_webPageProxy->pageGroup()->preferences()->setForceCompositingMode(true);

    char* debugVisualsEnvironment = getenv("WEBKIT_SHOW_COMPOSITING_DEBUG_VISUALS");
    bool showDebugVisuals = debugVisualsEnvironment && !strcmp(debugVisualsEnvironment, "1");
    m_webPageProxy->pageGroup()->preferences()->setCompositingBordersVisible(showDebugVisuals);
    m_webPageProxy->pageGroup()->preferences()->setCompositingRepaintCountersVisible(showDebugVisuals);

#if ENABLE(FULLSCREEN_API)
    m_webPageProxy->fullScreenManager()->setWebView(m_evasObject);
#endif
}

WebView::~WebView()
{
    if (m_webPageProxy->isClosed())
        return;

    m_webPageProxy->close();
}

void WebView::initialize()
{
    m_webPageProxy->initializeWebPage();
}

void WebView::setThemePath(WKStringRef theme)
{
    m_webPageProxy->setThemePath(toWTFString(theme).utf8().data());
}

void WebView::setDrawsBackground(bool drawsBackground)
{
    m_webPageProxy->setDrawsBackground(drawsBackground);
}

bool WebView::drawsBackground() const
{
    return m_webPageProxy->drawsBackground();
}

void WebView::setDrawsTransparentBackground(bool transparentBackground)
{
    m_webPageProxy->setDrawsTransparentBackground(transparentBackground);
}

bool WebView::drawsTransparentBackground() const
{
    return m_webPageProxy->drawsTransparentBackground();
}

void WebView::suspendActiveDOMObjectsAndAnimations()
{
    m_webPageProxy->suspendActiveDOMObjectsAndAnimations();
}

void WebView::resumeActiveDOMObjectsAndAnimations()
{
    m_webPageProxy->resumeActiveDOMObjectsAndAnimations();
}

void WebView::initializeClient(const WKViewClient* client)
{
    m_client.initialize(client);
}

void WebView::setViewNeedsDisplay(const WebCore::IntRect& area)
{
    m_client.viewNeedsDisplay(this, area);
}

void WebView::didChangeContentsSize(const WebCore::IntSize& size)
{
    m_client.didChangeContentsSize(this, size);
}

} // namespace WebKit
