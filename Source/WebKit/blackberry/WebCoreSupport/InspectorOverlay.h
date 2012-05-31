/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef InspectorOverlay_h
#define InspectorOverlay_h

#include "WebOverlay.h"
#include <GraphicsLayerClient.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace BlackBerry {
namespace WebKit {
class WebPagePrivate;
}
}

namespace WebCore {
class GraphicsContext;
class GraphicsLayer;


class InspectorOverlay : public WebCore::GraphicsLayerClient {
public:
    class InspectorOverlayClient {
    public:
        virtual void paintInspectorOverlay(GraphicsContext&) = 0;
    };

    static PassOwnPtr<InspectorOverlay> create(BlackBerry::WebKit::WebPagePrivate*, InspectorOverlayClient*);

    ~InspectorOverlay();

    void setClient(InspectorOverlayClient* client) { m_client = client; }

    void clear();
    void update();
    void paintWebFrame(GraphicsContext&);

#if USE(ACCELERATED_COMPOSITING)
    virtual void notifyAnimationStarted(const GraphicsLayer*, double time) { }
    virtual void notifySyncRequired(const GraphicsLayer*);
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& inClip);
    virtual bool showDebugBorders(const GraphicsLayer*) const;
    virtual bool showRepaintCounter(const GraphicsLayer*) const;

    virtual bool contentsVisible(const WebCore::GraphicsLayer*, const WebCore::IntRect& contentRect) const;
#endif

private:
    InspectorOverlay(BlackBerry::WebKit::WebPagePrivate*, InspectorOverlayClient*);
    void invalidateWebFrame();

    BlackBerry::WebKit::WebPagePrivate* m_webPage;
    InspectorOverlayClient* m_client;
    OwnPtr<BlackBerry::WebKit::WebOverlay> m_overlay;
};

} // namespace WebCore


#endif /* InspectorOverlay_h */
