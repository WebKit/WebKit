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

#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "MainFrame.h"
#include "PageOverlayController.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

MockPageOverlayClient& MockPageOverlayClient::singleton()
{
    static NeverDestroyed<MockPageOverlayClient> sharedClient;
    return sharedClient.get();
}

MockPageOverlayClient::MockPageOverlayClient()
{
}

void MockPageOverlayClient::installOverlay(MainFrame& mainFrame, PageOverlay::OverlayType overlayType)
{
    RefPtr<PageOverlay> overlay = PageOverlay::create(*this, overlayType);
    mainFrame.pageOverlayController().installPageOverlay(overlay, PageOverlay::FadeMode::DoNotFade);
    m_overlays.add(overlay.get());
}

void MockPageOverlayClient::uninstallAllOverlays()
{
    while (!m_overlays.isEmpty()) {
        PageOverlay* overlay = m_overlays.takeAny();
        ASSERT(overlay->controller());
        overlay->controller()->uninstallPageOverlay(overlay, PageOverlay::FadeMode::DoNotFade);
    }
}

String MockPageOverlayClient::layerTreeAsText(MainFrame& mainFrame)
{
    return "View-relative:\n" + mainFrame.pageOverlayController().viewOverlayRootLayer().layerTreeAsText(LayerTreeAsTextIncludePageOverlayLayers) + "\n\nDocument-relative:\n" + mainFrame.pageOverlayController().documentOverlayRootLayer().layerTreeAsText(LayerTreeAsTextIncludePageOverlayLayers);
}

void MockPageOverlayClient::pageOverlayDestroyed(PageOverlay& overlay)
{
    m_overlays.remove(&overlay);
}

void MockPageOverlayClient::willMoveToPage(PageOverlay&, Page*)
{
}

void MockPageOverlayClient::didMoveToPage(PageOverlay& overlay, Page* page)
{
    if (page)
        overlay.setNeedsDisplay();
}

void MockPageOverlayClient::drawRect(PageOverlay& overlay, GraphicsContext& context, const IntRect&)
{
    GraphicsContextStateSaver stateSaver(context);

    FloatRect insetRect = overlay.bounds();

    if (overlay.overlayType() == PageOverlay::OverlayType::Document) {
        context.setStrokeColor(Color(0, 255, 0), ColorSpaceDeviceRGB);
        insetRect.inflate(-50);
    } else {
        context.setStrokeColor(Color(0, 0, 255), ColorSpaceDeviceRGB);
        insetRect.inflate(-20);
    }

    context.strokeRect(insetRect, 20);
}

bool MockPageOverlayClient::mouseEvent(PageOverlay&, const PlatformMouseEvent&)
{
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
