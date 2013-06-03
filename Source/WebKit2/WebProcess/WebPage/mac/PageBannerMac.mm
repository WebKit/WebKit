/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "PageBanner.h"

#include "WebPage.h"
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsLayer.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<PageBanner> PageBanner::create(CALayer *layer, int height, Client* client)
{
    return adoptRef(new PageBanner(layer, height, client));
}

PageBanner::PageBanner(CALayer *layer, int height, Client* client)
    : m_type(NotSet)
    , m_client(client)
    , m_webPage(0)
    , m_mouseDownInBanner(false)
    , m_layer(layer)
    , m_height(height)
{
}

void PageBanner::addToPage(Type type, WebPage* webPage)
{
    m_type = type;
    m_webPage = webPage;

    ASSERT(m_type != NotSet);
    ASSERT(m_webPage);

    switch (m_type) {
    case Header:
        m_webPage->corePage()->addHeaderWithHeight(m_height);
        break;
    case Footer:
        m_webPage->corePage()->addFooterWithHeight(m_height);
        break;
    case NotSet:
        ASSERT_NOT_REACHED();
    }
}

void PageBanner::didAddParentLayer(GraphicsLayer* parentLayer)
{
    if (!parentLayer)
        return;

    m_layer.get().bounds = CGRectMake(0, 0, parentLayer->size().width(), parentLayer->size().height());
    [parentLayer->platformLayer() addSublayer:m_layer.get()];
}

void PageBanner::detachFromPage()
{
    m_type = NotSet;
    m_webPage = 0;
}

void PageBanner::didChangeDeviceScaleFactor(float scaleFactor)
{
    m_layer.get().contentsScale = scaleFactor;
    [m_layer.get() setNeedsDisplay];
}

bool PageBanner::mouseEvent(const WebMouseEvent& mouseEvent)
{
    FrameView* frameView = m_webPage->mainFrameView();
    if (!frameView)
        return false;

    IntPoint positionInBannerSpace;

    switch (m_type) {
    case Header: {
        positionInBannerSpace = frameView->rootViewToTotalContents(mouseEvent.position());
        break;
    }
    case Footer: {
        positionInBannerSpace = frameView->rootViewToTotalContents(mouseEvent.position()) - IntSize(0, frameView->totalContentsSize().height() - m_height);
        break;
    }
    case NotSet:
        ASSERT_NOT_REACHED();
    }

    if (!m_mouseDownInBanner && (positionInBannerSpace.y() < 0 || positionInBannerSpace.y() > m_height))
        return false;

    if (mouseEvent.type() == WebEvent::MouseDown)
        m_mouseDownInBanner = true;
    else if (mouseEvent.type() == WebEvent::MouseUp)
        m_mouseDownInBanner = false;

    return m_client->mouseEvent(this, mouseEvent.type(), mouseEvent.button(), positionInBannerSpace);
}

CALayer *PageBanner::layer()
{
    return m_layer.get();
}

} // namespace WebKit
