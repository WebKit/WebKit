/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "PageOverlay.h"

#include "WebPage.h"
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/Page.h>
#include <WebCore/ScrollbarTheme.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<PageOverlay> PageOverlay::create(Client* client)
{
    return adoptRef(new PageOverlay(client));
}

PageOverlay::PageOverlay(Client* client)
    : m_client(client)
    , m_webPage(0)
{
}

PageOverlay::~PageOverlay()
{
}

IntRect PageOverlay::bounds() const
{
    FrameView* frameView = webPage()->corePage()->mainFrame()->view();

    int width = frameView->width();
    int height = frameView->height();

    if (!ScrollbarTheme::nativeTheme()->usesOverlayScrollbars()) {
        if (frameView->verticalScrollbar())
            width -= frameView->verticalScrollbar()->width();
        if (frameView->horizontalScrollbar())
            height -= frameView->horizontalScrollbar()->height();
    }    
    return IntRect(0, 0, width, height);
}

void PageOverlay::setPage(WebPage* webPage)
{
    m_client->willMoveToWebPage(this, webPage);
    m_webPage = webPage;
    m_client->didMoveToWebPage(this, webPage);
}

void PageOverlay::setNeedsDisplay(const IntRect& dirtyRect)
{
    if (m_webPage)
        m_webPage->drawingArea()->setPageOverlayNeedsDisplay(dirtyRect);
}

void PageOverlay::setNeedsDisplay()
{
    setNeedsDisplay(bounds());
}

void PageOverlay::drawRect(GraphicsContext& graphicsContext, const IntRect& dirtyRect)
{
    // If the dirty rect is outside the bounds, ignore it.
    IntRect paintRect = intersection(dirtyRect, bounds());
    if (paintRect.isEmpty())
        return;

    graphicsContext.save();
    graphicsContext.beginTransparencyLayer(1);
    graphicsContext.setCompositeOperation(CompositeCopy);

    m_client->drawRect(this, graphicsContext, paintRect);

    graphicsContext.endTransparencyLayer();
    graphicsContext.restore();
}
    
bool PageOverlay::mouseEvent(const WebMouseEvent& mouseEvent)
{
    // Ignore events outside the bounds.
    if (!bounds().contains(mouseEvent.position()))
        return false;

    return m_client->mouseEvent(this, mouseEvent);
}

} // namespace WebKit
