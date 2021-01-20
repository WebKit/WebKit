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
#include "MockPageOverlayClient.h"

#include "Document.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "Page.h"
#include "PageOverlayController.h"
#include "PlatformMouseEvent.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

MockPageOverlayClient& MockPageOverlayClient::singleton()
{
    static NeverDestroyed<MockPageOverlayClient> sharedClient;
    return sharedClient.get();
}

MockPageOverlayClient::MockPageOverlayClient() = default;

Ref<MockPageOverlay> MockPageOverlayClient::installOverlay(Page& page, PageOverlay::OverlayType overlayType)
{
    auto overlay = PageOverlay::create(*this, overlayType);
    page.pageOverlayController().installPageOverlay(overlay, PageOverlay::FadeMode::DoNotFade);

    auto mockOverlay = MockPageOverlay::create(overlay.ptr());
    m_overlays.add(mockOverlay.ptr());

    return mockOverlay;
}

void MockPageOverlayClient::uninstallAllOverlays()
{
    while (!m_overlays.isEmpty()) {
        RefPtr<MockPageOverlay> mockOverlay = m_overlays.takeAny();
        PageOverlayController* overlayController = mockOverlay->overlay()->controller();
        ASSERT(overlayController);
        overlayController->uninstallPageOverlay(*mockOverlay->overlay(), PageOverlay::FadeMode::DoNotFade);
    }
}

String MockPageOverlayClient::layerTreeAsText(Page& page, LayerTreeFlags flags)
{
    GraphicsLayer* viewOverlayRoot = page.pageOverlayController().viewOverlayRootLayer();
    GraphicsLayer* documentOverlayRoot = page.pageOverlayController().documentOverlayRootLayer();
    
    return "View-relative:\n" + (viewOverlayRoot ? viewOverlayRoot->layerTreeAsText(flags | LayerTreeAsTextIncludePageOverlayLayers) : "(no view-relative overlay root)")
        + "\n\nDocument-relative:\n" + (documentOverlayRoot ? documentOverlayRoot->layerTreeAsText(flags | LayerTreeAsTextIncludePageOverlayLayers) : "(no document-relative overlay root)");
}

void MockPageOverlayClient::willMoveToPage(PageOverlay&, Page*)
{
}

void MockPageOverlayClient::didMoveToPage(PageOverlay& overlay, Page* page)
{
    if (page)
        overlay.setNeedsDisplay();
}

void MockPageOverlayClient::drawRect(PageOverlay& overlay, GraphicsContext& context, const IntRect& dirtyRect)
{
    StringBuilder message;
    message.appendLiteral("MockPageOverlayClient::drawRect dirtyRect (");
    message.appendNumber(dirtyRect.x());
    message.appendLiteral(", ");
    message.appendNumber(dirtyRect.y());
    message.appendLiteral(", ");
    message.appendNumber(dirtyRect.width());
    message.appendLiteral(", ");
    message.appendNumber(dirtyRect.height());
    message.appendLiteral(")");
    overlay.page()->mainFrame().document()->addConsoleMessage(MessageSource::Other, MessageLevel::Debug, message.toString());

    GraphicsContextStateSaver stateSaver(context);

    FloatRect insetRect = overlay.bounds();

    if (overlay.overlayType() == PageOverlay::OverlayType::Document) {
        context.setStrokeColor(Color::green);
        insetRect.inflate(-50);
    } else {
        context.setStrokeColor(Color::blue);
        insetRect.inflate(-20);
    }

    context.strokeRect(insetRect, 20);
}

bool MockPageOverlayClient::mouseEvent(PageOverlay& overlay, const PlatformMouseEvent& event)
{
    StringBuilder message;
    message.appendLiteral("MockPageOverlayClient::mouseEvent location (");
    message.appendNumber(event.position().x());
    message.appendLiteral(", ");
    message.appendNumber(event.position().y());
    message.appendLiteral(")");
    overlay.page()->mainFrame().document()->addConsoleMessage(MessageSource::Other, MessageLevel::Debug, message.toString());

    return false;
}

void MockPageOverlayClient::didScrollFrame(PageOverlay&, Frame&)
{
}

bool MockPageOverlayClient::copyAccessibilityAttributeStringValueForPoint(PageOverlay&, String /* attribute */, FloatPoint, String&)
{
    return false;
}

bool MockPageOverlayClient::copyAccessibilityAttributeBoolValueForPoint(PageOverlay&, String /* attribute */, FloatPoint, bool&)
{
    return false;
}

Vector<String> MockPageOverlayClient::copyAccessibilityAttributeNames(PageOverlay&, bool /* parameterizedNames */)
{
    return Vector<String>();
}

}
