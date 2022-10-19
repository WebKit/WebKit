/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "WebPageOverlay.h"

#include "WebFrame.h"
#include "WebPage.h"
#include <WebCore/Frame.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/PageOverlay.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {
using namespace WebCore;

static HashMap<PageOverlay*, WebPageOverlay*>& overlayMap()
{
    static NeverDestroyed<HashMap<PageOverlay*, WebPageOverlay*>> map;
    return map;
}

Ref<WebPageOverlay> WebPageOverlay::create(std::unique_ptr<WebPageOverlay::Client> client, PageOverlay::OverlayType overlayType)
{
    return adoptRef(*new WebPageOverlay(WTFMove(client), overlayType));
}

WebPageOverlay::WebPageOverlay(std::unique_ptr<WebPageOverlay::Client> client, PageOverlay::OverlayType overlayType)
    : m_overlay(PageOverlay::create(*this, overlayType))
    , m_client(WTFMove(client))
{
    ASSERT(m_client);
    overlayMap().add(m_overlay.get(), this);
}

WebPageOverlay::~WebPageOverlay()
{
    if (!m_overlay)
        return;

    overlayMap().remove(m_overlay.get());
    m_overlay = nullptr;
}

WebPageOverlay* WebPageOverlay::fromCoreOverlay(PageOverlay& overlay)
{
    return overlayMap().get(&overlay);
}

void WebPageOverlay::setNeedsDisplay(const IntRect& dirtyRect)
{
    m_overlay->setNeedsDisplay(dirtyRect);
}

void WebPageOverlay::setNeedsDisplay()
{
    m_overlay->setNeedsDisplay();
}

void WebPageOverlay::clear()
{
    m_overlay->clear();
}

void WebPageOverlay::willMoveToPage(PageOverlay&, Page* page)
{
    m_client->willMoveToPage(*this, page ? &WebPage::fromCorePage(*page) : nullptr);
}

void WebPageOverlay::didMoveToPage(PageOverlay&, Page* page)
{
    m_client->didMoveToPage(*this, page ? &WebPage::fromCorePage(*page) : nullptr);
}

void WebPageOverlay::drawRect(PageOverlay&, GraphicsContext& context, const IntRect& dirtyRect)
{
    m_client->drawRect(*this, context, dirtyRect);
}

bool WebPageOverlay::mouseEvent(PageOverlay&, const PlatformMouseEvent& event)
{
    return m_client->mouseEvent(*this, event);
}

void WebPageOverlay::didScrollFrame(PageOverlay&, Frame& frame)
{
    m_client->didScrollFrame(*this, WebFrame::fromCoreFrame(frame));
}

#if PLATFORM(MAC)
auto WebPageOverlay::actionContextForResultAtPoint(FloatPoint location) -> std::optional<ActionContext>
{
    return m_client->actionContextForResultAtPoint(*this, location);
}

void WebPageOverlay::dataDetectorsDidPresentUI()
{
    m_client->dataDetectorsDidPresentUI(*this);
}

void WebPageOverlay::dataDetectorsDidChangeUI()
{
    m_client->dataDetectorsDidChangeUI(*this);
}

void WebPageOverlay::dataDetectorsDidHideUI()
{
    m_client->dataDetectorsDidHideUI(*this);
}
#endif // PLATFORM(MAC)

bool WebPageOverlay::copyAccessibilityAttributeStringValueForPoint(PageOverlay&, String attribute, FloatPoint parameter, String& value)
{
    return m_client->copyAccessibilityAttributeStringValueForPoint(*this, attribute, parameter, value);
}

bool WebPageOverlay::copyAccessibilityAttributeBoolValueForPoint(PageOverlay&, String attribute, FloatPoint parameter, bool& value)
{
    return m_client->copyAccessibilityAttributeBoolValueForPoint(*this, attribute, parameter, value);
}

Vector<String> WebPageOverlay::copyAccessibilityAttributeNames(PageOverlay&, bool parameterizedNames)
{
    return m_client->copyAccessibilityAttributeNames(*this, parameterizedNames);
}

} // namespace WebKit
