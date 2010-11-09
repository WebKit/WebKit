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

#include "PageOverlay.h"

#include "WebPage.h"

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

void PageOverlay::setPage(WebPage* webPage)
{
    ASSERT(!m_webPage);

    m_webPage = webPage;
    setNeedsDisplay();
}

void PageOverlay::setNeedsDisplay()
{
    m_webPage->drawingArea()->setNeedsDisplay(IntRect(IntPoint(), m_webPage->size()));
}

void PageOverlay::drawRect(GraphicsContext& graphicsContext, const IntRect& dirtyRect)
{
    m_client->drawRect(this, graphicsContext, dirtyRect);
}
    
bool PageOverlay::mouseEvent(const WebMouseEvent& mouseEvent)
{
    return m_client->mouseEvent(this, mouseEvent);
}

} // namespace WebKit
