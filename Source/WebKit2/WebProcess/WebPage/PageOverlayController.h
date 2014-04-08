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

#ifndef PageOverlayController_h
#define PageOverlayController_h

#include "PageOverlay.h"
#include "WKBase.h"
#include <WebCore/GraphicsLayerClient.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebKit {

class WebMouseEvent;
class WebPage;

class PageOverlayController : public WebCore::GraphicsLayerClient {
public:
    PageOverlayController(WebPage*);

    void initialize();

    WebCore::GraphicsLayer* rootLayer() const { return m_rootLayer.get(); }

    void installPageOverlay(PassRefPtr<PageOverlay>, PageOverlay::FadeMode);
    void uninstallPageOverlay(PageOverlay*, PageOverlay::FadeMode);

    void setPageOverlayNeedsDisplay(PageOverlay*, const WebCore::IntRect&);
    void setPageOverlayOpacity(PageOverlay*, float);
    void clearPageOverlay(PageOverlay*);

    void didChangeViewSize();
    void didChangePreferences();
    void didChangeDeviceScaleFactor();
    void didChangeExposedRect();
    void didScrollAnyFrame();

    void flushPageOverlayLayers(WebCore::FloatRect);

    bool handleMouseEvent(const WebMouseEvent&);

    // FIXME: We shouldn't use API types here.
    WKTypeRef copyAccessibilityAttributeValue(WKStringRef attribute, WKTypeRef parameter);
    WKArrayRef copyAccessibilityAttributesNames(bool parameterizedNames);

private:
    void updateSettingsForLayer(WebCore::GraphicsLayer*);
    void invalidateAllPageOverlayLayers();

    // WebCore::GraphicsLayerClient
    virtual void notifyAnimationStarted(const WebCore::GraphicsLayer*, double) override { }
    virtual void notifyFlushRequired(const WebCore::GraphicsLayer*) override;
    virtual void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::FloatRect& clipRect) override;
    virtual float deviceScaleFactor() const override;
    virtual void didCommitChangesForLayer(const WebCore::GraphicsLayer*) const override { }

    std::unique_ptr<WebCore::GraphicsLayer> m_rootLayer;
    HashMap<PageOverlay*, std::unique_ptr<WebCore::GraphicsLayer>> m_overlayGraphicsLayers;
    Vector<RefPtr<PageOverlay>> m_pageOverlays;
    WebPage* m_webPage;
};

} // namespace WebKit

#endif // PageOverlayController_h
