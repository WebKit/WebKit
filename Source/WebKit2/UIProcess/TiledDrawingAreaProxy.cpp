/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
#include "TiledDrawingAreaProxy.h"

#if USE(TILED_BACKING_STORE)
#include "DrawingAreaMessages.h"
#include "DrawingAreaProxyMessages.h"
#include "MessageID.h"
#include "NotImplemented.h"
#include "WebCoreArgumentCoders.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

using namespace WebCore;

namespace WebKit {

PassOwnPtr<TiledDrawingAreaProxy> TiledDrawingAreaProxy::create(PlatformWebView* webView, WebPageProxy* webPageProxy)
{
    return adoptPtr(new TiledDrawingAreaProxy(webView, webPageProxy));
}

TiledDrawingAreaProxy::TiledDrawingAreaProxy(PlatformWebView* webView, WebPageProxy* webPageProxy)
    : DrawingAreaProxy(DrawingAreaTypeTiled, webPageProxy)
    , m_isWaitingForDidSetFrameNotification(false)
    , m_isVisible(true)
    , m_webView(webView)
{
}

TiledDrawingAreaProxy::~TiledDrawingAreaProxy()
{
}

void TiledDrawingAreaProxy::setVisibleContentRectAndScale(const WebCore::IntRect& visibleContentRect, float scale)
{
    page()->process()->send(Messages::DrawingArea::SetVisibleContentRectAndScale(visibleContentRect, scale), page()->pageID());
}

void TiledDrawingAreaProxy::setVisibleContentRectTrajectoryVector(const WebCore::FloatPoint& trajectoryVector)
{
    page()->process()->send(Messages::DrawingArea::SetVisibleContentRectTrajectoryVector(trajectoryVector), page()->pageID());
}

void TiledDrawingAreaProxy::renderNextFrame()
{
    page()->process()->send(Messages::DrawingArea::RenderNextFrame(), page()->pageID());
}

void TiledDrawingAreaProxy::sizeDidChange()
{
    WebPageProxy* page = this->page();
    if (!page || !page->isValid())
        return;

    if (m_size.isEmpty())
        return;

    if (m_isWaitingForDidSetFrameNotification)
        return;
    m_isWaitingForDidSetFrameNotification = true;

    page->process()->responsivenessTimer()->start();
    page->process()->send(Messages::DrawingArea::SetSize(m_size), page->pageID());
}

void TiledDrawingAreaProxy::deviceScaleFactorDidChange()
{
    notImplemented();
}

void TiledDrawingAreaProxy::setPageIsVisible(bool isVisible)
{
    WebPageProxy* page = this->page();

    if (isVisible == m_isVisible)
        return;

    m_isVisible = isVisible;
    if (!page || !page->isValid())
        return;

    if (!m_isVisible) {
        // Tell the web process that it doesn't need to paint anything for now.
        page->process()->send(Messages::DrawingArea::SuspendPainting(), page->pageID());
        return;
    }

    // The page is now visible.
    page->process()->send(Messages::DrawingArea::ResumePainting(), page->pageID());

    // FIXME: We should request a full repaint here if needed.
}

} // namespace WebKit

#endif
