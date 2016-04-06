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

#include "GraphicsLayerClient.h"
#include "PageOverlay.h"
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class Frame;
class MainFrame;
class Page;
class PlatformMouseEvent;

class PageOverlayController final : public GraphicsLayerClient {
    WTF_MAKE_NONCOPYABLE(PageOverlayController);
    WTF_MAKE_FAST_ALLOCATED;
public:
    PageOverlayController(MainFrame&);
    virtual ~PageOverlayController();

    WEBCORE_EXPORT GraphicsLayer& documentOverlayRootLayer();
    WEBCORE_EXPORT GraphicsLayer& viewOverlayRootLayer();

    const Vector<RefPtr<PageOverlay>>& pageOverlays() const { return m_pageOverlays; }

    WEBCORE_EXPORT void installPageOverlay(PassRefPtr<PageOverlay>, PageOverlay::FadeMode);
    WEBCORE_EXPORT void uninstallPageOverlay(PageOverlay*, PageOverlay::FadeMode);

    void setPageOverlayNeedsDisplay(PageOverlay&, const IntRect&);
    void setPageOverlayOpacity(PageOverlay&, float);
    void clearPageOverlay(PageOverlay&);
    GraphicsLayer& layerForOverlay(PageOverlay&) const;

    void willAttachRootLayer();

    void didChangeViewSize();
    void didChangeDocumentSize();
    void didChangeSettings();
    void didChangeDeviceScaleFactor();
    void didChangeViewExposedRect();
    void didScrollFrame(Frame&);

    void didChangeOverlayFrame(PageOverlay&);
    void didChangeOverlayBackgroundColor(PageOverlay&);

    int overlayCount() const { return m_overlayGraphicsLayers.size(); }

    bool handleMouseEvent(const PlatformMouseEvent&);

    WEBCORE_EXPORT bool copyAccessibilityAttributeStringValueForPoint(String attribute, FloatPoint, String& value);
    WEBCORE_EXPORT bool copyAccessibilityAttributeBoolValueForPoint(String attribute, FloatPoint, bool& value);
    WEBCORE_EXPORT Vector<String> copyAccessibilityAttributesNames(bool parameterizedNames);

private:
    void createRootLayersIfNeeded();

    void updateSettingsForLayer(GraphicsLayer&);
    void updateForceSynchronousScrollLayerPositionUpdates();

    // GraphicsLayerClient
    void notifyFlushRequired(const GraphicsLayer*) override;
    void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const FloatRect& clipRect) override;
    float deviceScaleFactor() const override;
    bool shouldSkipLayerInDump(const GraphicsLayer*, LayerTreeAsTextBehavior) const override;

    std::unique_ptr<GraphicsLayer> m_documentOverlayRootLayer;
    std::unique_ptr<GraphicsLayer> m_viewOverlayRootLayer;
    bool m_initialized;

    HashMap<PageOverlay*, std::unique_ptr<GraphicsLayer>> m_overlayGraphicsLayers;
    Vector<RefPtr<PageOverlay>> m_pageOverlays;
    MainFrame& m_mainFrame;
};

} // namespace WebKit

#endif // PageOverlayController_h
