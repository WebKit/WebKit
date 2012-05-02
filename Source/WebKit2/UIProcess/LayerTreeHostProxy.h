/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef LayerTreeHostProxy_h
#define LayerTreeHostProxy_h

#if USE(UI_SIDE_COMPOSITING)

#include "BackingStore.h"
#include "DrawingAreaProxy.h"
#include "Region.h"
#include "SurfaceUpdateInfo.h"
#include "WebLayerTreeInfo.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <WebCore/RunLoop.h>
#include <WebCore/Timer.h>
#include <wtf/Functional.h>
#include <wtf/HashSet.h>

class QSGNode;

namespace WebKit {

class WebLayerInfo;
class WebLayerTreeRenderer;
class WebLayerUpdateInfo;

class LayerTreeHostProxy {
public:
    LayerTreeHostProxy(DrawingAreaProxy*);
    virtual ~LayerTreeHostProxy();
    void setCompositingLayerState(WebLayerID, const WebLayerInfo&);
    void setCompositingLayerChildren(WebLayerID, const Vector<WebLayerID>&);
#if ENABLE(CSS_FILTERS)
    void setCompositingLayerFilters(WebLayerID, const WebCore::FilterOperations&);
#endif
    void deleteCompositingLayer(WebLayerID);
    void setRootCompositingLayer(WebLayerID);
    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    void purgeGLResources();
    void setContentsSize(const WebCore::FloatSize&);
    void setVisibleContentsRect(const WebCore::IntRect&, float scale, const WebCore::FloatPoint& trajectory, const WebCore::FloatPoint& accurateVisibleContentsPosition);
    void didRenderFrame();
    void createTileForLayer(int layerID, int tileID, const WebCore::IntRect&, const SurfaceUpdateInfo&);
    void updateTileForLayer(int layerID, int tileID, const WebCore::IntRect&, const SurfaceUpdateInfo&);
    void removeTileForLayer(int layerID, int tileID);
    void createDirectlyCompositedImage(int64_t, const WebKit::ShareableBitmap::Handle&);
    void destroyDirectlyCompositedImage(int64_t);
    void didReceiveLayerTreeHostProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    void updateViewport();
    void renderNextFrame();
    void didChangeScrollPosition(const WebCore::IntPoint& position);
    void purgeBackingStores();
    WebLayerTreeRenderer* layerTreeRenderer() const { return m_renderer.get(); }

protected:
    void dispatchUpdate(const Function<void()>&);

    DrawingAreaProxy* m_drawingAreaProxy;
    RefPtr<WebLayerTreeRenderer> m_renderer;
#if USE(GRAPHICS_SURFACE)
    HashMap<uint32_t, RefPtr<ShareableSurface> > m_surfaces;
#endif
};

}

#endif

#endif // LayerTreeHostProxy_h
